#pragma once

#include <AK/Core/Result.hpp>
#include <AK/Core/Types.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace AK
{
    enum class BuildProfile
    {
        Debug,
        Development,
        Release,
        Shipping
    };

    enum class DistanceUnit
    {
        Meter
    };

    enum class AngleUnit
    {
        RadianInternalDegreeUI
    };

    enum class MassUnit
    {
        Kilogram
    };

    enum class TimeUnit
    {
        Second
    };

    enum class WorldHandedness
    {
        RightHanded
    };

    enum class WorldUpAxis
    {
        YUp
    };

    enum class MatrixConvention
    {
        ColumnMajor
    };

    enum class ClipSpaceConvention
    {
        VulkanZeroToOne
    };

    enum class FrontFaceWinding
    {
        CounterClockwise
    };

    struct UnitsPolicy final
    {
        DistanceUnit distance = DistanceUnit::Meter;
        AngleUnit angle = AngleUnit::RadianInternalDegreeUI;
        MassUnit mass = MassUnit::Kilogram;
        TimeUnit time = TimeUnit::Second;
        double metersPerUnit = 1.0;
        double secondsPerUnit = 1.0;
        double kilogramsPerUnit = 1.0;
    };

    struct CoordinateConvention final
    {
        WorldHandedness handedness = WorldHandedness::RightHanded;
        WorldUpAxis upAxis = WorldUpAxis::YUp;
        MatrixConvention matrix = MatrixConvention::ColumnMajor;
        ClipSpaceConvention clipSpace = ClipSpaceConvention::VulkanZeroToOne;
        FrontFaceWinding frontFace = FrontFaceWinding::CounterClockwise;
        bool cameraRelativeRendering = true;
        bool reversedZ = true;
    };

    struct RuntimePolicy final
    {
        double fixedDeltaSeconds = 1.0 / 60.0;
        double maxFrameDeltaSeconds = 0.25;
        u32 maxFixedStepsPerFrame = 8;
        u32 maxFramesInFlight = 2;
        u64 maxStreamingBytesPerUpdate = 8ull * 1024ull * 1024ull;
        u64 maxResidentStreamingBytes = 256ull * 1024ull * 1024ull;
    };

    struct ProjectSettings final
    {
        u32 schemaVersion = 1;
        std::string projectName = "AK Sandbox";
        BuildProfile buildProfile = BuildProfile::Development;
        UnitsPolicy units{};
        CoordinateConvention coordinates{};
        RuntimePolicy runtime{};
    };

    struct SettingsValidationReport final
    {
        bool valid = true;
        u32 warningCount = 0;
        u32 errorCount = 0;
        std::vector<std::string> warnings;
        std::vector<std::string> errors;
        std::string summary;
    };

    struct SettingsProbeResult final
    {
        bool ok = false;
        ProjectSettings defaults{};
        SettingsValidationReport validation{};
        std::string serialized;
        ProjectSettings roundtrip{};
        SettingsValidationReport roundtripValidation{};
        std::string summary;
    };

    ProjectSettings MakeDefaultProjectSettings();
    SettingsValidationReport ValidateProjectSettings(const ProjectSettings& settings);

    std::string SerializeProjectSettingsText(const ProjectSettings& settings);
    Result<ProjectSettings> ParseProjectSettingsText(std::string_view text);
    Result<ProjectSettings> LoadProjectSettings(const std::filesystem::path& path);
    Result<void> SaveProjectSettingsAtomic(const std::filesystem::path& path, const ProjectSettings& settings);

    const char* ToString(BuildProfile value);
    const char* ToString(DistanceUnit value);
    const char* ToString(AngleUnit value);
    const char* ToString(MassUnit value);
    const char* ToString(TimeUnit value);
    const char* ToString(WorldHandedness value);
    const char* ToString(WorldUpAxis value);
    const char* ToString(MatrixConvention value);
    const char* ToString(ClipSpaceConvention value);
    const char* ToString(FrontFaceWinding value);

    BuildProfile BuildProfileFromString(std::string_view text);

    std::string ToDebugString(const UnitsPolicy& policy);
    std::string ToDebugString(const CoordinateConvention& convention);
    std::string ToDebugString(const RuntimePolicy& policy);
    std::string ToDebugString(const ProjectSettings& settings);
    std::string ToDebugString(const SettingsValidationReport& report);

    SettingsProbeResult BuildSettingsProbe();
    std::string BuildSettingsProbeSummary();
}
