#include <engine/trace_state_transition_plugin.hpp>

#include <engine/task/task_context.hpp>

USERVER_NAMESPACE_BEGIN

namespace engine {

void TraceStateTransitionPlugin::HookTaskStart(impl::TaskContext& task) noexcept {
    task.TraceStateTransition(Task::State::kRunning);
}

void TraceStateTransitionPlugin::HookAfterWakeup(impl::TaskContext& task) noexcept {
    task.TraceStateTransition(Task::State::kRunning);
}

void TraceStateTransitionPlugin::HookBeforeSleep(impl::TaskContext& task) noexcept {
    task.TraceStateTransition(Task::State::kSuspended);
}

void TraceStateTransitionPlugin::HookTaskStop(impl::TaskContext& task) noexcept {
    task.TraceStateTransition(task.GetPendingFinalState());
}

}  // namespace engine

USERVER_NAMESPACE_END
