
#ifndef BITMANIPULATE_H
#define BITMANIPULATE_H

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>

#include "macros.h"

#define EMPTY UINT64_C(0)
#define UNIVERSE UINT64_MAX

static inline bool empty(uint64_t bitboard)
{
    return bitboard == EMPTY;
}

static inline bool nonempty(uint64_t bitboard)
{
    return bitboard != EMPTY;
}

static inline uint64_t bit64(unsigned index)
{
    return UINT64_C(1) << index;
}

#if defined(__GNUC__) && defined(BUILD_64)

#include <x86intrin.h>

static inline unsigned bsf(unsigned long long value)
{
    return __builtin_ctzll(value);
}

static inline uint64_t bswap(uint64_t value)
{
    return __builtin_bswap64(value);
}

static inline int popcnt(uint64_t value)
{
    return __builtin_popcountll(value);
}

static inline uint64_t lsb(uint64_t value)
{
    /*  return __blsi_u64(value);  TODO */
    return (value & (UINT64_C(0) - value));
}

static inline uint64_t msb(uint64_t value)
{
    if (value == EMPTY)
        return EMPTY;
    else
        return bit64(63 - __builtin_clzll(value));
}

#elif defined(_MSC_VER) && defined(BUILD_64)

static inline uint64_t lsb(uint64_t value)
{
    return (value & (UINT64_C(0) - value));
}

#include <intrin.h>

static inline unsigned bsf(uint64_t value)
{
	unsigned long index;

	_BitScanForward64(&index, value);
	return (unsigned)index;
}

static inline uint64_t bswap(uint64_t value)
{
    return _byteswap_uint64(value);
}

static inline int popcnt(uint64_t value)
{
    return (int)__popcnt64(value);
}

#else 

static inline uint64_t msb(uint64_t value)
{
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    value |= value >> 32;
    return (value >> 1) + 1;
}

static inline unsigned bsf(uint64_t value)
{
    assert(nonempty(value));
    static const unsigned lsb_64_table[64] =
    {
        63, 30,  3, 32, 59, 14, 11, 33,
        60, 24, 50,  9, 55, 19, 21, 34,
        61, 29,  2, 53, 51, 23, 41, 18,
        56, 28,  1, 43, 46, 27,  0, 35,
        62, 31, 58,  4,  5, 49, 54,  6,
        15, 52, 12, 40,  7, 42, 45, 16,
        25, 57, 48, 13, 10, 39,  8, 44,
        20, 47, 38, 22, 17, 37, 36, 26
    };

    unsigned folded;
    folded  = (unsigned)((value ^ (value-1)) >> 32);
    folded ^= (unsigned)( value ^ (value-1));
    return lsb_64_table[folded * 0x78291ACF >> 26];
}

static inline uint64_t bswap(uint64_t value)
{
	static const uint64_t k1 = UINT64_C(0x00ff00ff00ff00ff);
	static const uint64_t k2 = UINT64_C(0x0000ffff0000ffff);
	static const uint64_t k3 = UINT64_C(0x00000000ffffffff);

	value = ((value >> 8) & k1) | ((value & k1) << 8);
	value = ((value >> 16) & k2) | ((value & k2) << 16);
	value = ((value >> 32) & k3) | ((value & k3) << 32);
	return value;
}

static inline uint64_t lsb(uint64_t value)
{
    return (value & (UINT64_C(0) - value));
}

static inline int popcnt(uint64_t value)
{
	static const uint64_t k1 = UINT64_C(0x5555555555555555);
	static const uint64_t k2 = UINT64_C(0x3333333333333333);
	static const uint64_t k4 = UINT64_C(0x0f0f0f0f0f0f0f0f);
	static const uint64_t kf = UINT64_C(0x0101010101010101);

    value =  value       - ((value >> 1)  & k1);
    value = (value & k2) + ((value >> 2)  & k2);
    value = (value       +  (value >> 4)) & k4 ;
    value = (value * kf) >> 56;
    return (int) value;
}

#endif

static inline uint64_t reset_lsb(uint64_t value)
{
    return value & (value - UINT64_C(1));
}

static inline uint64_t kogge_stone_north(uint64_t map)
{
    map |= map >> 8;
    map |= map >> 16;
    map |= map >> 32;
    return map;
}

static inline uint64_t kogge_stone_south(uint64_t map)
{
    map |= map << 8;
    map |= map << 16;
    map |= map << 32;
    return map;
}

static inline uint64_t pext(uint64_t source, uint64_t selector)
{
    uint64_t result = EMPTY;
    uint64_t dst_bit = UINT64_C(1);

    while (nonempty(selector)) {
        if (nonempty(source & lsb(selector))) {
            result |= dst_bit;
        }
        selector = reset_lsb(selector);
        dst_bit <<= 1;
    }
    return result;
}

static inline uint64_t pdep(uint64_t value, uint64_t selector)
{
    uint64_t result = EMPTY;
	uint64_t result_bit = UINT64_C(1);

	while (nonempty(selector)) {
		if (nonempty(lsb(selector) & value)) {
				result |= result_bit;
		}
		result_bit <<= 1;
		selector = reset_lsb(selector);
    }
    return result;
}

static inline uint64_t snoob(uint64_t value)
{
    uint64_t ripple = value + lsb(value);

    return ripple | (((value ^ ripple) >> 2) / lsb(value));
}

static inline uint64_t rol(uint64_t value, unsigned d)
{
    return (value << d) | (value >> (64 - d));
}

static inline uint64_t interval(uint64_t bit_a, uint64_t bit_b)
{
    assert(popcnt(bit_a) == 1);
    assert(popcnt(bit_b) == 1);
    return bit_a | bit_b | ((bit_a-1) ^ (bit_b-1));
}

#endif

/* vim: set filetype=c : */
