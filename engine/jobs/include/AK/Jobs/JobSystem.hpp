#pragma once

#include <AK/Core/Types.hpp>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace AK
{
    struct JobSystemConfig
    {
        u32 workerCount = 0;
        u32 maxWorkerCount = 16;
        bool allowInlineFallback = true;
    };

    struct JobSystemStats
    {
        u32 workerCount = 0;
        u64 submittedJobs = 0;
        u64 completedJobs = 0;
        u64 inlineJobs = 0;
        u64 pendingJobs = 0;
        u32 activeJobs = 0;
        bool running = false;
    };

    using JobFunction = std::function<void(u32 workerIndex)>;
    using ParallelForFunction = std::function<void(u32 begin, u32 end, u32 workerIndex)>;

    class JobSystem
    {
    public:
        JobSystem() = default;
        explicit JobSystem(const JobSystemConfig& config);
        ~JobSystem();

        JobSystem(const JobSystem&) = delete;
        JobSystem& operator=(const JobSystem&) = delete;

        bool Start(const JobSystemConfig& config = {});
        void Shutdown();

        bool IsRunning() const;
        u32 WorkerCount() const;

        void Submit(JobFunction job);
        void ParallelFor(u32 itemCount, u32 groupSize, ParallelForFunction function);
        void WaitIdle();

        JobSystemStats Stats() const;

    private:
        void WorkerMain(u32 workerIndex);
        bool PopJob(JobFunction& outJob);
        void CompleteOneJob();

        mutable std::mutex m_mutex;
        std::condition_variable m_workAvailable;
        std::condition_variable m_idle;
        std::deque<JobFunction> m_queue;
        std::vector<std::thread> m_workers;
        bool m_running = false;
        bool m_stopRequested = false;
        u32 m_activeJobs = 0;
        bool m_allowInlineFallback = true;
        std::atomic<u64> m_submittedJobs{0};
        std::atomic<u64> m_completedJobs{0};
        std::atomic<u64> m_inlineJobs{0};
    };

    std::string ToDebugString(const JobSystemStats& stats);
    std::string BuildJobSystemProbeSummary();
}
