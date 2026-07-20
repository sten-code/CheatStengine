#include "JobManager.h"

#include <Engine/Core/Core.h>

#include <algorithm>
#include <functional>
#include <vector>

namespace Server {

    uint64_t JobManager::Submit(const std::string& label, std::function<nlohmann::json()> work)
    {
        uint64_t id;
        {
            std::lock_guard lock(m_Mutex);
            id = m_NextId++;
            m_Jobs.emplace(id, Job { id, label });
        }

        ++m_ActiveCount;
        m_Pool.Enqueue([this, id, work = std::move(work)] {
            nlohmann::json result;
            std::string error;
            bool failed = false;

            try {
                result = work();
            } catch (const std::exception& e) {
                failed = true;
                error = e.what();
                ERR("Job {} failed: {}", id, error);
            }

            {
                std::lock_guard lock(m_Mutex);
                auto it = m_Jobs.find(id);
                if (it != m_Jobs.end()) {
                    if (failed) {
                        it->second.Status = Status::Failed;
                        it->second.Error = std::move(error);
                    } else {
                        it->second.Status = Status::Completed;
                        it->second.Result = std::move(result);
                    }
                }
            }

            --m_ActiveCount;
        });

        return id;
    }

    nlohmann::json JobManager::Describe(uint64_t id) const
    {
        std::lock_guard lock(m_Mutex);
        auto it = m_Jobs.find(id);
        if (it == m_Jobs.end()) {
            return nlohmann::json { { "error", "unknown job id" }, { "code", 404 } };
        }

        const Job& job = it->second;
        nlohmann::json out {
            { "id", job.Id },
            { "label", job.Label },
            { "status", StatusToString(job.Status) },
        };

        if (job.Status == Status::Completed) {
            out["result"] = job.Result;
        } else if (job.Status == Status::Failed) {
            out["error"] = job.Error;
        }

        return out;
    }

    nlohmann::json JobManager::DescribeAll() const
    {
        std::lock_guard lock(m_Mutex);

        std::vector<uint64_t> ids;
        ids.reserve(m_Jobs.size());
        for (const auto& [id, job] : m_Jobs) {
            ids.push_back(id);
        }
        std::sort(ids.begin(), ids.end(), std::greater<uint64_t> {});

        nlohmann::json jobs = nlohmann::json::array();
        for (uint64_t id : ids) {
            const Job& job = m_Jobs.at(id);
            jobs.push_back({
                { "id", job.Id },
                { "label", job.Label },
                { "status", StatusToString(job.Status) },
            });
        }
        return jobs;
    }

    const char* JobManager::StatusToString(Status status)
    {
        switch (status) {
            case Status::Running: return "running";
            case Status::Completed: return "completed";
            case Status::Failed: return "failed";
            default: return "unknown";
        }
    }

}
