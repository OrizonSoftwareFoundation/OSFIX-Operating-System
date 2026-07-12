#ifndef KERNEL_STRING_H
#define KERNEL_STRING_H

#include <stddef.h>
#include <stdint.h>

void *memcpy(void *dest, const void *src, size_t n);

void *memset(void *s, int c, size_t n);

void *memmove(void *dest, const void *src, size_t n);

int memcmp(const void *s1, const void *s2, size_t n);

int memcmp_const(const void *ptr1, uint8_t val, size_t n);

int strcmp(const char *str1, const char *str2);

int strncmp(const char *str1, const char *str2, size_t n);

size_t strlen(const char *str);

char *strcpy(char *dest, const char *src);

char *strncpy(char *dest, const char *src, size_t n);

size_t strlcpy(char *dst, const char *src, size_t n);

char *strcat(char *dest, const char *src);

char *strncat(char *dest, const char *src, size_t n);

size_t strlcat(char *dst, const char *src, size_t size);

char *strchr(const char *str, char chr);

char *strrchr(const char *str, int c);

char *strpbrk(const char *haystack, const char *needle);

char *strstr(const char *haystack, const char *needle);

char *strtok(char *str, const char *delim);

void strswap(char *str, char char1, char char2);

uint32_t strcount(char *str, char char1);

int ipow(int exp);

#endif 
