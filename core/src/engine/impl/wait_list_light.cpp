#include "wait_list_light.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include <fmt/format.h>

#include <concurrent/impl/fast_atomic.hpp>
#include <engine/task/sleep_state.hpp>
#include <userver/engine/impl/awaiter.hpp>
#include <userver/logging/log.hpp>
#include <userver/utils/assert.hpp>
#include <userver/utils/underlying_value.hpp>

USERVER_NAMESPACE_BEGIN

namespace engine::impl {
namespace {

struct alignas(8) AwaiterWithEpoch32 final {
    Awaiter* awaiter{nullptr};
    Epoch epoch{0};
};

struct alignas(16) AwaiterWithEpoch64 final {
    Awaiter* awaiter{nullptr};
    Epoch epoch{0};
    std::uint32_t padding_dont_use{0};
};

// Silence 'unused' warning.
static_assert(sizeof(AwaiterWithEpoch64::padding_dont_use) != 0);

using AwaiterWithEpoch = std::conditional_t<sizeof(void*) == 8, AwaiterWithEpoch64, AwaiterWithEpoch32>;

}  // namespace
}  // namespace engine::impl

USERVER_NAMESPACE_END

template <>
struct fmt::formatter<USERVER_NAMESPACE::engine::impl::AwaiterWithEpoch> {
    static constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(USERVER_NAMESPACE::engine::impl::AwaiterWithEpoch awaiter, FormatContext& ctx) const {
        return fmt::format_to(
            ctx.out(),
            "({}, {})",
            fmt::ptr(awaiter.awaiter),
            USERVER_NAMESPACE::utils::UnderlyingValue(awaiter.epoch)
        );
    }
};

USERVER_NAMESPACE_BEGIN

namespace engine::impl {
namespace {

Awaiter* const kSignaled = reinterpret_cast<Awaiter*>(1);

void DoNotify(AwaiterWithEpoch awaiter) {
    LOG_TRACE() << "NotifyOne awaiter=" << fmt::to_string(awaiter) << " use_count=" << awaiter.awaiter->UseCount();
    awaiter.awaiter->Notify(awaiter.epoch);
}

}  // namespace

struct WaitListLight::Impl final {
    concurrent::impl::FastAtomic<AwaiterWithEpoch> awaiter{AwaiterWithEpoch{}};
};

WaitListLight::WaitListLight() noexcept = default;

WaitListLight::~WaitListLight() {
    UASSERT_MSG(IsEmptyRelaxed(), "Someone is awaiting on WaitListLight while it's being destroyed");
}

void WaitListLight::Append(boost::intrusive_ptr<impl::Awaiter>&& awaiter) noexcept {
    [[maybe_unused]] const bool was_signaled = GetSignalOrAppend(std::move(awaiter));
    UASSERT_MSG(!was_signaled, "Signals cannot be used with plain Append");
}

bool WaitListLight::GetSignalOrAppend(boost::intrusive_ptr<Awaiter> awaiter) noexcept {
    UASSERT(awaiter);

    const AwaiterWithEpoch new_awaiter{awaiter.get(), awaiter->GetEpoch()};
    LOG_TRACE() << "Append awaiter=" << fmt::to_string(new_awaiter) << " use_count=" << awaiter->UseCount();

    AwaiterWithEpoch expected{};
    // seq_cst is important for the "Append-Check-Wakeup" sequence.
    const bool success = impl_->awaiter.compare_exchange_strong<
        std::memory_order_seq_cst,
        std::memory_order_relaxed>(expected, new_awaiter);
    if (!success) {
        UASSERT_MSG(
            expected.awaiter == kSignaled,
            fmt::format(
                "Attempting to wait in a single atomic Awaiter "
                "from multiple coroutines: new={} existing={}",
                new_awaiter,
                expected
            )
        );
        return true;
    }

    // Keep a reference logically stored in the WaitListLight to ensure that
    // WakeupOne can complete safely in parallel with the awaiting task being
    // cancelled, Remove-d and stopped.
    awaiter.detach();

    return false;
}

void WaitListLight::NotifyOne() {
    // seq_cst is important for the "Append-Check-Notify" sequence.
    const auto old_awaiter = impl_->awaiter.exchange<std::memory_order_seq_cst>(AwaiterWithEpoch{});
    if (old_awaiter.awaiter == nullptr) {
        return;
    }

    UASSERT_MSG(old_awaiter.awaiter != kSignaled, "Use SetSignalAndNotifyOne for dealing with signals instead");

    const boost::intrusive_ptr<Awaiter> awaiter{
        old_awaiter.awaiter,
        /*add_ref=*/false
    };
    DoNotify(old_awaiter);
}

void WaitListLight::SetSignalAndNotifyOne() {
    // seq_cst is important for the "Append-Check-Notify" sequence.
    const auto old_awaiter = impl_->awaiter.exchange<std::memory_order_seq_cst>(AwaiterWithEpoch{kSignaled, {}});
    if (old_awaiter.awaiter == nullptr || old_awaiter.awaiter == kSignaled) {
        return;
    }

    const boost::intrusive_ptr<Awaiter> awaiter{
        old_awaiter.awaiter,
        /*add_ref=*/false
    };
    DoNotify(old_awaiter);
}

void WaitListLight::Remove(Awaiter& awaiter) noexcept {
    const AwaiterWithEpoch expected{&awaiter, awaiter.GetEpoch()};

    auto old_awaiter = expected;
    const bool success = impl_->awaiter.compare_exchange_strong<
        std::memory_order_release,
        std::memory_order_relaxed>(old_awaiter, AwaiterWithEpoch{});

    if (!success) {
        UASSERT_MSG(
            old_awaiter.awaiter == nullptr || old_awaiter.awaiter == kSignaled,
            fmt::format(
                "An unexpected awaiter is occupying the "
                "WaitListLight: expected={} actual={}",
                expected,
                old_awaiter
            )
        );
        return;
    }

    LOG_TRACE() << "Remove awaiter=" << fmt::to_string(expected) << " use_count=" << awaiter.UseCount();
    intrusive_ptr_release(&awaiter);
}

bool WaitListLight::GetAndResetSignal() noexcept {
    AwaiterWithEpoch expected{kSignaled, {}};
    const bool success = impl_->awaiter.compare_exchange_strong<
        std::memory_order_relaxed,
        std::memory_order_relaxed>(expected, AwaiterWithEpoch{});

    if (!success && expected.awaiter != nullptr) {
        utils::AbortWithStacktrace(
            fmt::format("ResetSignal with an active awaiter is not allowed: awaiter={}", expected)
        );
    }

    return success;
}

bool WaitListLight::IsSignaled() const noexcept {
    const auto torn_awaiter = impl_->awaiter.LoadWithTearing();
    std::atomic_thread_fence(std::memory_order_acquire);
    return torn_awaiter.awaiter == kSignaled;
}

bool WaitListLight::IsEmptyRelaxed() noexcept {
    const auto awaiter = impl_->awaiter.load<std::memory_order_relaxed>();
    return awaiter.awaiter == nullptr || awaiter.awaiter == kSignaled;
}

}  // namespace engine::impl

USERVER_NAMESPACE_END
