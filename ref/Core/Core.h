#pragma once

#include <spdlog/spdlog.h>

#include <limits>

namespace ref
{

namespace logger = spdlog;

template<typename T, typename P, T Sentinel = std::numeric_limits<T>::max()> struct IdType
{
    IdType() = default;
    IdType(T value) : Value(value)
    {
        assert(value != Sentinel);
    }

    operator T() const
    {
        assert(Value != Sentinel);
        return Value;
    }

    T Value = Sentinel;
};

}