#pragma once

#include <AK/Core/Types.hpp>

#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace AK
{
    enum class DiagnosticSeverity : u32
    {
        Trace = 0,
        Info,
        Warning,
        Error,
        Fatal,
        Count
    };

    struct ProfileZoneStats final
    {
        std::string name;
        u64 callCount = 0;
        double totalMilliseconds = 0.0;
        double minMilliseconds = 0.0;
        double maxMilliseconds = 0.0;
        double lastMilliseconds = 0.0;
    };

    struct DiagnosticCounter final
    {
        std::string name;
        i64 value = 0;
    };

    struct DiagnosticEvent final
    {
        u64 sequence = 0;
        DiagnosticSeverity severity = DiagnosticSeverity::Info;
        std::string category;
        std::string message;
    };

    struct DiagnosticsSnapshot final
    {
        u64 frameIndex = 0;
        u64 scopeCount = 0;
        u64 eventCount = 0;
        u64 droppedEvents = 0;
        u64 counterCount = 0;
        double frameMilliseconds = 0.0;
        std::vector<ProfileZoneStats> zones;
        std::vector<DiagnosticCounter> counters;
        std::vector<DiagnosticEvent> recentEvents;
        std::string summary;
    };

    struct DiagnosticsProbeResult final
    {
        bool ok = false;
        DiagnosticsSnapshot snapshot{};
        std::string summary;
    };

    const char* ToString(DiagnosticSeverity severity);
    std::string ToDebugString(const ProfileZoneStats& stats);
    std::string ToDebugString(const DiagnosticsSnapshot& snapshot);

    class DiagnosticsHub final
    {
    public:
        explicit DiagnosticsHub(usize eventCapacity = 128);
        DiagnosticsHub(const DiagnosticsHub&) = delete;
        DiagnosticsHub& operator=(const DiagnosticsHub&) = delete;
        DiagnosticsHub(DiagnosticsHub&&) = delete;
        DiagnosticsHub& operator=(DiagnosticsHub&&) = delete;
        ~DiagnosticsHub() = default;

        void BeginFrame(u64 frameIndex);
        void EndFrame();
        void Reset();

        void RecordZone(const char* name, double milliseconds);
        void SetCounter(const char* name, i64 value);
        void AddCounter(const char* name, i64 delta);
        void AddEvent(DiagnosticSeverity severity, const char* category, const std::string& message);

        [[nodiscard]] DiagnosticsSnapshot Snapshot() const;

    private:
        using Clock = std::chrono::steady_clock;

        ProfileZoneStats& FindOrCreateZoneLocked(const char* name);
        DiagnosticCounter& FindOrCreateCounterLocked(const char* name);
        void PushEventLocked(DiagnosticEvent event);

        mutable std::mutex mMutex;
        std::vector<ProfileZoneStats> mZones;
        std::vector<DiagnosticCounter> mCounters;
        std::vector<DiagnosticEvent> mEvents;
        usize mEventCapacity = 128;
        u64 mFrameIndex = 0;
        u64 mScopeCount = 0;
        u64 mEventCount = 0;
        u64 mDroppedEvents = 0;
        u64 mNextEventSequence = 1;
        double mLastFrameMilliseconds = 0.0;
        Clock::time_point mFrameStart{};
        bool mFrameOpen = false;
    };

    class ProfileScope final
    {
    public:
        ProfileScope(DiagnosticsHub& hub, const char* name);
        ProfileScope(const ProfileScope&) = delete;
        ProfileScope& operator=(const ProfileScope&) = delete;
        ProfileScope(ProfileScope&&) = delete;
        ProfileScope& operator=(ProfileScope&&) = delete;
        ~ProfileScope();

    private:
        using Clock = std::chrono::steady_clock;

        DiagnosticsHub* mHub = nullptr;
        const char* mName = nullptr;
        Clock::time_point mStart{};
    };

    DiagnosticsProbeResult BuildDiagnosticsProbe();
    std::string BuildDiagnosticsProbeSummary();
}
