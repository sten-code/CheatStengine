#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <vector>

class ThreadPool {
public:
    explicit ThreadPool(size_t numThreads)
        : m_Stop(false)
    {
        for (size_t i = 0; i < numThreads; ++i) {
            m_Workers.emplace_back([this] {
                while (true) {
                    std::function<void()> task;

                    {
                        std::unique_lock lock(m_QueueMutex);
                        m_Condition.wait(lock, [this] {
                            return m_Stop || !m_Tasks.empty();
                        });

                        if (m_Stop && m_Tasks.empty()) {
                            return;
                        }

                        task = std::move(m_Tasks.front());
                        m_Tasks.pop();
                    }

                    task();
                }
            });
        }
    }

    ~ThreadPool()
    {
        {
            std::unique_lock lock(m_QueueMutex);
            m_Stop = true;
        }
        m_Condition.notify_all();
        for (std::thread& worker : m_Workers) {
            worker.join();
        }
    }

    template <class F, class... Args>
    std::future<std::result_of_t<F(Args...)>> Enqueue(F&& f, Args&&... args)
    {
        using return_type = std::result_of_t<F(Args...)>;

        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));

        std::future<return_type> res = task->get_future();
        {
            std::unique_lock lock(m_QueueMutex);

            if (m_Stop) {
                throw std::runtime_error("Enqueue on stopped ThreadPool");
            }

            m_Tasks.emplace([task]() { (*task)(); });
        }
        m_Condition.notify_one();
        return res;
    }

    size_t GetThreadCount() const
    {
        return m_Workers.size();
    }

    size_t GetPendingTaskCount() const
    {
        std::unique_lock lock(m_QueueMutex);
        return m_Tasks.size();
    }

private:
    std::vector<std::thread> m_Workers;
    std::queue<std::function<void()>> m_Tasks;

    mutable std::mutex m_QueueMutex;
    std::condition_variable m_Condition;
    std::atomic<bool> m_Stop;
};