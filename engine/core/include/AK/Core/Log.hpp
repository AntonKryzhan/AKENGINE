#pragma once

#include <string_view>

namespace AK
{
    enum class LogLevel
    {
        Trace,
        Info,
        Warning,
        Error
    };

    void Log(LogLevel level, std::string_view message);
    void LogTrace(std::string_view message);
    void LogInfo(std::string_view message);
    void LogWarning(std::string_view message);
    void LogError(std::string_view message);
}
