#include <AK/Core/Log.hpp>

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>

namespace AK
{
    namespace
    {
        std::mutex gLogMutex;

        const char* ToString(LogLevel level)
        {
            switch (level)
            {
                case LogLevel::Trace: return "trace";
                case LogLevel::Info: return "info";
                case LogLevel::Warning: return "warning";
                case LogLevel::Error: return "error";
            }
            return "unknown";
        }

        std::string CurrentTimeString()
        {
            const auto now = std::chrono::system_clock::now();
            const auto time = std::chrono::system_clock::to_time_t(now);

            std::tm tm{};
#if defined(_WIN32)
            localtime_s(&tm, &time);
#else
            localtime_r(&time, &tm);
#endif

            std::ostringstream out;
            out << std::put_time(&tm, "%H:%M:%S");
            return out.str();
        }
    }

    void Log(LogLevel level, std::string_view message)
    {
        std::lock_guard<std::mutex> lock(gLogMutex);
        std::cout << "[" << CurrentTimeString() << "][" << ToString(level) << "] " << message << '\n';
    }

    void LogTrace(std::string_view message)
    {
        Log(LogLevel::Trace, message);
    }

    void LogInfo(std::string_view message)
    {
        Log(LogLevel::Info, message);
    }

    void LogWarning(std::string_view message)
    {
        Log(LogLevel::Warning, message);
    }

    void LogError(std::string_view message)
    {
        Log(LogLevel::Error, message);
    }
}
