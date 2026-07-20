//Author: Yazin T.
//License: EUPL v1.2
//Copyright (C) 2026, Orizon Software Foundation
//-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-/-
//                greedy LZ4 block-format encoder
 
//   produces output readable by LZ4_decompress_safe() from the kernel's
//   lz4.c; it does not need to be fast, only correct.
 
//format and description:
//  sequence = token(1B) [literal-len-ext] literals
//               [offset(2B LE) matchlen-ext]     <- omitted for final seq

//    token = (literalLen4 << 4) | matchLen4

//    literalLen4 == 15  -> read extra bytes, each 0-255, sum added,
//                          stop after a byte != 255

//    matchLen4   == 15  -> same extension scheme; final matchLen += 4 (MINMATCH)

//    final sequence in the block is literals-only: no offset/matchlen bytes
//    follow it, and its literal length must exactly consume all remaining
//    input and exactly fill all remaining output.


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define MINMATCH 4
#define MFLIMIT 12      //stop searching for new matches once fewer bytes remain 
#define LASTLITERALS 5
#define ML_MASK 15
#define RUN_MASK 15
#define MAX_OFFSET 65535

#define HASH_LOG 16
#define HASH_SIZE (1 << HASH_LOG)

static uint32_t hash4(const uint8_t *p)
{
    uint32_t v;
    memcpy(&v, p, 4);
    //Fibonacci hashing
    return (v * 2654435761u) >> (32 - HASH_LOG);
}

// dynamic output buffer
typedef struct {
    uint8_t *buf;
    size_t len, cap;
} obuf_t;

static void ob_init(obuf_t *o) { o->buf = malloc(1); o->len = 0; o->cap = 1; }
static void ob_push(obuf_t *o, uint8_t b)
{
    if (o->len == o->cap) {
        o->cap *= 2;
        o->buf = realloc(o->buf, o->cap);
    }
    o->buf[o->len++] = b;
}
static void ob_push_buf(obuf_t *o, const uint8_t *src, size_t n)
{
    while (o->len + n > o->cap) { o->cap *= 2; o->buf = realloc(o->buf, o->cap); }
    memcpy(o->buf + o->len, src, n);
    o->len += n;
}

//emit a length using the token-nibble + extension-byte scheme. 
static void emit_length_ext(obuf_t *o, size_t len)
{
    //len here is the amount REMAINING after the nibble's implicit 15
    while (len >= 255) {
        ob_push(o, 255);
        len -= 255;
    }
    ob_push(o, (uint8_t)len);
}

int lz4_block_compress(const uint8_t *src, size_t srcSize, obuf_t *out)
{
    static int32_t hashtab[HASH_SIZE];
    memset(hashtab, -1, sizeof(hashtab));

    size_t pos = 0;
    size_t anchor = 0; // start of current pending literal run

    if (srcSize == 0) {
        // still need a valid (degenerate) stream: token with literalLen 0
        ob_push(out, 0x00);
        return 0;
    }

    while (pos + MFLIMIT <= srcSize) {
        uint32_t h = hash4(&src[pos]);
        int32_t cand = hashtab[h];
        hashtab[h] = (int32_t)pos;

        size_t matchLen = 0;
        size_t offset = 0;

        if (cand >= 0) {
            size_t c = (size_t)cand;
            offset = pos - c;
            if (offset >= 1 && offset <= MAX_OFFSET &&
                memcmp(&src[c], &src[pos], 4) == 0) {
                // extend match forward
                size_t maxLen = srcSize - pos;
                //keep LASTLITERALS bytes at the very end unmatched, so the final sequence is literals-only
                if (maxLen > srcSize - LASTLITERALS - pos)
                    maxLen = srcSize - LASTLITERALS - pos;
                while (matchLen < maxLen && src[c + matchLen] == src[pos + matchLen])
                    matchLen++;
                if (matchLen < MINMATCH)
                    matchLen = 0;
            }
        }

        if (matchLen == 0) {
            pos++;
            continue;
        }

        //we have a match: emit [literals anchor..pos) + match
        size_t litLen = pos - anchor;
        size_t mlCode = matchLen - MINMATCH;

        uint8_t tokLit = (litLen < RUN_MASK) ? (uint8_t)litLen : RUN_MASK;
        uint8_t tokMl  = (mlCode < ML_MASK)  ? (uint8_t)mlCode  : ML_MASK;
        ob_push(out, (uint8_t)((tokLit << 4) | tokMl));

        if (litLen >= RUN_MASK)
            emit_length_ext(out, litLen - RUN_MASK);
        if (litLen > 0)
            ob_push_buf(out, &src[anchor], litLen);

        uint16_t off16 = (uint16_t)offset;
        ob_push(out, (uint8_t)(off16 & 0xFF));
        ob_push(out, (uint8_t)((off16 >> 8) & 0xFF));

        if (mlCode >= ML_MASK)
            emit_length_ext(out, mlCode - ML_MASK);

        //update hash table for a few positions inside the match (helps later matches; not required for correctness)
        size_t upd_end = pos + matchLen;
        for (size_t p = pos + 1; p < upd_end && p + 4 <= srcSize; p++)
            hashtab[hash4(&src[p])] = (int32_t)p;

        pos = upd_end;
        anchor = pos;
    }

    //final literal-only sequence: everything from anchor to end
    size_t litLen = srcSize - anchor;
    uint8_t tokLit = (litLen < RUN_MASK) ? (uint8_t)litLen : RUN_MASK;
    ob_push(out, (uint8_t)(tokLit << 4)); //matchlen nibble = 0, unused
    if (litLen >= RUN_MASK)
        emit_length_ext(out, litLen - RUN_MASK);
    if (litLen > 0)
        ob_push_buf(out, &src[anchor], litLen);

    return 0;
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "usage: %s <in> <out.lz4>\n", argv[0]);
        return 1;
    }
    FILE *fin = fopen(argv[1], "rb");
    if (!fin) { perror("fopen in"); return 1; }
    fseek(fin, 0, SEEK_END);
    long sz = ftell(fin);
    fseek(fin, 0, SEEK_SET);
    uint8_t *data = malloc(sz > 0 ? sz : 1);
    if (sz > 0 && fread(data, 1, sz, fin) != (size_t)sz) {
        fprintf(stderr, "short read\n");
        return 1;
    }
    fclose(fin);

    obuf_t out;
    ob_init(&out);
    lz4_block_compress(data, (size_t)sz, &out);

    FILE *fout = fopen(argv[2], "wb");
    if (!fout) { perror("fopen out"); return 1; }
    fwrite(out.buf, 1, out.len, fout);
    fclose(fout);

    fprintf(stderr, "input:  %ld bytes\noutput: %zu bytes (%.1f%%)\n",
            sz, out.len, sz ? (100.0 * out.len / sz) : 0.0);
    return 0;
}
