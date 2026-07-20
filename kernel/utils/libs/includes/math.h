/**
 @copyright (C) 2026 Orizon Software Foundation
 @author Mejd A.
 @license: EUPL 1.2
 */
 
#ifndef MATH_H
#define MATH_H
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include "../../misc/includes/util.h"

#define E 2.71828                           

#define PI 3.14159265358979323846264338327950 

static inline uint32_t abs32(int32_t x) {
    return x < 0 ? -x : x;
}

static inline uint16_t abs16(int16_t x) {
    return x < 0 ? -x : x;
}

static inline uint8_t abs8(int8_t x) {
    return x < 0 ? -x : x;
}

static inline uint32_t min32(int a, int b) {
    return (a < b) ? a : b;
}

static inline uint32_t max32(int a, int b) {
    return (a > b) ? a : b;
}

static inline uint32_t log2u(uint32_t number) {
    uint32_t num=number, counter=0;
    for (; num!=1; counter++) {
        num >>= 1;
    }
    return counter;
}

uint32_t roundf(float number);

double fabs(double x);

double fmod(double x, double y);

float fmodf(float x, float y);

double sin(double x);

double cos(double x);

float sinf(float x);

float cosf(float x);

double tan(double x);

float tanf(float x);

double pow(double x, double y);

static inline int compute_intersection_x(uint16_Vector2_t a, uint16_Vector2_t b, int y) {
    if (a.y == b.y)
        return a.x;
    float t = (float)(y - a.y) / (float)(b.y - a.y);
    return a.x + (int)((b.x - a.x) * t);
}

uint64_t __udivdi3(uint64_t n, uint32_t d);

uint64_t __umoddi3(uint64_t n, uint32_t d);

#endif 
