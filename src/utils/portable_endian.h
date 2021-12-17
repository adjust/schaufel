#ifndef _SCHAUFEL_UTILS_ENDIAN_H
#define _SCHAUFEL_UTILS_ENDIAN_H

#include <stdint.h>

/* implementation of uint32 bswap32(uint32) */
#if defined(HAVE__BUILTIN_BSWAP32)

#define bswap32(x) __builtin_bswap32(x)

#elif defined(_MSC_VER)

#define bswap32(x) _byteswap_ulong(x)

#else

static inline uint32_t
bswap32(uint32_t x)
{
	return
		((x << 24) & 0xff000000) |
		((x << 8) & 0x00ff0000) |
		((x >> 8) & 0x0000ff00) |
		((x >> 24) & 0x000000ff);
}

#endif							/* HAVE__BUILTIN_BSWAP32 */

/* implementation of uint64 bswap64(uint64) */
#if defined(HAVE__BUILTIN_BSWAP64)

#define bswap64(x) __builtin_bswap64(x)

#elif defined(_MSC_VER)

#define bswap64(x) _byteswap_uint64(x)

#else

#define UINT64CONST(x) (x##UL)

static inline uint64_t
bswap64(uint64_t x)
{
	return
		((x << 56) & UINT64CONST(0xff00000000000000)) |
		((x << 40) & UINT64CONST(0x00ff000000000000)) |
		((x << 24) & UINT64CONST(0x0000ff0000000000)) |
		((x << 8) & UINT64CONST(0x000000ff00000000)) |
		((x >> 8) & UINT64CONST(0x00000000ff000000)) |
		((x >> 24) & UINT64CONST(0x0000000000ff0000)) |
		((x >> 40) & UINT64CONST(0x000000000000ff00)) |
		((x >> 56) & UINT64CONST(0x00000000000000ff));
}
#endif							/* HAVE__BUILTIN_BSWAP64 */

#ifdef WORDS_BIGENDIAN

#define sc_htobe32(x)		(x)
#define sc_htobe64(x)		(x)

#else

#define sc_htobe32(x)		bswap32(x)
#define sc_htobe64(x)		bswap64(x)

#endif							/* WORDS_BIGENDIAN */

#endif
