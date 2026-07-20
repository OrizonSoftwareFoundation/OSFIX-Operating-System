#include <stddef.h>
#include <stdint.h>

void *memcpy(void *dest, const void *src, size_t n) {
    uint8_t *pdest = (uint8_t *)dest;
    const uint8_t *psrc = (const uint8_t *)src;
    for (size_t i = 0; i < n; i++) {
        pdest[i] = psrc[i];
    }
    return dest;
}

void *memset(void *s, int c, size_t n) {
    uint8_t *p = (uint8_t *)s;
    for (size_t i = 0; i < n; i++) {
        p[i] = (uint8_t)c;
    }
    return s;
}

void *memmove(void *dest, const void *src, size_t n) {
    uint8_t *pdest = (uint8_t *)dest;
    const uint8_t *psrc = (const uint8_t *)src;
    if (src > dest) {
        for (size_t i = 0; i < n; i++) pdest[i] = psrc[i];
    } else if (src < dest) {
        for (size_t i = n; i > 0; i--) pdest[i-1] = psrc[i-1];
    }
    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const uint8_t *p1 = (const uint8_t *)s1;
    const uint8_t *p2 = (const uint8_t *)s2;
    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) return p1[i] < p2[i] ? -1 : 1;
    }
    return 0;
}

int strncmp(const char *str1, const char *str2, size_t n) {
    while (n-- > 0) {
        if (*str1 != *str2) {
            return (unsigned char)(*str1) - (unsigned char)(*str2);
        }
        if (*str1 == '\0') {
            return 0;  
        }
        str1++;
        str2++;
    }
    return 0;  
}

size_t strlen(const char *str) {
    size_t l = 0;
    while (*str++ != 0) {
        l++;
    }
    return l;
}

int strcmp(const char *str1, const char *str2) {
    while (*str1 && (*str1 == *str2)) {
        str1++;
        str2++;
    }
    return (unsigned char)(*str1) - (unsigned char)(*str2);
}

size_t strlcat(char *dst, const char *src, size_t size) {
    const size_t sl = strlen(src),
          dl = strlen(dst);

    if (dl == size) {
        return size + sl;
    }

    if (sl < (size - dl)) {
        memcpy(dst + dl, src, sl + 1);
    } else {
        memcpy(dst + dl, src, size - dl - 1);
        dst[size - 1] = '\0';
    }

    return sl + dl;
}

size_t strlcpy(char *dst, const char *src, size_t n) {

    char *d = dst;
    const char *s = src;
    size_t size = n;

    while (--n > 0) {
        if ((*d++ = *s++) == 0) {
            break;
        }
    }

    if (n == 0) {
        if (size != 0) {
            *d = 0;
        }

        while (*s++);
    }

    return s - src - 1;
}

char *strncpy(char *dest, const char *src, size_t n) {
    char *original = dest;
    size_t i;
    
    for (i = 0; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    
    for ( ; i < n; i++) {
        dest[i] = '\0';
    }
    
    return original;
}

char *strchr(const char *str, char chr) {
    if (str == NULL)
        return NULL;

    while (*str)
    {
        if (*str == chr)
            return (char*)str;

        ++str;
    }

    return NULL;
}

void strswap(char *str, char char1, char char2) {
    if (str == NULL)
        return;

    while (*str) {
        if (*str == char1) {
            *str = char2;
        }
        ++str;
    }
}

uint32_t strcount(char *str, char char1) {
    if (str == NULL)
        return 0;

    uint32_t count = 0;

    while (*str) {
        if (*str == char1) {
            ++count;
        }
        ++str;
    }
    return count;
}

char *strcat(char *dest, const char *src)
{
    char *d = dest;
    while (*d != '\0') d++;

    while ((*d++ = *src++) != '\0') {}
    return dest;
}

char *strncat(char *dest, const char *src, size_t n)
{
    char *d = dest;

    while (*d != '\0') d++;

    while (n-- != 0 && *src != '\0') {
        *d++ = *src++;
    }

    *d = '\0';
    return dest;
}

int memcmp_const(const void *ptr1, const uint8_t val, size_t n) {
    const uint8_t *s1 = (const uint8_t *)ptr1;

    for (size_t i = 0; i < n; i++) {
        if (s1[i] != val) {
            return (int8_t)(s1[i] - val);
        }
    }
    return 0;
}

char* strcpy(char* dest, const char* src) {
    char* d = dest;
    while ((*d++ = *src++) != '\0');
    return dest;
}

char *strstr(const char *haystack, const char *needle)
{
	char *hpos = (char *)haystack;

	if (!*needle) return (char *)haystack;
	while (*hpos && strlen(hpos) >= strlen(needle)) {
		char *a = hpos, *b = (char *)needle;
		while (*a && *b && *a == *b) {
			a++;
			b++;
		}

		if (*b == '\0')
			return hpos;

		hpos++;
	}

	return NULL;
}

char *strpbrk(const char *haystack, const char *needle)
{
	const char *c1, *c2;
	for (c1 = haystack; *c1 != '\0'; ++c1) {
		for (c2 = needle; *c2 != '\0'; ++c2) {
			if (*c1 == *c2) {
				return (char *)c1;
			}
		}
	}
	return NULL;
}

static char *tok = NULL;

char *strtok(char *str, const char *delim)
{
	char *start, *end;
	start = (str != NULL ? str : tok);
	if (start == NULL)
		return NULL;

	end = strpbrk(start, delim);
	if (end) {
		*end = '\0';
		tok = end + 1;
	} else {
		tok = NULL;
	}
	return start;
}

int ipow(int exp)
{
	int res = 1;
	for (int i = 0; i < exp; i++)
		res *= exp;

	return res;
}

char *strrchr(const char *s, int c) {
    const char *last = NULL;
    for (; *s; s++) {
        if (*s == (char)c) last = s;
    }
    if ((char)c == '\0') return (char *)s;
    return (char *)last;
}
