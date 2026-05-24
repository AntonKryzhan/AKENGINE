#pragma once

#include <string>
#include <utility>

namespace AK
{
    enum class ErrorCode
    {
        None = 0,
        InvalidArgument,
        NotFound,
        IoError,
        ParseError,
        UnsupportedVersion,
        InvalidState
    };

    struct Error final
    {
        ErrorCode code = ErrorCode::None;
        std::string message;

        explicit operator bool() const
        {
            return code != ErrorCode::None;
        }
    };

    template <typename T>
    class Result final
    {
    public:
        Result(const T& value)
            : mValue(value)
        {
        }

        Result(T&& value)
            : mValue(std::move(value))
        {
        }

        Result(Error error)
            : mError(std::move(error))
        {
        }

        bool Ok() const
        {
            return mError.code == ErrorCode::None;
        }

        explicit operator bool() const
        {
            return Ok();
        }

        const T& Value() const
        {
            return mValue;
        }

        T& Value()
        {
            return mValue;
        }

        const Error& GetError() const
        {
            return mError;
        }

    private:
        T mValue{};
        Error mError{};
    };

    template <>
    class Result<void> final
    {
    public:
        Result() = default;

        Result(Error error)
            : mError(std::move(error))
        {
        }

        bool Ok() const
        {
            return mError.code == ErrorCode::None;
        }

        explicit operator bool() const
        {
            return Ok();
        }

        const Error& GetError() const
        {
            return mError;
        }

    private:
        Error mError{};
    };

    inline Result<void> Ok()
    {
        return {};
    }

    template <typename T>
    Result<T> Ok(T value)
    {
        return Result<T>(std::move(value));
    }

    inline Error MakeError(ErrorCode code, std::string message)
    {
        return Error{code, std::move(message)};
    }
}
