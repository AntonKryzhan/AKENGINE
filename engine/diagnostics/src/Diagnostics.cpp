#include <AK/Diagnostics/Diagnostics.hpp>

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <thread>

namespace AK
{
    namespace
    {
        double ToMilliseconds(std::chrono::steady_clock::duration duration)
        {
            return std::chrono::duration<double, std::milli>(duration).count();
        }
    }

    const char* ToString(DiagnosticSeverity severity)
    {
        switch (severity)
        {
            case DiagnosticSeverity::Trace:
                return "Trace";
            case DiagnosticSeverity::Info:
                return "Info";
            case DiagnosticSeverity::Warning:
                return "Warning";
            case DiagnosticSeverity::Error:
                return "Error";
            case DiagnosticSeverity::Fatal:
                return "Fatal";
            case DiagnosticSeverity::Count:
                return "Count";
            default:
                return "Invalid";
        }
    }

    std::string ToDebugString(const ProfileZoneStats& stats)
    {
        const double average = stats.callCount == 0 ? 0.0 : stats.totalMilliseconds / static_cast<double>(stats.callCount);
        std::ostringstream out;
        out << std::fixed << std::setprecision(3)
            << stats.name
            << " calls=" << stats.callCount
            << " last=" << stats.lastMilliseconds << "ms"
            << " avg=" << average << "ms"
            << " max=" << stats.maxMilliseconds << "ms";
        return out.str();
    }

    std::string ToDebugString(const DiagnosticsSnapshot& snapshot)
    {
        std::ostringstream out;
        out << std::fixed << std::setprecision(3)
            << "frame=" << snapshot.frameIndex
            << " frameMs=" << snapshot.frameMilliseconds
            << " zones=" << snapshot.zones.size()
            << " scopes=" << snapshot.scopeCount
            << " counters=" << snapshot.counters.size()
            << " events=" << snapshot.eventCount
            << " dropped=" << snapshot.droppedEvents;
        return out.str();
    }

    DiagnosticsHub::DiagnosticsHub(usize eventCapacity)
        : mEventCapacity(std::max<usize>(1, eventCapacity))
    {
        mFrameStart = Clock::now();
    }

    void DiagnosticsHub::BeginFrame(u64 frameIndex)
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mFrameIndex = frameIndex;
        mFrameStart = Clock::now();
        mFrameOpen = true;
    }

    void DiagnosticsHub::EndFrame()
    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (!mFrameOpen)
        {
            return;
        }

        mLastFrameMilliseconds = ToMilliseconds(Clock::now() - mFrameStart);
        mFrameOpen = false;
    }

    void DiagnosticsHub::Reset()
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mZones.clear();
        mCounters.clear();
        mEvents.clear();
        mFrameIndex = 0;
        mScopeCount = 0;
        mEventCount = 0;
        mDroppedEvents = 0;
        mNextEventSequence = 1;
        mLastFrameMilliseconds = 0.0;
        mFrameStart = Clock::now();
        mFrameOpen = false;
    }

    void DiagnosticsHub::RecordZone(const char* name, double milliseconds)
    {
        if (!name)
        {
            return;
        }

        std::lock_guard<std::mutex> lock(mMutex);
        ProfileZoneStats& stats = FindOrCreateZoneLocked(name);
        ++stats.callCount;
        ++mScopeCount;
        stats.totalMilliseconds += milliseconds;
        stats.lastMilliseconds = milliseconds;
        if (stats.callCount == 1)
        {
            stats.minMilliseconds = milliseconds;
            stats.maxMilliseconds = milliseconds;
        }
        else
        {
            stats.minMilliseconds = std::min(stats.minMilliseconds, milliseconds);
            stats.maxMilliseconds = std::max(stats.maxMilliseconds, milliseconds);
        }
    }

    void DiagnosticsHub::SetCounter(const char* name, i64 value)
    {
        if (!name)
        {
            return;
        }

        std::lock_guard<std::mutex> lock(mMutex);
        FindOrCreateCounterLocked(name).value = value;
    }

    void DiagnosticsHub::AddCounter(const char* name, i64 delta)
    {
        if (!name)
        {
            return;
        }

        std::lock_guard<std::mutex> lock(mMutex);
        FindOrCreateCounterLocked(name).value += delta;
    }

    void DiagnosticsHub::AddEvent(DiagnosticSeverity severity, const char* category, const std::string& message)
    {
        std::lock_guard<std::mutex> lock(mMutex);
        DiagnosticEvent event{};
        event.sequence = mNextEventSequence++;
        event.severity = severity;
        event.category = category ? category : "General";
        event.message = message;
        PushEventLocked(std::move(event));
        ++mEventCount;
    }

    DiagnosticsSnapshot DiagnosticsHub::Snapshot() const
    {
        std::lock_guard<std::mutex> lock(mMutex);
        DiagnosticsSnapshot snapshot{};
        snapshot.frameIndex = mFrameIndex;
        snapshot.scopeCount = mScopeCount;
        snapshot.eventCount = mEventCount;
        snapshot.droppedEvents = mDroppedEvents;
        snapshot.counterCount = static_cast<u64>(mCounters.size());
        snapshot.frameMilliseconds = mFrameOpen ? ToMilliseconds(Clock::now() - mFrameStart) : mLastFrameMilliseconds;
        snapshot.zones = mZones;
        snapshot.counters = mCounters;
        snapshot.recentEvents = mEvents;
        snapshot.summary = ToDebugString(snapshot);

        std::sort(snapshot.zones.begin(), snapshot.zones.end(), [](const ProfileZoneStats& a, const ProfileZoneStats& b)
        {
            return a.maxMilliseconds > b.maxMilliseconds;
        });

        std::sort(snapshot.counters.begin(), snapshot.counters.end(), [](const DiagnosticCounter& a, const DiagnosticCounter& b)
        {
            return a.name < b.name;
        });

        return snapshot;
    }

    ProfileZoneStats& DiagnosticsHub::FindOrCreateZoneLocked(const char* name)
    {
        const auto it = std::find_if(mZones.begin(), mZones.end(), [name](const ProfileZoneStats& stats)
        {
            return stats.name == name;
        });

        if (it != mZones.end())
        {
            return *it;
        }

        mZones.push_back(ProfileZoneStats{name});
        return mZones.back();
    }

    DiagnosticCounter& DiagnosticsHub::FindOrCreateCounterLocked(const char* name)
    {
        const auto it = std::find_if(mCounters.begin(), mCounters.end(), [name](const DiagnosticCounter& counter)
        {
            return counter.name == name;
        });

        if (it != mCounters.end())
        {
            return *it;
        }

        mCounters.push_back(DiagnosticCounter{name, 0});
        return mCounters.back();
    }

    void DiagnosticsHub::PushEventLocked(DiagnosticEvent event)
    {
        if (mEvents.size() >= mEventCapacity)
        {
            mEvents.erase(mEvents.begin());
            ++mDroppedEvents;
        }

        mEvents.push_back(std::move(event));
    }

    ProfileScope::ProfileScope(DiagnosticsHub& hub, const char* name)
        : mHub(&hub)
        , mName(name)
        , mStart(Clock::now())
    {
    }

    ProfileScope::~ProfileScope()
    {
        if (!mHub || !mName)
        {
            return;
        }

        mHub->RecordZone(mName, ToMilliseconds(Clock::now() - mStart));
    }

    DiagnosticsProbeResult BuildDiagnosticsProbe()
    {
        DiagnosticsHub hub(4);
        hub.BeginFrame(42);
        hub.SetCounter("entities", 3);
        hub.AddCounter("entities", 2);
        hub.AddEvent(DiagnosticSeverity::Info, "Diagnostics", "probe started");

        {
            ProfileScope scope(hub, "DiagnosticsProbe.Work");
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        {
            ProfileScope scope(hub, "DiagnosticsProbe.Work");
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        hub.AddEvent(DiagnosticSeverity::Warning, "Diagnostics", "ring buffer checked");
        hub.EndFrame();

        DiagnosticsProbeResult result{};
        result.snapshot = hub.Snapshot();
        const bool hasZone = std::any_of(result.snapshot.zones.begin(), result.snapshot.zones.end(), [](const ProfileZoneStats& stats)
        {
            return stats.name == "DiagnosticsProbe.Work" && stats.callCount == 2 && stats.maxMilliseconds > 0.0;
        });
        const bool hasCounter = std::any_of(result.snapshot.counters.begin(), result.snapshot.counters.end(), [](const DiagnosticCounter& counter)
        {
            return counter.name == "entities" && counter.value == 5;
        });
        result.ok = hasZone && hasCounter && result.snapshot.eventCount == 2 && result.snapshot.frameMilliseconds > 0.0;
        result.summary = std::string("Diagnostics probe: ") + (result.ok ? "ok " : "failed ") + result.snapshot.summary;
        return result;
    }

    std::string BuildDiagnosticsProbeSummary()
    {
        return BuildDiagnosticsProbe().summary;
    }
}
