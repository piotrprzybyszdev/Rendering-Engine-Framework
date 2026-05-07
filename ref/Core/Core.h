#pragma once

#include <spdlog/spdlog.h>

#include <limits>

namespace ref
{

namespace logger = spdlog;

template<typename T, typename P, T Sentinel = std::numeric_limits<T>::max()> struct IdType
{
    IdType() = default;

    explicit IdType(T value) : Value(value)
    {
        assert(value != Sentinel);
    }

    operator T() const
    {
        assert(Value != Sentinel);
        return Value;
    }

    bool IsValid() const
    {
        return Value != Sentinel;
    }

    T Value = Sentinel;
};

class error : public std::runtime_error
{
public:
    using runtime_error::runtime_error;
};

class initialization_error : public error
{
public:
    using error::error;
};

class configuration_error : public error
{
public:
    using error::error;
};

}