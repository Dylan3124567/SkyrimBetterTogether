#include <Services/RunnerService.h>

#include <Events/UpdateEvent.h>

RunnerService::RunnerService(entt::dispatcher& aDispatcher) noexcept
    : m_dispatcher(aDispatcher)
{
}

void RunnerService::Queue(std::function<void()> aFunctor) noexcept
{
    m_runner.Add(std::move(aFunctor));
}

void RunnerService::OnUpdate(const UpdateEvent& acUpdateEvent) noexcept
{
    TiltedPhoques::Vector<std::function<void()>> tasks;
    {
        std::lock_guard lock(m_runner.Lock());
        std::function<void()> task;
        while (m_runner.UnsafePop(task))
            tasks.push_back(std::move(task));
    }

    // Game callbacks may queue more work. Run outside the lock, and defer new tasks to the next frame.
    for (auto& task : tasks)
        task();
}
