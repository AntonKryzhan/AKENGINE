#include <AK/Settings/ProjectSettings.hpp>

#include <AK/Filesystem/FileSystem.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cctype>
#include <sstream>
#include <string>
#include <unordered_map>

namespace AK
{
    namespace
    {
        std::string Trim(std::string_view text)
        {
            std::size_t begin = 0;
            std::size_t end = text.size();
            while (begin < end && std::isspace(static_cast<unsigned char>(text[begin])) != 0)
            {
                ++begin;
            }
            while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0)
            {
                --end;
            }
            return std::string(text.substr(begin, end - begin));
        }

        std::string ToLowerAscii(std::string text)
        {
            std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c)
            {
                return static_cast<char>(std::tolower(c));
            });
            return text;
        }

        bool ParseBool(std::string_view text, bool fallback)
        {
            const std::string lower = ToLowerAscii(Trim(text));
            if (lower == "1" || lower == "true" || lower == "yes" || lower == "on")
            {
                return true;
            }
            if (lower == "0" || lower == "false" || lower == "no" || lower == "off")
            {
                return false;
            }
            return fallback;
        }

        double ParseDouble(std::string_view text, double fallback)
        {
            const std::string trimmed = Trim(text);
            double value = fallback;
            const char* begin = trimmed.data();
            const char* end = begin + trimmed.size();
            const auto result = std::from_chars(begin, end, value);
            if (result.ec != std::errc{} || result.ptr != end)
            {
                return fallback;
            }
            return value;
        }

        u32 ParseU32(std::string_view text, u32 fallback)
        {
            const std::string trimmed = Trim(text);
            u32 value = fallback;
            const char* begin = trimmed.data();
            const char* end = begin + trimmed.size();
            const auto result = std::from_chars(begin, end, value);
            if (result.ec != std::errc{} || result.ptr != end)
            {
                return fallback;
            }
            return value;
        }

        u64 ParseU64(std::string_view text, u64 fallback)
        {
            const std::string trimmed = Trim(text);
            u64 value = fallback;
            const char* begin = trimmed.data();
            const char* end = begin + trimmed.size();
            const auto result = std::from_chars(begin, end, value);
            if (result.ec != std::errc{} || result.ptr != end)
            {
                return fallback;
            }
            return value;
        }

        void AddWarning(SettingsValidationReport& report, std::string message)
        {
            report.warnings.push_back(std::move(message));
            report.warningCount = static_cast<u32>(report.warnings.size());
        }

        void AddError(SettingsValidationReport& report, std::string message)
        {
            report.valid = false;
            report.errors.push_back(std::move(message));
            report.errorCount = static_cast<u32>(report.errors.size());
        }

        std::string FormatDouble(double value)
        {
            std::ostringstream out;
            out.precision(17);
            out << value;
            return out.str();
        }
    }

    ProjectSettings MakeDefaultProjectSettings()
    {
        return {};
    }

    SettingsValidationReport ValidateProjectSettings(const ProjectSettings& settings)
    {
        SettingsValidationReport report{};

        if (settings.schemaVersion == 0)
        {
            AddError(report, "schemaVersion must be greater than zero");
        }
        if (settings.projectName.empty())
        {
            AddWarning(report, "projectName is empty");
        }
        if (!std::isfinite(settings.units.metersPerUnit) || settings.units.metersPerUnit <= 0.0)
        {
            AddError(report, "metersPerUnit must be finite and positive");
        }
        if (!std::isfinite(settings.units.secondsPerUnit) || settings.units.secondsPerUnit <= 0.0)
        {
            AddError(report, "secondsPerUnit must be finite and positive");
        }
        if (!std::isfinite(settings.units.kilogramsPerUnit) || settings.units.kilogramsPerUnit <= 0.0)
        {
            AddError(report, "kilogramsPerUnit must be finite and positive");
        }
        if (settings.units.metersPerUnit != 1.0)
        {
            AddWarning(report, "non-meter world scale is supported for import, but runtime physics/rendering should stay meter-based");
        }
        if (!settings.coordinates.cameraRelativeRendering)
        {
            AddError(report, "cameraRelativeRendering must stay enabled for large worlds");
        }
        if (!settings.coordinates.reversedZ)
        {
            AddWarning(report, "reversedZ is disabled; large far/near ratios may cause depth fighting");
        }
        if (!std::isfinite(settings.runtime.fixedDeltaSeconds) || settings.runtime.fixedDeltaSeconds <= 0.0)
        {
            AddError(report, "fixedDeltaSeconds must be finite and positive");
        }
        if (!std::isfinite(settings.runtime.maxFrameDeltaSeconds) || settings.runtime.maxFrameDeltaSeconds <= 0.0)
        {
            AddError(report, "maxFrameDeltaSeconds must be finite and positive");
        }
        if (settings.runtime.maxFrameDeltaSeconds < settings.runtime.fixedDeltaSeconds)
        {
            AddWarning(report, "maxFrameDeltaSeconds is smaller than fixedDeltaSeconds; fixed ticks may be dropped too aggressively");
        }
        if (settings.runtime.maxFixedStepsPerFrame == 0)
        {
            AddError(report, "maxFixedStepsPerFrame must be greater than zero");
        }
        if (settings.runtime.maxFramesInFlight == 0 || settings.runtime.maxFramesInFlight > 4)
        {
            AddWarning(report, "maxFramesInFlight should usually be in range 1..4");
        }
        if (settings.runtime.maxResidentStreamingBytes < settings.runtime.maxStreamingBytesPerUpdate)
        {
            AddWarning(report, "maxResidentStreamingBytes is smaller than maxStreamingBytesPerUpdate");
        }

        std::ostringstream summary;
        summary << "Settings validation: " << (report.valid ? "ok" : "failed")
                << " warnings=" << report.warningCount
                << " errors=" << report.errorCount;
        report.summary = summary.str();
        return report;
    }

    std::string SerializeProjectSettingsText(const ProjectSettings& settings)
    {
        std::ostringstream out;
        out << "AKSETTINGS 1\n";
        out << "schemaVersion=" << settings.schemaVersion << "\n";
        out << "projectName=" << settings.projectName << "\n";
        out << "buildProfile=" << ToString(settings.buildProfile) << "\n";
        out << "distanceUnit=" << ToString(settings.units.distance) << "\n";
        out << "angleUnit=" << ToString(settings.units.angle) << "\n";
        out << "massUnit=" << ToString(settings.units.mass) << "\n";
        out << "timeUnit=" << ToString(settings.units.time) << "\n";
        out << "metersPerUnit=" << FormatDouble(settings.units.metersPerUnit) << "\n";
        out << "secondsPerUnit=" << FormatDouble(settings.units.secondsPerUnit) << "\n";
        out << "kilogramsPerUnit=" << FormatDouble(settings.units.kilogramsPerUnit) << "\n";
        out << "handedness=" << ToString(settings.coordinates.handedness) << "\n";
        out << "upAxis=" << ToString(settings.coordinates.upAxis) << "\n";
        out << "matrixConvention=" << ToString(settings.coordinates.matrix) << "\n";
        out << "clipSpace=" << ToString(settings.coordinates.clipSpace) << "\n";
        out << "frontFace=" << ToString(settings.coordinates.frontFace) << "\n";
        out << "cameraRelativeRendering=" << (settings.coordinates.cameraRelativeRendering ? "true" : "false") << "\n";
        out << "reversedZ=" << (settings.coordinates.reversedZ ? "true" : "false") << "\n";
        out << "fixedDeltaSeconds=" << FormatDouble(settings.runtime.fixedDeltaSeconds) << "\n";
        out << "maxFrameDeltaSeconds=" << FormatDouble(settings.runtime.maxFrameDeltaSeconds) << "\n";
        out << "maxFixedStepsPerFrame=" << settings.runtime.maxFixedStepsPerFrame << "\n";
        out << "maxFramesInFlight=" << settings.runtime.maxFramesInFlight << "\n";
        out << "maxStreamingBytesPerUpdate=" << settings.runtime.maxStreamingBytesPerUpdate << "\n";
        out << "maxResidentStreamingBytes=" << settings.runtime.maxResidentStreamingBytes << "\n";
        return out.str();
    }

    Result<ProjectSettings> ParseProjectSettingsText(std::string_view text)
    {
        ProjectSettings settings = MakeDefaultProjectSettings();
        std::unordered_map<std::string, std::string> values;
        std::istringstream input{std::string(text)};
        std::string line;
        bool headerSeen = false;

        while (std::getline(input, line))
        {
            std::string trimmed = Trim(line);
            if (trimmed.empty() || trimmed[0] == '#')
            {
                continue;
            }
            if (!headerSeen)
            {
                if (trimmed != "AKSETTINGS 1")
                {
                    return MakeError(ErrorCode::UnsupportedVersion, "unsupported settings header: " + trimmed);
                }
                headerSeen = true;
                continue;
            }

            const std::size_t equals = trimmed.find('=');
            if (equals == std::string::npos)
            {
                return MakeError(ErrorCode::ParseError, "settings line has no '=': " + trimmed);
            }
            const std::string key = Trim(std::string_view(trimmed).substr(0, equals));
            const std::string value = Trim(std::string_view(trimmed).substr(equals + 1));
            values[key] = value;
        }

        if (!headerSeen)
        {
            return MakeError(ErrorCode::ParseError, "missing AKSETTINGS header");
        }

        auto findValue = [&values](const char* key) -> std::string_view
        {
            const auto it = values.find(key);
            if (it == values.end())
            {
                return {};
            }
            return it->second;
        };

        if (const std::string_view value = findValue("schemaVersion"); !value.empty())
        {
            settings.schemaVersion = ParseU32(value, settings.schemaVersion);
        }
        if (const std::string_view value = findValue("projectName"); !value.empty())
        {
            settings.projectName = std::string(value);
        }
        if (const std::string_view value = findValue("buildProfile"); !value.empty())
        {
            settings.buildProfile = BuildProfileFromString(value);
        }
        if (const std::string_view value = findValue("metersPerUnit"); !value.empty())
        {
            settings.units.metersPerUnit = ParseDouble(value, settings.units.metersPerUnit);
        }
        if (const std::string_view value = findValue("secondsPerUnit"); !value.empty())
        {
            settings.units.secondsPerUnit = ParseDouble(value, settings.units.secondsPerUnit);
        }
        if (const std::string_view value = findValue("kilogramsPerUnit"); !value.empty())
        {
            settings.units.kilogramsPerUnit = ParseDouble(value, settings.units.kilogramsPerUnit);
        }
        if (const std::string_view value = findValue("cameraRelativeRendering"); !value.empty())
        {
            settings.coordinates.cameraRelativeRendering = ParseBool(value, settings.coordinates.cameraRelativeRendering);
        }
        if (const std::string_view value = findValue("reversedZ"); !value.empty())
        {
            settings.coordinates.reversedZ = ParseBool(value, settings.coordinates.reversedZ);
        }
        if (const std::string_view value = findValue("fixedDeltaSeconds"); !value.empty())
        {
            settings.runtime.fixedDeltaSeconds = ParseDouble(value, settings.runtime.fixedDeltaSeconds);
        }
        if (const std::string_view value = findValue("maxFrameDeltaSeconds"); !value.empty())
        {
            settings.runtime.maxFrameDeltaSeconds = ParseDouble(value, settings.runtime.maxFrameDeltaSeconds);
        }
        if (const std::string_view value = findValue("maxFixedStepsPerFrame"); !value.empty())
        {
            settings.runtime.maxFixedStepsPerFrame = ParseU32(value, settings.runtime.maxFixedStepsPerFrame);
        }
        if (const std::string_view value = findValue("maxFramesInFlight"); !value.empty())
        {
            settings.runtime.maxFramesInFlight = ParseU32(value, settings.runtime.maxFramesInFlight);
        }
        if (const std::string_view value = findValue("maxStreamingBytesPerUpdate"); !value.empty())
        {
            settings.runtime.maxStreamingBytesPerUpdate = ParseU64(value, settings.runtime.maxStreamingBytesPerUpdate);
        }
        if (const std::string_view value = findValue("maxResidentStreamingBytes"); !value.empty())
        {
            settings.runtime.maxResidentStreamingBytes = ParseU64(value, settings.runtime.maxResidentStreamingBytes);
        }

        const SettingsValidationReport validation = ValidateProjectSettings(settings);
        if (!validation.valid)
        {
            return MakeError(ErrorCode::InvalidArgument, validation.summary);
        }
        return Ok(settings);
    }

    Result<ProjectSettings> LoadProjectSettings(const std::filesystem::path& path)
    {
        Result<std::string> text = ReadTextFile(path);
        if (!text)
        {
            return text.GetError();
        }
        return ParseProjectSettingsText(text.Value());
    }

    Result<void> SaveProjectSettingsAtomic(const std::filesystem::path& path, const ProjectSettings& settings)
    {
        const SettingsValidationReport validation = ValidateProjectSettings(settings);
        if (!validation.valid)
        {
            return MakeError(ErrorCode::InvalidArgument, validation.summary);
        }
        return WriteTextFileAtomic(path, SerializeProjectSettingsText(settings));
    }

    const char* ToString(BuildProfile value)
    {
        switch (value)
        {
            case BuildProfile::Debug: return "Debug";
            case BuildProfile::Development: return "Development";
            case BuildProfile::Release: return "Release";
            case BuildProfile::Shipping: return "Shipping";
        }
        return "Unknown";
    }

    const char* ToString(DistanceUnit value)
    {
        switch (value)
        {
            case DistanceUnit::Meter: return "Meter";
        }
        return "Unknown";
    }

    const char* ToString(AngleUnit value)
    {
        switch (value)
        {
            case AngleUnit::RadianInternalDegreeUI: return "RadianInternalDegreeUI";
        }
        return "Unknown";
    }

    const char* ToString(MassUnit value)
    {
        switch (value)
        {
            case MassUnit::Kilogram: return "Kilogram";
        }
        return "Unknown";
    }

    const char* ToString(TimeUnit value)
    {
        switch (value)
        {
            case TimeUnit::Second: return "Second";
        }
        return "Unknown";
    }

    const char* ToString(WorldHandedness value)
    {
        switch (value)
        {
            case WorldHandedness::RightHanded: return "RightHanded";
        }
        return "Unknown";
    }

    const char* ToString(WorldUpAxis value)
    {
        switch (value)
        {
            case WorldUpAxis::YUp: return "YUp";
        }
        return "Unknown";
    }

    const char* ToString(MatrixConvention value)
    {
        switch (value)
        {
            case MatrixConvention::ColumnMajor: return "ColumnMajor";
        }
        return "Unknown";
    }

    const char* ToString(ClipSpaceConvention value)
    {
        switch (value)
        {
            case ClipSpaceConvention::VulkanZeroToOne: return "VulkanZeroToOne";
        }
        return "Unknown";
    }

    const char* ToString(FrontFaceWinding value)
    {
        switch (value)
        {
            case FrontFaceWinding::CounterClockwise: return "CounterClockwise";
        }
        return "Unknown";
    }

    BuildProfile BuildProfileFromString(std::string_view text)
    {
        const std::string lower = ToLowerAscii(Trim(text));
        if (lower == "debug")
        {
            return BuildProfile::Debug;
        }
        if (lower == "release")
        {
            return BuildProfile::Release;
        }
        if (lower == "shipping")
        {
            return BuildProfile::Shipping;
        }
        return BuildProfile::Development;
    }

    std::string ToDebugString(const UnitsPolicy& policy)
    {
        std::ostringstream out;
        out << ToString(policy.distance)
            << " scale=" << policy.metersPerUnit
            << " angle=" << ToString(policy.angle)
            << " mass=" << ToString(policy.mass)
            << " time=" << ToString(policy.time);
        return out.str();
    }

    std::string ToDebugString(const CoordinateConvention& convention)
    {
        std::ostringstream out;
        out << ToString(convention.handedness)
            << " " << ToString(convention.upAxis)
            << " matrix=" << ToString(convention.matrix)
            << " clip=" << ToString(convention.clipSpace)
            << " front=" << ToString(convention.frontFace)
            << " cameraRelative=" << (convention.cameraRelativeRendering ? "yes" : "no")
            << " reversedZ=" << (convention.reversedZ ? "yes" : "no");
        return out.str();
    }

    std::string ToDebugString(const RuntimePolicy& policy)
    {
        std::ostringstream out;
        out << "fixedDt=" << policy.fixedDeltaSeconds
            << " maxDt=" << policy.maxFrameDeltaSeconds
            << " maxSteps=" << policy.maxFixedStepsPerFrame
            << " framesInFlight=" << policy.maxFramesInFlight
            << " streamBudget=" << policy.maxStreamingBytesPerUpdate
            << " residentBudget=" << policy.maxResidentStreamingBytes;
        return out.str();
    }

    std::string ToDebugString(const ProjectSettings& settings)
    {
        std::ostringstream out;
        out << settings.projectName
            << " profile=" << ToString(settings.buildProfile)
            << " units=[" << ToDebugString(settings.units) << "]"
            << " coordinates=[" << ToDebugString(settings.coordinates) << "]"
            << " runtime=[" << ToDebugString(settings.runtime) << "]";
        return out.str();
    }

    std::string ToDebugString(const SettingsValidationReport& report)
    {
        return report.summary;
    }

    SettingsProbeResult BuildSettingsProbe()
    {
        SettingsProbeResult result{};
        result.defaults = MakeDefaultProjectSettings();
        result.validation = ValidateProjectSettings(result.defaults);
        result.serialized = SerializeProjectSettingsText(result.defaults);

        Result<ProjectSettings> parsed = ParseProjectSettingsText(result.serialized);
        if (parsed)
        {
            result.roundtrip = parsed.Value();
            result.roundtripValidation = ValidateProjectSettings(result.roundtrip);
        }
        else
        {
            result.roundtripValidation = {};
            result.roundtripValidation.valid = false;
            result.roundtripValidation.errorCount = 1;
            result.roundtripValidation.errors.push_back(parsed.GetError().message);
            result.roundtripValidation.summary = "Settings validation: failed warnings=0 errors=1";
        }

        const bool defaultsOk = result.validation.valid && result.validation.errorCount == 0;
        const bool roundtripOk = result.roundtripValidation.valid
            && result.roundtrip.projectName == result.defaults.projectName
            && result.roundtrip.runtime.maxFramesInFlight == result.defaults.runtime.maxFramesInFlight
            && result.roundtrip.coordinates.cameraRelativeRendering == result.defaults.coordinates.cameraRelativeRendering;
        result.ok = defaultsOk && roundtripOk;

        std::ostringstream summary;
        summary << "Settings probe: " << (result.ok ? "ok" : "failed")
                << " profile=" << ToString(result.defaults.buildProfile)
                << " units=" << ToString(result.defaults.units.distance)
                << " coordinates=" << ToString(result.defaults.coordinates.handedness) << "/" << ToString(result.defaults.coordinates.upAxis)
                << " reversedZ=" << (result.defaults.coordinates.reversedZ ? "yes" : "no")
                << " cameraRelative=" << (result.defaults.coordinates.cameraRelativeRendering ? "yes" : "no")
                << " warnings=" << result.validation.warningCount
                << " errors=" << result.validation.errorCount;
        result.summary = summary.str();
        return result;
    }

    std::string BuildSettingsProbeSummary()
    {
        return BuildSettingsProbe().summary;
    }
}
