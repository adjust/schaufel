#ifndef _SCHAUFEL_UTILS_ENDIAN_H
#define _SCHAUFEL_UTILS_ENDIAN_H

#include <stdint.h>

#ifdef HAVE_CONFIG_H
#include "schaufel_config.h"

/* implementation of uint32 bswap32(uint32) */
#if defined(HAVE__BUILTIN_BSWAP32)

/* BSD expects bswap32 to be defined with _BSD_SOURCE
 * in sys/types.h and glibc copies that behaviour */
#ifndef bswap32
#define bswap32(x) __builtin_bswap32(x)
#endif

/* If we ever do CMAKE, we can have these just in case */
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

#endif                          /* HAVE__BUILTIN_BSWAP32 */

/* implementation of uint64 bswap64(uint64) */
#if defined(HAVE__BUILTIN_BSWAP64)

/* BSD expects bswap64 to be defined with _BSD_SOURCE
 * in sys/types.h and glibc copies that behaviour */
#ifndef bswap64
#define bswap64(x) __builtin_bswap64(x)
#endif

/* If we ever do CMAKE, we can have these just in case */
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
#endif                          /* HAVE__BUILTIN_BSWAP64 */

#ifdef WORDS_BIGENDIAN

#define htobe32(x)      (x)
#define htobe64(x)      (x)

#else

#define htobe32(x)      bswap32(x)
#define htobe64(x)      bswap64(x)

#endif                          /* WORDS_BIGENDIAN */

#else /* HAVE_CONFIG_H */
/* We're in a manual Makefile build, guess the required header */

#if defined(__linux__)
#include <endian.h>
#elif defined(__APPLE__)
#include <libkern/OSByteOrder.h>
#define htobe32(x) OSSwapHostToBigInt32(x)
#define htobe64(x) OSSwapHostToBigInt64(x)
#elif defined(__FreeBSD__) || defined(__OpenBSD__) \
    || defined(__NetBSD__) || defined(__DragonFly__)
#include <sys/endian.h>
#endif

#endif                          /* HAVE_CONFIG_H */

#endif
