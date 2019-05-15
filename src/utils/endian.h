#ifndef _SCHAUFEL_UTILS_ENDIAN_H
#define _SCHAUFEL_UTILS_ENDIAN_H

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

#endif
