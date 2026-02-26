#include <userver/engine/impl/awaiter.hpp>

#include <engine/task/task_context.hpp>
#include <userver/utils/assert.hpp>

USERVER_NAMESPACE_BEGIN

namespace engine::impl {

Awaiter::Awaiter(StaticType type, InitialRefCounter initial_ref_counter)
    : intrusive_refcount_(initial_ref_counter),
      type_(type)
{}

void Awaiter::Notify(std::uintptr_t context) noexcept {
    if (type_ == StaticType::kTaskContext) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
        static_cast<TaskContext*>(this)->Wakeup(TaskContext::WakeupSource::kNotify, static_cast<Epoch>(context));
        return;
    }

    CastToPolymorphic().DoNotify(context);
}

std::size_t Awaiter::UseCount() const noexcept {
    // memory order could potentially be less restrictive, but it gets very
    // complicated to reason about
    return intrusive_refcount_.load(std::memory_order_seq_cst);
}

PolymorphicAwaiter& Awaiter::CastToPolymorphic() noexcept {
    UASSERT_MSG(type_ == StaticType::kPolymorphic, "Unexpected awaiter type");
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
    return static_cast<PolymorphicAwaiter&>(*this);
}

void intrusive_ptr_add_ref(Awaiter* awaiter) noexcept {
    UASSERT(awaiter);

    // memory order could potentially be less restrictive, but it gets very
    // complicated to reason about
    awaiter->intrusive_refcount_.fetch_add(1, std::memory_order_seq_cst);
}

void intrusive_ptr_release(Awaiter* awaiter) noexcept {
    UASSERT(awaiter);

    // memory order could potentially be less restrictive, but it gets very
    // complicated to reason about
    if (awaiter->intrusive_refcount_.fetch_sub(1, std::memory_order_seq_cst) != 1) {
        return;
    }

    if (awaiter->type_ == Awaiter::StaticType::kTaskContext) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
        static_cast<TaskContext*>(awaiter)->Destroy();
        return;
    }

    awaiter->CastToPolymorphic().Destroy();
}

PolymorphicAwaiter::PolymorphicAwaiter()
    : PolymorphicAwaiter(InitialRefCounter::kZero)
{}

PolymorphicAwaiter::PolymorphicAwaiter(InitialRefCounter initial_ref_counter)
    : Awaiter(StaticType::kPolymorphic, initial_ref_counter)
{}

}  // namespace engine::impl

USERVER_NAMESPACE_END
