#include <userver/engine/impl/context_accessor.hpp>

#include <engine/task/task_context.hpp>

USERVER_NAMESPACE_BEGIN

namespace engine::impl {

ContextAccessor::ContextAccessor() = default;

EarlyNotify ContextAccessor::TryAppendAwaiter(TaskContext& task_context) {
    return TryAppendAwaiter(task_context, task_context.GetAwaiterContext());
}

void ContextAccessor::RemoveAwaiter(TaskContext& task_context) noexcept {
    RemoveAwaiter(task_context, task_context.GetAwaiterContext());
}

}  // namespace engine::impl

USERVER_NAMESPACE_END
