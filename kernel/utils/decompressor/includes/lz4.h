//Author: Yann Collet
//Source: https://github.com/lz4/lz4/blob/dev/lib/lz4.h
//License: BSD-2-Clause License

// Minimal LZ4 block-format decompressor, adapted from the reference
//BSD-2-Clause / GPL2+ LZ4 implementation (Yann Collet).

#ifndef LZ4_H
#define LZ4_H

/*
 Decompress exactly one LZ4 block.

 source               - pointer to compressed data
 dest                 - pointer to destination buffer
 compressedSize       - exact size in bytes of the compressed block
 maxDecompressedSize  - size of the dest buffer / expected decompressed size

 Returns the number of bytes written to dest on success, or a negative
 value on error (malformed stream / buffer overflow attempt).
 */
int LZ4_decompress_safe(const char *source, char *dest,
			 int compressedSize, int maxDecompressedSize);

/*
 Same as LZ4_decompress_safe, but stops as soon as targetOutputSize bytes
 have been written, even if the block has more data. dstCapacity is the
 real size of the dest buffer (targetOutputSize must be <= dstCapacity).
 */
int LZ4_decompress_safe_partial(const char *src, char *dst,
				 int compressedSize,
				 int targetOutputSize, int dstCapacity);

#endif
