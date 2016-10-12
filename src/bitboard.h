
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=(4: */

#ifndef TALTOS_BITBOARD_H
#define TALTOS_BITBOARD_H

#include <assert.h>
#include <stdbool.h>
#include <inttypes.h>

#include "macros.h"

#ifdef TALTOS_CAN_USE_IMMINTRIN_H
#include <immintrin.h>
#endif

#define EMPTY (UINT64_C(0))
#define UNIVERSE UINT64_MAX

static inline attribute(artificial) bool
is_empty(uint64_t bitboard)
{
	return bitboard == EMPTY;
}

static inline attribute(artificial) bool
is_nonempty(uint64_t bitboard)
{
	return bitboard != EMPTY;
}

static inline attribute(artificial) uint64_t
bit64(int index)
{
	invariant(index >= 0 && index <= 63);
	return UINT64_C(1) << index;
}

static inline attribute(artificial) int
bsf(uint64_t value)
{
	invariant(value != 0);

	int result;

#ifdef TALTOS_CAN_USE_BUILTIN_CTZLL_64

	result = __builtin_ctzll(value);

#elif TALTOS_CAN_USE_BUILTIN_CTZL_64

	result = __builtin_ctzl(value);

#elif TALTOS_CAN_USE_INTEL_BITSCANFORWARD64

	unsigned long index;

	_BitScanForward64(&index, value);
	result = (int)index;

#else

	static const char lsb_64_table[64] = {
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
	folded  = (unsigned)((value ^ (value - 1)) >> 32);
	folded ^= (unsigned)(value ^ (value - 1));
	result = lsb_64_table[folded * 0x78291acf >> 26];

#endif

	invariant(result >= 0 && result <= 63);

	return result;
}

#ifdef TALTOS_CAN_USE_INTEL_BSWAP64
#define bswap _bswap64
#elif defined(TALTOS_CAN_USE_BUILTIN_BSWAP64)
#define bswap __builtin_bswap64
#else

static inline attribute(artificial) uint64_t
bswap(uint64_t value)
{
	static const uint64_t k1 = UINT64_C(0x00ff00ff00ff00ff);
	static const uint64_t k2 = UINT64_C(0x0000ffff0000ffff);
	static const uint64_t k3 = UINT64_C(0x00000000ffffffff);

	value = ((value >> 8) & k1) | ((value & k1) << 8);
	value = ((value >> 16) & k2) | ((value & k2) << 16);
	value = ((value >> 32) & k3) | ((value & k3) << 32);
	return value;
}

#endif


#ifdef TALTOS_CAN_USE_INTEL_BLSI64
#define lsb _blsi_u64
#else
static inline attribute(artificial) uint64_t
lsb(uint64_t value)
{
	return (value & (UINT64_C(0) - value));
}
#endif

#ifdef TALTOS_CAN_USE_INTEL_BLSR64
#define reset_lsb _blsr_u64
#else
static inline attribute(artificial) uint64_t
reset_lsb(uint64_t value)
{
	return value & (value - UINT64_C(1));
}
#endif

#ifdef TALTOS_CAN_USE_INTEL_POPCOUNT64
#define popcnt _mm_popcnt_u64
#elif defined(TALTOS_CAN_USE_BUILTIN_POPCOUNTLL64)
#define popcnt __builtin_popcountll
#elif defined(TALTOS_CAN_USE_BUILTIN_POPCOUNTL64)
#define popcnt __builtin_popcountl
#else
static inline attribute(artificial) int
popcnt(uint64_t value)
{
	static const uint64_t k1 = UINT64_C(0x5555555555555555);
	static const uint64_t k2 = UINT64_C(0x3333333333333333);
	static const uint64_t k4 = UINT64_C(0x0f0f0f0f0f0f0f0f);
	static const uint64_t kf = UINT64_C(0x0101010101010101);

	value = value - ((value >> 1)  & k1);
	value = (value & k2) + ((value >> 2)  & k2);
	value = (value + (value >> 4)) & k4;
	value = (value * kf) >> 56;
	return (int)value;
}
#endif

#ifdef TALTOS_CAN_USE_INTEL_PDEP_PEXT_64
#define pdep _pdep_u64
#define pext _pext_u64
#else

static inline uint64_t
pext(uint64_t source, uint64_t selector)
{
	uint64_t result = EMPTY;
	uint64_t dst_bit = UINT64_C(1);

	while (is_nonempty(selector)) {
		if (is_nonempty(source & lsb(selector))) {
			result |= dst_bit;
		}
		selector = reset_lsb(selector);
		dst_bit <<= 1;
	}
	return result;
}

static inline uint64_t
pdep(uint64_t value, uint64_t selector)
{
	uint64_t result = EMPTY;
	uint64_t result_bit = UINT64_C(1);

	while (is_nonempty(selector)) {
		if (is_nonempty(lsb(selector) & value)) {
			result |= result_bit;
		}
		result_bit <<= 1;
		selector = reset_lsb(selector);
	}
	return result;
}

#endif

static inline uint64_t
kogge_stone_north(uint64_t map)
{
	map |= map >> 8;
	map |= map >> 16;
	map |= map >> 32;
	return map;
}

static inline uint64_t
kogge_stone_south(uint64_t map)
{
	map |= map << 8;
	map |= map << 16;
	map |= map << 32;
	return map;
}

static inline uint64_t
fill_files(uint64_t occ)
{
	return kogge_stone_north(kogge_stone_south(occ));
}

static inline attribute(artificial) bool
is_singular(uint64_t map)
{
	return is_empty(reset_lsb(map)) && is_nonempty(map);
}

static inline attribute(artificial) uint64_t
rol(uint64_t value, unsigned d)
{
	return (value << d) | (value >> (64 - d));
}

#endif
