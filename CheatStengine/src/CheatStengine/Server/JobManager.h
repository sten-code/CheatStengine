#pragma once

#include <CheatStengine/Core/ThreadPool.h>

#include <nlohmann/json.hpp>

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>

namespace Server {

    class JobManager {
    public:
        enum class Status {
            Running,
            Completed,
            Failed,
        };

        explicit JobManager(size_t numThreads = 2)
            : m_Pool(numThreads)
        {
        }

        [[nodiscard]] uint64_t Submit(const std::string& label, std::function<nlohmann::json()> work);
        [[nodiscard]] nlohmann::json Describe(uint64_t id) const;
        [[nodiscard]] nlohmann::json DescribeAll() const;
        [[nodiscard]] size_t GetActiveCount() const { return m_ActiveCount; }
        [[nodiscard]] bool HasActiveJobs() const { return m_ActiveCount > 0; }

    private:
        struct Job {
            uint64_t Id;
            std::string Label;
            Status Status = Status::Running;
            nlohmann::json Result;
            std::string Error;
        };

        [[nodiscard]] static const char* StatusToString(Status status);

        ThreadPool m_Pool;

        mutable std::mutex m_Mutex;
        std::unordered_map<uint64_t, Job> m_Jobs;
        uint64_t m_NextId = 1;
        std::atomic<size_t> m_ActiveCount = 0;
    };

}
