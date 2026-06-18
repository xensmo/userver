#include <engine/profiler_execution_plugin.hpp>

#include <engine/task/task_context.hpp>

USERVER_NAMESPACE_BEGIN

namespace engine {

void ProfilerExecutionPlugin::HookTaskStart(impl::TaskContext& task) noexcept { task.ProfilerStartExecution(); }

void ProfilerExecutionPlugin::HookAfterWakeup(impl::TaskContext& task) noexcept { task.ProfilerStartExecution(); }

void ProfilerExecutionPlugin::HookBeforeSleep(impl::TaskContext& task) noexcept { task.ProfilerStopExecution(); }

void ProfilerExecutionPlugin::HookTaskStop(impl::TaskContext& task) noexcept { task.ProfilerStopExecution(); }

}  // namespace engine

USERVER_NAMESPACE_END
