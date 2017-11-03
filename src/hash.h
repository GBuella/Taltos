/* vim: set filetype=c : */
/* vim: set noet tw=80 ts=8 sw=8 cinoptions=+4,(0,t0: */
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

#ifndef TALTOS_HASH_H
#define TALTOS_HASH_H

#include <stdbool.h>
#include <inttypes.h>
#include <stdio.h>

#include "chess.h"

struct hash_table;

enum value_type {
	vt_none = 0,
	vt_upper_bound = 1 << 24,
	vt_lower_bound = 2 << 24,
	vt_exact = 3 << 24
};

#define VALUE_TYPE_MASK (3 << 24)

typedef uint64_t ht_entry;
#define HT_NULL UINT64_C(0)
#define F_ZHASH_HEX PRIx64

static_assert(move_bit_mask_plus_one_shift <= 24, "layout requirement");

/*
 * A hash table entry is a 64bit value, with the following bitfields:
 *
 * best move    : bits  0 - 23
 * value type   : bits 24 - 25
 * reserved     : bits 26 - 30
 * no_null      : bits 31 - 31
 * value        : bits 32 - 47
 * depth        : bits 48 - 63
 *
 * This pattern depends on the bits used by move type.
 */

#define HT_NO_NULL_FLAG (1u << 31)

static_assert(
	(MOVE_MASK & VALUE_TYPE_MASK) == 0,
	    "unexpected overlapping masks");



static inline move
ht_move(ht_entry e)
{
	return (move)(e & MOVE_MASK);
}

static inline ht_entry
ht_set_move(ht_entry e, move m)
{
	invariant(is_move_valid(m));
	return e | m;
}

static inline ht_entry
ht_set_depth(ht_entry e, int depth)
{
	invariant(depth <= MAX_PLY * PLY);
	invariant(depth <= 0xffff);

	if (depth < 0)
		return e;
	return e | (((ht_entry)depth) << (32 + 16));
}

static inline ht_entry
ht_set_value(ht_entry e, enum value_type vt, int value)
{
	invariant(value >= -0x7fff && value < 0x7fff);
	invariant((vt & VALUE_TYPE_MASK) == vt);

	e |= (ht_entry)vt;
	e |= (((ht_entry)(value + 0x7fff)) << 32);
	return e;
}

static inline ht_entry
ht_clear_value(ht_entry e)
{
	return e & ~(VALUE_TYPE_MASK | (UINT64_C(0xffff) << 32));
}

static inline bool
ht_has_move(ht_entry e)
{
	return (e & MOVE_MASK) != 0;
	// perhaps rather   return ((move)e) != 0;   ??
}

static inline ht_entry
ht_reset_move(ht_entry dst, ht_entry src)
{
	return (dst & ~((ht_entry)MOVE_MASK)) | (src & MOVE_MASK);
}

static inline ht_entry
ht_set_no_move(ht_entry e)
{
	return e & ~((ht_entry)MOVE_MASK);
}

static inline int
ht_depth(ht_entry e)
{
	return (unsigned)(e >> (32 + 16));
}

static inline enum value_type
ht_value_type(ht_entry e)
{
	return (int)(e & VALUE_TYPE_MASK);
}

static inline bool
ht_value_is_lower_bound(ht_entry e)
{
	return (e & vt_lower_bound) != 0;
}

static inline bool
ht_value_is_upper_bound(ht_entry e)
{
	return (e & vt_upper_bound) != 0;
}

static inline int
ht_value(ht_entry e)
{
	return (int)(((e >> 32) % 0x10000) - 0x7fff);
}

static inline ht_entry
ht_set_no_null(ht_entry e)
{
	return e | HT_NO_NULL_FLAG;
}

static inline bool
ht_no_null(ht_entry e)
{
	return (e & HT_NO_NULL_FLAG) != 0;
}

static inline ht_entry
create_ht_entry(int value, enum value_type vt, move m, int depth)
{
	invariant(value >= -0x7fff && value < 0x7fff);
	invariant((vt & VALUE_TYPE_MASK) == vt);
	invariant((m & MOVE_MASK) == m);
	if (depth < 0)
		depth = 0;
	invariant(depth <= 0xff);

	ht_entry e = 0;
	e = ht_set_move(e, m);
	e = ht_set_value(e, vt, value);
	e = ht_set_depth(e, depth);

	assert(ht_move(e) == m);
	assert(ht_value_type(e) == vt);
	assert(ht_value(e) == value);
	assert(ht_depth(e) == depth);

	return e;
}

static inline bool
ht_is_set(ht_entry e)
{
	return e != HT_NULL;
}



static inline uint64_t
z_toggle_ep_file(uint64_t hash, int file)
{
	invariant(file >= 0 && file <= 7);

	static const uint64_t zobrist_value[8] = {
		UINT64_C(0x70CC73D90BC26E24), UINT64_C(0xE21A6B35DF0C3AD7),
		UINT64_C(0x003A93D8B2806962), UINT64_C(0x1C99DED33CB890A1),
		UINT64_C(0xCF3145DE0ADD4289), UINT64_C(0xD0E4427A5514FB72),
		UINT64_C(0x77C621CC9FB3A483), UINT64_C(0x67A34DAC4356550B) };

	return hash ^ zobrist_value[file];
}

static inline uint64_t
z_toggle_sq(uint64_t hash, int i, enum piece p, enum player pl)
{
	invariant(ivalid(i));

	extern const uint64_t z_random[14][64];

	return hash ^= z_random[p + pl][i];
}

static inline void
z2_toggle_sq(uint64_t hash[2], int i, enum piece p, enum player pl)
{
	invariant(ivalid(i));

	extern const uint64_t z_random[14][64];

	hash[0] ^= z_random[p + pl][i];
	hash[1] ^= z_random[opponent_of(p + pl)][flip_i(i)];
}

static inline uint64_t
z_toggle_castle_queen_side_opponent(uint64_t hash)
{
	return hash ^ UINT64_C(0x1EF6E6DBB1961EC9);
}

static inline uint64_t
z_toggle_castle_queen_side(uint64_t hash)
{
	return hash ^ UINT64_C(0xF165B587DF898190);
}

static inline uint64_t
z_toggle_castle_king_side_opponent(uint64_t hash)
{
	return hash ^ UINT64_C(0xA57E6339DD2CF3A0);
}

static inline uint64_t
z_toggle_castle_king_side(uint64_t hash)
{
	return hash ^ UINT64_C(0x31D71DCE64B2C310);
}

static inline void
z2_toggle_castle_queen_side_opponent(uint64_t hash[2])
{
	hash[0] = z_toggle_castle_queen_side_opponent(hash[0]);
	hash[1] = z_toggle_castle_queen_side(hash[1]);
}

static inline void
z2_toggle_castle_queen_side(uint64_t hash[2])
{
	hash[0] = z_toggle_castle_queen_side(hash[0]);
	hash[1] = z_toggle_castle_queen_side_opponent(hash[1]);
}

static inline void
z2_toggle_castle_king_side_opponent(uint64_t hash[2])
{
	hash[0] = z_toggle_castle_king_side_opponent(hash[0]);
	hash[1] = z_toggle_castle_king_side(hash[1]);
}

static inline void
z2_toggle_castle_king_side(uint64_t hash[2])
{
	hash[0] = z_toggle_castle_king_side(hash[0]);
	hash[1] = z_toggle_castle_king_side_opponent(hash[1]);
}

static inline void
prefetch_z2_xor_move(move m)
{
	extern uint64_t alignas(16) zhash_xor_table[64 * 64 * 8 * 8 * 8][2];

	prefetch(zhash_xor_table + m, 0, 0);
}

static inline void
z2_xor_move(uint64_t hash[2], move m)
{
	extern uint64_t alignas(16) zhash_xor_table[64 * 64 * 8 * 8 * 8][2];

	hash[0] ^= zhash_xor_table[m][0];
	hash[1] ^= zhash_xor_table[m][1];
}



struct hash_table *ht_create(unsigned log2_size);

struct hash_table *ht_create_mb(unsigned megabytes);

struct hash_table *ht_resize(struct hash_table*, unsigned log2_size);

struct hash_table *ht_resize_mb(struct hash_table*, unsigned megabytes);

void ht_destroy(struct hash_table*);

ht_entry ht_lookup_fresh(const struct hash_table*, const struct position*);

ht_entry ht_lookup_deep(const struct hash_table*, const struct position*,
			int depth, int beta);

#ifdef TALTOS_CAN_USE_BUILTIN_PREFETCH
void ht_prefetch(const struct hash_table*, uint64_t hash_key);
#else
#define ht_prefetch(...)
#endif

size_t ht_slot_count(const struct hash_table*);

void ht_pos_insert(struct hash_table*, const struct position*, ht_entry);

void ht_extract_pv(const struct hash_table*, const struct position*,
			int depth, move pv[], int value);

size_t ht_size(const struct hash_table*);

bool ht_is_mb_size_valid(unsigned megabytes);

unsigned ht_min_size_mb(void);

unsigned ht_max_size_mb(void);

void ht_clear(struct hash_table*);

void ht_swap(struct hash_table*);

size_t ht_usage(const struct hash_table*);

uint64_t position_polyglot_key(const struct position*, enum player turn);

void init_zhash_table(void);

#endif
