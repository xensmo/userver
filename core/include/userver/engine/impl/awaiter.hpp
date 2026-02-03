#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

#include <boost/intrusive/list_hook.hpp>
#include <boost/smart_ptr/intrusive_ref_counter.hpp>

#include <userver/engine/impl/epoch.hpp>

USERVER_NAMESPACE_BEGIN

namespace engine::impl {

class Awaiter {
public:
    enum StaticType : std::uint8_t { kPolymorphic, kTaskContext };

    enum InitialRefCounter : std::size_t { kZero = 0, kOne = 1 };  //  NOLINT(performance-enum-size)

    Awaiter(const Awaiter&) = delete;
    Awaiter(Awaiter&&) = delete;
    Awaiter& operator=(const Awaiter&) = delete;
    Awaiter& operator=(Awaiter&&) = delete;

    void Notify(Epoch epoch);

    void Notify(NoEpoch);

    Epoch GetEpoch() const noexcept;

    std::size_t UseCount() const noexcept;

    StaticType GetStaticType() const noexcept;

protected:
    Awaiter(StaticType type, InitialRefCounter initial_ref_counter);
    ~Awaiter() = default;

private:
    using WaitListHook = typename boost::intrusive::make_list_member_hook<
        boost::intrusive::link_mode<boost::intrusive::auto_unlink>>::type;

    friend class WaitList;

    friend void intrusive_ptr_add_ref(Awaiter* awaiter) noexcept;  // NOLINT(readability-identifier-naming)
    friend void intrusive_ptr_release(Awaiter* awaiter) noexcept;  // NOLINT(readability-identifier-naming)

    // refcounter for resources and memory deallocation
    std::atomic<std::size_t> intrusive_refcount_{0};

    StaticType type_;

    WaitListHook wait_list_hook_;
};

class PolymorphicAwaiter : public Awaiter {
public:
    virtual void DoNotify(Epoch epoch) = 0;

    virtual void DoNotify(NoEpoch) = 0;

    virtual Epoch DoGetEpoch() const noexcept = 0;

protected:
    PolymorphicAwaiter();
    explicit PolymorphicAwaiter(InitialRefCounter initial_ref_counter);

    ~PolymorphicAwaiter() = default;

private:
    friend void intrusive_ptr_release(Awaiter* p) noexcept;  // NOLINT(readability-identifier-naming)

    // Called from intrusive_ptr_release. Should delete the instance
    virtual void Destroy() noexcept = 0;
};

}  // namespace engine::impl

USERVER_NAMESPACE_END
