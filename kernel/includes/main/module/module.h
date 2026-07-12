/**
 @copyright (C) 2026 Orizon Software Foundation
 @author Yazin T.
 @license: EUPL 1.2
 */


#ifndef MODULE_H
#define MODULE_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    int     (*kprintf)(const char *fmt, ...);

    void    *(*malloc) (size_t size);
    void    *(*calloc) (size_t n, size_t size);
    void    *(*realloc)(void *ptr, size_t size);
    void     (*free)   (void *ptr);

    void    *(*memcpy) (void *dst, const void *src, size_t n);
    void    *(*memset) (void *dst, int c, size_t n);
    int      (*memcmp) (const void *a, const void *b, size_t n);
    size_t   (*strlen) (const char *s);
    char    *(*strcpy) (char *dst, const char *src);
    char    *(*strncpy)(char *dst, const char *src, size_t n);
    int      (*strcmp) (const char *a, const char *b);
    int      (*strncmp)(const char *a, const char *b, size_t n);
    char    *(*strstr) (const char *haystack, const char *needle);
    char    *(*strchr) (const char *s, char c);
    void     (*outb)(uint16_t port, uint8_t  val);
    void     (*outw)(uint16_t port, uint16_t val);
    void     (*outl)(uint16_t port, uint32_t val);
    uint8_t  (*inb) (uint16_t port);
    uint16_t (*inw) (uint16_t port);
    uint32_t (*inl) (uint16_t port);

    void     (*ksleep_ms)(uint64_t ms);
    void     (*log_ok)  (const char *msg);
    void     (*log_info)(const char *msg);
    void     (*log_warn)(const char *msg);
    void     (*log_fatal) (const char *msg);
} kernel_api_t;

typedef int (*module_init_fn)(const kernel_api_t *api);

#endif 