#include <AK/Core/Log.hpp>
#include <AK/Jobs/JobSystem.hpp>

#include <atomic>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <vector>

int main()
{
    AK::LogInfo("AK job probe starting");

    AK::JobSystemConfig config{};
    config.workerCount = 4;
    config.maxWorkerCount = 4;
    AK::JobSystem jobs(config);

    constexpr AK::u32 itemCount = 4096;
    std::vector<AK::u32> values(itemCount);
    std::iota(values.begin(), values.end(), 1u);

    std::atomic<AK::u64> sum{0};
    jobs.ParallelFor(itemCount, 128, [&values, &sum](AK::u32 begin, AK::u32 end, AK::u32)
    {
        AK::u64 localSum = 0;
        for (AK::u32 i = begin; i < end; ++i)
        {
            localSum += values[i];
        }
        sum.fetch_add(localSum, std::memory_order_relaxed);
    });

    jobs.WaitIdle();
    const AK::u64 expected = (static_cast<AK::u64>(itemCount) * static_cast<AK::u64>(itemCount + 1)) / 2u;
    const AK::JobSystemStats stats = jobs.Stats();

    std::cout << AK::BuildJobSystemProbeSummary() << '\n';
    std::cout << "Job stats: " << AK::ToDebugString(stats) << '\n';
    std::cout << "Parallel sum: " << sum.load(std::memory_order_relaxed) << " expected=" << expected << '\n';

    const bool ok = sum.load(std::memory_order_relaxed) == expected
        && stats.workerCount == 4
        && stats.submittedJobs >= 32
        && stats.completedJobs >= 32
        && stats.pendingJobs == 0
        && stats.activeJobs == 0;

    jobs.Shutdown();

    if (!ok)
    {
        AK::LogError("AK job probe failed");
        return 1;
    }

    AK::LogInfo("AK job probe finished");
    return 0;
}
