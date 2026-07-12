/**
 @copyright (C) 2026 Orizon Software Foundation
 @author Yazin T.
 @license: EUPL 1.2
 */

#ifndef UTIL_H
#define UTIL_H
#include <stdint.h>

typedef struct {
    uint16_t x; 

    uint16_t y; 

} uint16_Vector2_t;

#define ALIGN_DOWN(x, align) ((x) & ~((align) - 1))

#define ALIGN_UP(x, align)   (((x) + ((align) - 1)) & ~((align) - 1))

char *itoa(int32_t value, char *str, uint32_t base);

int atoi(const char* str);

long atol(const char* str);

#endif
