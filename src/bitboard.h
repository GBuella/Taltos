/* vim: set filetype=cpp : */
/* vim: set noet tw=100 ts=8 sw=8 cinoptions=+4,(0,t0: */
/*
 * Copyright 2014-2017, Gabor Buella
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TALTOS_BITBOARD_H
#define TALTOS_BITBOARD_H

/*

bitboard square index map:

    A  B  C  D  E  F  G  H
   -- -- -- -- -- -- -- --
8 | 7| 6| 5| 4| 3| 2| 1| 0| 8
   -- -- -- -- -- -- -- --
7 |15|14|13|12|11|10| 9|10| 7
   -- -- -- -- -- -- -- --
6 |23|22|21|20|19|18|17|16| 6
   -- -- -- -- -- -- -- --
5 |31|30|29|28|27|26|25|24| 5
   -- -- -- -- -- -- -- --
4 |39|38|37|36|35|34|33|32| 4
   -- -- -- -- -- -- -- --
3 |47|46|45|44|43|42|41|40| 3
   -- -- -- -- -- -- -- --
2 |55|54|53|52|51|50|49|48| 2
   -- -- -- -- -- -- -- --
1 |63|62|61|60|59|58|57|56| 1
   -- -- -- -- -- -- -- --
    A  B  C  D  E  F  G  H

*/

#include <cinttypes>

#include "macros.h"

#if __has_include(<x86intrin.h>)
#include <x86intrin.h>
#endif

namespace taltos
{

struct bitboard
{
uint64_t value;

explicit constexpr bitboard(uint64_t n):
	value(n)
{
}

bitboard()
{
}

constexpr bitboard(const bitboard& other):
	value(other.value)
{
}

bitboard(bitboard&& other):
	value(other.value)
{
}

bool operator ==(const bitboard& other) const
{
	return value == other.value;
}

bitboard operator =(const bitboard& other)
{
	value = other.value;
	return *this;
}

bool is_empty() const
{
	return value == 0;
}

bool is_nonempty() const
{
	return not is_empty();
}

constexpr bitboard operator |(const bitboard& other) const
{
	return bitboard{value | other.value};
}

constexpr bitboard operator &(const bitboard& other) const
{
	return bitboard{value & other.value};
}

constexpr bitboard operator ^(const bitboard& other) const
{
	return bitboard{value ^ other.value};
}

constexpr bitboard operator ~() const
{
	return bitboard{~value};
}

constexpr bitboard operator &=(const bitboard& other)
{
	value &= other.value;
	return *this;
}

constexpr bitboard operator |=(const bitboard& other)
{
	value |= other.value;
	return *this;
}

constexpr bitboard operator ^=(const bitboard& other)
{
	value ^= other.value;
	return *this;
}

bitboard ls1b() const
{
#if __has_include(<x86intrin.h>) && defined(__BMI__)
	return bitboard{_blsi_u64(value)};
#else
	return bitboard{value & (UINT64_C(0) - value)};
#endif
}

int ls1b_index() const
{
#ifdef __GNUC__
	return __builtin_ctzll(value);
#elif defined(_MSC_VER)
	unsigned long index;
	_BitScanForward64(&index, value);
	return (int)index;
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
	return lsb_64_table[folded * 0x78291acf >> 26];
#endif
}

void flip()
{
#ifdef __GNUC__
	value = __builtin_bswap64(value);
#else
	static const uint64_t k1 = UINT64_C(0x00ff00ff00ff00ff);
	static const uint64_t k2 = UINT64_C(0x0000ffff0000ffff);
	static const uint64_t k3 = UINT64_C(0x00000000ffffffff);

	value = ((value >> 8) & k1) | ((value & k1) << 8);
	value = ((value >> 16) & k2) | ((value & k2) << 16);
	value = ((value >> 32) & k3) | ((value & k3) << 32);
#endif
}

bitboard flipped() const
{
	bitboard result = *this;
	result.flip();
	return result;
}

int popcnt() const
{
#ifdef __GNUC__
	return __builtin_popcountll(value);
#elif __has_include(<x86intrin.h>) && defined(__POPCNT__)
	return _mm_popcnt_u64(value);
#else
	static const uint64_t k1 = UINT64_C(0x5555555555555555);
	static const uint64_t k2 = UINT64_C(0x3333333333333333);
	static const uint64_t k4 = UINT64_C(0x0f0f0f0f0f0f0f0f);
	static const uint64_t kf = UINT64_C(0x0101010101010101);

	uint64_t x = value;
	x = x - ((x >> 1)  & k1);
	x = (x & k2) + ((x >> 2)  & k2);
	x = (x + (x >> 4)) & k4;
	x = (x * kf) >> 56;
	return (int)x;
#endif
}

void shift_north(int n = 1)
{
	value >>= 8 * n;
}

void shift_south(int n = 1)
{
	value <<= 8 * n;
}

void shift_west(int n = 1)
{
	value <<= n;
}

void shift_east(int n = 1)
{
	value >>= n;
}

void fill_north()
{
	value |= value >> 8;
	value |= value >> 16;
	value |= value >> 32;
}

void fill_south()
{
	value |= value << 8;
	value |= value << 16;
	value |= value << 32;
}

void reset_ls1b()
{
	value &= value - UINT64_C(1);
}

bool is_singular() const
{
	if (is_empty())
		return false;
	bitboard x = *this;
	x.reset_ls1b();
	return x.is_empty();
}

bool is_set(int index) const
{
	return (value & (UINT64_C(1) << index)) != 0;
}

bool is_any_set(int index) const
{
	return is_set(index);
}

template<typename... indices>
bool is_any_set(int index, indices... args) const
{
	return is_set(index) or is_any_set(args...);
}

class iterator
{
private:
	uint64_t value;

	constexpr iterator(uint64_t x):
		value(x)
	{
	}

	friend bitboard;
public:
	int operator *() const
	{
		return bitboard{value}.ls1b_index();
	}

	iterator& operator++()
	{
		auto x = bitboard{value};
		x.reset_ls1b();
		value = x.value;
		return *this;
	}

	bool operator ==(const iterator& other) const
	{
		return value == other.value;
	}

	bool operator !=(const iterator& other) const
	{
		return value != other.value;
	}
};

iterator cbegin() const
{
	return iterator(value);
}

static constexpr iterator cend()
{
	return iterator(0);
}

iterator begin() const
{
	return cbegin();
}

static constexpr iterator end()
{
	return cend();
}

}; // struct bitboard

constexpr bitboard empty{0};

static inline bool is_empty(bitboard x)
{
	return x.is_empty();
}

static inline bool is_nonempty(bitboard x)
{
	return x.is_nonempty();
}

static inline bitboard bb(int index)
{
	invariant(index >= 0 && index <= 63);
	return bitboard{UINT64_C(1) << index};
}

template<typename... indices>
static inline bitboard bb(int index, indices... args)
{
	return bb(index) | bb(args...);
}

static inline int popcnt(bitboard x)
{
	return x.popcnt();
}

constexpr bitboard north_of(bitboard x, int n = 1)
{
	return bitboard{x.value >> 8 * n};
}

constexpr bitboard south_of(bitboard x, int n = 1)
{
	return bitboard{x.value << 8 * n};
}

constexpr bitboard west_of(bitboard x, int n = 1)
{
	return bitboard{x.value << n};
}

constexpr bitboard east_of(bitboard x, int n = 1)
{
	return bitboard{x.value >> n};
}

static inline bitboard filled_north(bitboard x)
{
	x.fill_north();
	return x;
}

static inline bitboard filled_south(bitboard x)
{
	x.fill_south();
	return x;
}

constexpr bitboard bb_file_a{UINT64_C(0x8080808080808080)};
constexpr bitboard bb_file_b = east_of(bb_file_a);
constexpr bitboard bb_file_c = east_of(bb_file_b);
constexpr bitboard bb_file_d = east_of(bb_file_c);
constexpr bitboard bb_file_e = east_of(bb_file_d);
constexpr bitboard bb_file_f = east_of(bb_file_e);
constexpr bitboard bb_file_g = east_of(bb_file_f);
constexpr bitboard bb_file_h = east_of(bb_file_g);

constexpr bitboard bb_rank_8{UINT64_C(0x00000000000000ff)};
constexpr bitboard bb_rank_7 = south_of(bb_rank_8);
constexpr bitboard bb_rank_6 = south_of(bb_rank_7);
constexpr bitboard bb_rank_5 = south_of(bb_rank_6);
constexpr bitboard bb_rank_4 = south_of(bb_rank_5);
constexpr bitboard bb_rank_3 = south_of(bb_rank_4);
constexpr bitboard bb_rank_2 = south_of(bb_rank_3);
constexpr bitboard bb_rank_1 = south_of(bb_rank_2);

constexpr bitboard edges = bb_file_a | bb_file_h | bb_rank_1 | bb_rank_8;

constexpr bitboard diag_a1h8{UINT64_C(0x8040201008040201)};
constexpr bitboard diag_a8h1{UINT64_C(0x0102040810204080)};
constexpr bitboard diag_c2h7{UINT64_C(0x0020100804020100)};

constexpr bitboard black_squares{UINT64_C(0xaa55aa55aa55aa55)};
constexpr bitboard white_squares = ~black_squares;

constexpr bitboard center{UINT64_C(0x0000001818000000)};
constexpr bitboard center4{UINT64_C(0x00003c3c3c3c0000)};

extern const bitboard knight_pattern[64];
extern const bitboard king_pattern[64];
extern const bitboard diag_masks[64];
extern const bitboard adiag_masks[64];
extern const bitboard hor_masks[64];
extern const bitboard ver_masks[64];
extern const bitboard bishop_masks[64];
extern const bitboard rook_masks[64];
extern const bitboard pawn_attacks_north[64];
extern const bitboard pawn_attacks_south[64];

}

#endif
