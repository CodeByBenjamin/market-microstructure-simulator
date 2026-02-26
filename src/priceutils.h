#pragma once

#include <cmath>
#include <limits>

#ifdef _MSC_VER
#include <intrin.h>
#endif

#include "datatypes.h"

constexpr double TICK_SIZE = 0.01;

inline PriceTicks toPriceTicks(double price) {
    return static_cast<PriceTicks>(std::llround(price / TICK_SIZE));
}

inline double toPrice(PriceTicks ticks) {
    return ticks * TICK_SIZE;
}

inline bool mul_overflow_i64(PriceTicks a, PriceTicks b, PriceTicks& out)
{
    static_assert(sizeof(PriceTicks) == 8, "mul_overflow_i64 expects 64-bit PriceTicks");
    static_assert(std::numeric_limits<PriceTicks>::is_signed, "mul_overflow_i64 expects signed PriceTicks");

#if defined(_MSC_VER)
    __int64 high = 0;
    __int64 low = _mul128(static_cast<__int64>(a),
        static_cast<__int64>(b),
        &high);

    if (high != (low >> 63)) return true;
    out = static_cast<PriceTicks>(low);
    return false;

#elif defined(__clang__) || defined(__GNUC__)
    return __builtin_mul_overflow(a, b, &out);

#else
# error "Unsupported compiler for mul_overflow_i64"
#endif
}