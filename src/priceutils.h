#pragma once

#include <cmath>
#include <intrin.h>

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
    __int64 high = 0;
    __int64 low = _mul128(a, b, &high);

    if (high != (low >> 63)) return true;

    out = low;
    return false;
}