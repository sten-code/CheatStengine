#pragma once

#include <nlohmann/json.hpp>

#include <atomic>
#include <functional>
#include <future>
#include <mutex>
#include <queue>

class CommandQueue {
public:
    using Task = std::function<nlohmann::json()>;

    [[nodiscard]] std::future<nlohmann::json> Push(Task task)
    {
        auto packaged = std::make_shared<std::packaged_task<nlohmann::json()>>(std::move(task));
        std::future<nlohmann::json> future = packaged->get_future();

        {
            std::unique_lock lock(m_Mutex);
            if (m_Stopping) {
                lock.unlock();
                (*packaged) = std::packaged_task<nlohmann::json()>([] {
                    return nlohmann::json { { "error", "server shutting down" }, { "code", 503 } };
                });
                (*packaged)();
                return future;
            }

            m_Tasks.emplace([packaged] { (*packaged)(); });
        }

        return future;
    }

    void Drain(size_t maxTasks = 256)
    {
        for (size_t processed = 0; processed < maxTasks; ++processed) {
            std::function<void()> task;
            {
                std::unique_lock lock(m_Mutex);
                if (m_Tasks.empty()) {
                    return;
                }
                task = std::move(m_Tasks.front());
                m_Tasks.pop();
            }
            task();
        }
    }

    void Shutdown()
    {
        {
            std::unique_lock lock(m_Mutex);
            m_Stopping = true;
        }

        while (true) {
            std::function<void()> task;
            {
                std::unique_lock lock(m_Mutex);
                if (m_Tasks.empty()) {
                    return;
                }
                task = std::move(m_Tasks.front());
                m_Tasks.pop();
            }
            task();
        }
    }

    void Reset()
    {
        std::unique_lock lock(m_Mutex);
        m_Stopping = false;
    }

    [[nodiscard]] size_t GetPendingCount() const
    {
        std::unique_lock lock(m_Mutex);
        return m_Tasks.size();
    }

private:
    std::queue<std::function<void()>> m_Tasks;
    mutable std::mutex m_Mutex;
    std::atomic<bool> m_Stopping = false;
};
