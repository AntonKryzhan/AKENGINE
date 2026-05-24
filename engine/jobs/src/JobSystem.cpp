#include <AK/Jobs/JobSystem.hpp>

#include <AK/Core/Log.hpp>

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <numeric>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace AK
{
    namespace
    {
        u32 ResolveWorkerCount(JobSystemConfig config)
        {
            const u32 hardwareThreads = std::max<u32>(1, std::thread::hardware_concurrency());
            u32 requested = config.workerCount;
            if (requested == 0)
            {
                requested = hardwareThreads > 1 ? hardwareThreads - 1 : 1;
            }

            const u32 maxWorkers = std::max<u32>(1, config.maxWorkerCount);
            return std::clamp(requested, 1u, maxWorkers);
        }
    }

    JobSystem::JobSystem(const JobSystemConfig& config)
    {
        Start(config);
    }

    JobSystem::~JobSystem()
    {
        Shutdown();
    }

    bool JobSystem::Start(const JobSystemConfig& config)
    {
        Shutdown();

        const u32 workerCount = ResolveWorkerCount(config);
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_running = true;
            m_stopRequested = false;
            m_activeJobs = 0;
            m_allowInlineFallback = config.allowInlineFallback;
            m_queue.clear();
        }

        m_workers.reserve(workerCount);
        try
        {
            for (u32 workerIndex = 0; workerIndex < workerCount; ++workerIndex)
            {
                m_workers.emplace_back([this, workerIndex]()
                {
                    WorkerMain(workerIndex);
                });
            }
        }
        catch (...)
        {
            Shutdown();
            LogError("JobSystem failed to start worker threads");
            return false;
        }

        return true;
    }

    void JobSystem::Shutdown()
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_running && m_workers.empty())
            {
                return;
            }

            m_stopRequested = true;
            m_running = false;
        }

        m_workAvailable.notify_all();

        for (std::thread& worker : m_workers)
        {
            if (worker.joinable())
            {
                worker.join();
            }
        }

        m_workers.clear();

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_queue.clear();
            m_activeJobs = 0;
            m_stopRequested = false;
        }

        m_idle.notify_all();
    }

    bool JobSystem::IsRunning() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_running;
    }

    u32 JobSystem::WorkerCount() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return static_cast<u32>(m_workers.size());
    }

    void JobSystem::Submit(JobFunction job)
    {
        if (!job)
        {
            return;
        }

        bool runInline = false;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_running && !m_stopRequested)
            {
                m_queue.push_back(std::move(job));
                m_submittedJobs.fetch_add(1, std::memory_order_relaxed);
            }
            else
            {
                runInline = m_allowInlineFallback;
            }
        }

        if (runInline)
        {
            m_inlineJobs.fetch_add(1, std::memory_order_relaxed);
            m_submittedJobs.fetch_add(1, std::memory_order_relaxed);
            job(0);
            m_completedJobs.fetch_add(1, std::memory_order_relaxed);
            m_idle.notify_all();
            return;
        }

        m_workAvailable.notify_one();
    }

    void JobSystem::ParallelFor(u32 itemCount, u32 groupSize, ParallelForFunction function)
    {
        if (itemCount == 0 || !function)
        {
            return;
        }

        const u32 safeGroupSize = std::max<u32>(1, groupSize);
        const u32 groupCount = (itemCount + safeGroupSize - 1) / safeGroupSize;
        auto remainingGroups = std::make_shared<std::atomic<u32>>(groupCount);
        auto waitMutex = std::make_shared<std::mutex>();
        auto waitCv = std::make_shared<std::condition_variable>();

        for (u32 groupIndex = 0; groupIndex < groupCount; ++groupIndex)
        {
            const u32 begin = groupIndex * safeGroupSize;
            const u32 end = std::min(itemCount, begin + safeGroupSize);
            Submit([begin, end, function, remainingGroups, waitMutex, waitCv](u32 workerIndex)
            {
                function(begin, end, workerIndex);
                if (remainingGroups->fetch_sub(1, std::memory_order_acq_rel) == 1)
                {
                    std::lock_guard<std::mutex> lock(*waitMutex);
                    waitCv->notify_one();
                }
            });
        }

        std::unique_lock<std::mutex> lock(*waitMutex);
        waitCv->wait(lock, [remainingGroups]()
        {
            return remainingGroups->load(std::memory_order_acquire) == 0;
        });
    }

    void JobSystem::WaitIdle()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_idle.wait(lock, [this]()
        {
            return m_queue.empty() && m_activeJobs == 0;
        });
    }

    JobSystemStats JobSystem::Stats() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        JobSystemStats stats{};
        stats.workerCount = static_cast<u32>(m_workers.size());
        stats.submittedJobs = m_submittedJobs.load(std::memory_order_relaxed);
        stats.completedJobs = m_completedJobs.load(std::memory_order_relaxed);
        stats.inlineJobs = m_inlineJobs.load(std::memory_order_relaxed);
        stats.pendingJobs = static_cast<u64>(m_queue.size());
        stats.activeJobs = m_activeJobs;
        stats.running = m_running;
        return stats;
    }

    void JobSystem::WorkerMain(u32 workerIndex)
    {
        for (;;)
        {
            JobFunction job;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_workAvailable.wait(lock, [this]()
                {
                    return m_stopRequested || !m_queue.empty();
                });

                if (m_stopRequested && m_queue.empty())
                {
                    return;
                }

                job = std::move(m_queue.front());
                m_queue.pop_front();
                ++m_activeJobs;
            }

            job(workerIndex);
            CompleteOneJob();
        }
    }

    bool JobSystem::PopJob(JobFunction& outJob)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_queue.empty())
        {
            return false;
        }

        outJob = std::move(m_queue.front());
        m_queue.pop_front();
        ++m_activeJobs;
        return true;
    }

    void JobSystem::CompleteOneJob()
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_activeJobs > 0)
            {
                --m_activeJobs;
            }
            m_completedJobs.fetch_add(1, std::memory_order_relaxed);
        }

        m_idle.notify_all();
    }

    std::string ToDebugString(const JobSystemStats& stats)
    {
        std::ostringstream out;
        out << "workers=" << stats.workerCount
            << " submitted=" << stats.submittedJobs
            << " completed=" << stats.completedJobs
            << " pending=" << stats.pendingJobs
            << " active=" << stats.activeJobs
            << " inline=" << stats.inlineJobs
            << " running=" << (stats.running ? "yes" : "no");
        return out.str();
    }

    std::string BuildJobSystemProbeSummary()
    {
        JobSystemConfig config{};
        config.workerCount = 2;
        config.maxWorkerCount = 2;
        JobSystem jobs(config);

        constexpr u32 itemCount = 1024;
        std::vector<u32> values(itemCount);
        std::iota(values.begin(), values.end(), 1u);

        std::atomic<u64> sum{0};
        jobs.ParallelFor(itemCount, 64, [&values, &sum](u32 begin, u32 end, u32)
        {
            u64 localSum = 0;
            for (u32 i = begin; i < end; ++i)
            {
                localSum += values[i];
            }
            sum.fetch_add(localSum, std::memory_order_relaxed);
        });

        jobs.WaitIdle();
        const u64 expected = (static_cast<u64>(itemCount) * static_cast<u64>(itemCount + 1)) / 2u;
        const bool ok = sum.load(std::memory_order_relaxed) == expected;
        const JobSystemStats stats = jobs.Stats();

        std::ostringstream out;
        out << "Job probe: " << (ok ? "ok" : "failed")
            << " sum=" << sum.load(std::memory_order_relaxed)
            << " expected=" << expected
            << " " << ToDebugString(stats);
        return out.str();
    }
}
