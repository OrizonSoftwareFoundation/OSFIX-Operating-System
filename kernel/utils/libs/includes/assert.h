#include <kprintf.h>

#ifndef ASSERT_H
#define ASSERT_H

extern void kpanic(const char *file, int line, const char *func, const char *msg);

#define assert(expr) \
    ((expr) ? (void)0 : kpanic(__FILE__, __LINE__, __func__, #expr))

void __assert_fail(const char *assertion, const char *file,
                   unsigned int line, const char *function)
{
    kprintf("Assertion failed: %s at %s:%d in %s\n", assertion, file, line, function);
    for (;;) __asm__ volatile("hlt");
}

#endif