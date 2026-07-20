/**
 @copyright (C) 2026 Orizon Software Foundation
 @author Mejd A.
 @license: EUPL 1.2
 */

#ifndef ENDIAN_H
#define ENDIAN_H

#include <stdint.h>

static inline uint64_t be64toh(uint64_t be) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return ((be & 0xFF00000000000000) >> 56) |
           ((be & 0x00FF000000000000) >> 40) |
           ((be & 0x0000FF0000000000) >> 24) |
           ((be & 0x000000FF00000000) >> 8)  |
           ((be & 0x00000000FF000000) << 8)  |
           ((be & 0x0000000000FF0000) << 24) |
           ((be & 0x000000000000FF00) << 40) |
           ((be & 0x00000000000000FF) << 56);
#else
    return be;
#endif
}

static inline uint64_t le64toh(uint64_t le) {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    return ((le & 0xFF00000000000000) >> 56) |
           ((le & 0x00FF000000000000) >> 40) |
           ((le & 0x0000FF0000000000) >> 24) |
           ((le & 0x000000FF00000000) >> 8)  |
           ((le & 0x00000000FF000000) << 8)  |
           ((le & 0x0000000000FF0000) << 24) |
           ((le & 0x000000000000FF00) << 40) |
           ((le & 0x00000000000000FF) << 56);
#else
    return le;
#endif
}

static inline uint32_t be32toh(uint32_t be) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return ((be & 0xFF000000) >> 24) |
           ((be & 0x00FF0000) >> 8)  |
           ((be & 0x0000FF00) << 8)  |
           ((be & 0x000000FF) << 24);
#else
    return be;
#endif
}

static inline uint32_t le32toh(uint32_t le) {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    return ((le & 0xFF000000) >> 24) |
           ((le & 0x00FF0000) >> 8)  |
           ((le & 0x0000FF00) << 8)  |
           ((le & 0x000000FF) << 24);
#else
    return le;
#endif
}

static inline uint16_t be16toh(uint16_t be) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return ((be & 0xFF00) >> 8) |
           ((be & 0x00FF) << 8);
#else
    return be;
#endif
}

static inline uint16_t le16toh(uint16_t le) {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    return ((le & 0xFF00) >> 8) |
           ((le & 0x00FF) << 8);
#else
    return le;
#endif
}

static inline uint64_t bew64toh(uint64_t be) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return ((be & 0xFFFF000000000000) >> 48) |
           ((be & 0x0000FFFF00000000) >> 16) |
           ((be & 0x00000000FFFF0000) << 16) |
           ((be & 0x000000000000FFFF) << 48);
#else
    return be;
#endif
}

static inline uint64_t lew64toh(uint64_t le) {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    return ((le & 0xFFFF000000000000) >> 48) |
           ((le & 0x0000FFFF00000000) >> 16) |
           ((le & 0x00000000FFFF0000) << 16) |
           ((le & 0x000000000000FFFF) << 48);
#else
    return le;
#endif
}

static inline uint32_t bew32toh(uint32_t be) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return ((be & 0xFFFF0000) >> 16) |
           ((be & 0x0000FFFF) << 16);
#else
    return be;
#endif
}

static inline uint32_t lew32toh(uint32_t le) {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    return ((le & 0xFFFF0000) >> 16) |
           ((le & 0x0000FFFF) << 16);
#else
    return le;
#endif
}

static inline uint64_t lebbew64toh(uint64_t lbe) {
    return bew64toh(le64toh(lbe));
}

static inline uint64_t beblew64toh(uint64_t lbe) {
    return lew64toh(be64toh(lbe));
}

static inline uint32_t lebbew32toh(uint32_t lbe) {
    return bew32toh(le32toh(lbe));
}

static inline uint32_t beblew32toh(uint32_t lbe) {
    return lew32toh(be32toh(lbe));
}

#endif 
