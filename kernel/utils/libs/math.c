/**
 @copyright (C) 2026 Orizon Software Foundation
 @author Mejd A.
 @license: EUPL 1.2
 */
 

#include "includes/math.h"

uint32_t roundf(float number) {
    return (number >= 0.0f) ? (int)(number + 0.5f) : (int)(number - 0.5f);
}

double fabs(double x) { return x < 0 ? -x : x; }

uint64_t __udivdi3(uint64_t n, uint32_t d) { return n / d; }

uint64_t __umoddi3(uint64_t n, uint32_t d) { return n % d; }
