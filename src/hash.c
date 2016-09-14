
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=+4,(4: */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdalign.h>

#include "macros.h"

#include "hash.h"
#include "z_random.inc"
#include "search.h"
#include "util.h"
#include "eval.h"

#include "str_util.h"

#ifndef HT_MIN_SIZE
#define HT_MIN_SIZE 6
#endif

#ifndef HT_MAX_SIZE
#define HT_MAX_SIZE 29
#endif


/*
 * The bits in the lower 32 bits, not used for storing the move,
 * or the value_type.
 */
#define COUNTER_MASK ((uint64_t)((uint32_t)~(MOVE_MASK | VALUE_TYPE_MASK)))

static_assert(
	COUNTER_MASK == UINT64_C(0xf080c000),
	"COUNTER_MASK not as expected,"
	" there might be some problem with some values here...");

#define HASH_MASK (~COUNTER_MASK)

struct slot {
	alignas(16) uint64_t hash_key;
	uint64_t entry;
};

#ifndef FRESH_SLOT_COUNT
#define FRESH_SLOT_COUNT 2
#endif

#ifndef DEEP_SLOT_COUNT
#define DEEP_SLOT_COUNT 6
#endif

static_assert((FRESH_SLOT_COUNT > 0),
		"FRESH_SLOT_COUNT expected to be a positive integer");

static_assert((DEEP_SLOT_COUNT > 0),
		"DEEP_SLOT_COUNT expected to be a positive integer");

static_assert((DEEP_SLOT_COUNT % 2) == 0,
		"DEEP_SLOT_COUNT expected to be an even number");

#define SLOT_COUNT (FRESH_SLOT_COUNT + DEEP_SLOT_COUNT)

static_assert(((SLOT_COUNT & (SLOT_COUNT - 1)) == 0),
		"FRESH_SLOT_COUNT + DEEP_SLOT_COUNT "
		"expected to be a power of two");

struct bucket {
	alignas(SLOT_COUNT * sizeof(struct slot))
		volatile struct slot slots[SLOT_COUNT];
};

struct hash_table {
	unsigned long bucket_count;
	struct bucket *table;
	unsigned long usage;
	unsigned log2_size;
};

size_t
ht_slot_count(const struct hash_table *ht)
{
	return ht->bucket_count * ARRAY_LENGTH(ht->table[0].slots);
}

size_t
ht_usage(const struct hash_table *ht)
{
	return ht->usage;
}

size_t
ht_size(const struct hash_table *ht)
{
	return ht->bucket_count * sizeof(ht->table[0]);
}

struct hash_table*
ht_create(unsigned log2_size)
{
	struct hash_table *ht;

	if (log2_size < HT_MIN_SIZE || log2_size > HT_MAX_SIZE) {
		return NULL;
	}
	ht = xmalloc(sizeof *ht);
	ht->bucket_count = (((size_t)1) << log2_size);
	ht->log2_size = log2_size;
	ht->table = aligned_alloc(64, ht_size(ht));
	if (ht->table == NULL) {
		free(ht);
		return NULL;
	}
	ht_clear(ht);
	return ht;
}

struct hash_table*
ht_resize(struct hash_table *ht, unsigned log2_size)
{
	unsigned long bucket_count = (1lu << log2_size);

	if (ht == NULL)
		return ht_create(log2_size);

	if (ht->log2_size == log2_size)
		return ht;

	struct bucket *table;

	table = aligned_alloc(64, bucket_count * sizeof(table[0]));
	if (table == NULL)
		return NULL;

	xaligned_free(ht->table);
	ht->table = table;
	ht->bucket_count = bucket_count;
	ht_clear(ht);
	return ht;
}

void
ht_clear(struct hash_table *ht)
{
	memset((void*)(ht->table), 0, ht_size(ht));
	ht->usage = 0;
}

void
ht_destroy(struct hash_table *ht)
{
	if (ht != NULL) {
		xaligned_free((void*)ht->table);
		free(ht);
	}
}

void
ht_prefetch(const struct hash_table *ht, uint64_t hash)
{
	prefetch(ht->table + (hash % ht->bucket_count));
}

static bool
hash_match(const struct slot *slot, const uint64_t hash[static 2])
{
	return ((slot->hash_key ^ hash[1]) & HASH_MASK) == 0;
}

static bool
thread_id_match(const struct slot *slot)
{
	(void) slot;
	return true;
	// todo: return ((slot->hash_key ^ slot->entry) & COUNTER_MASK) == 0;
}

static bool
move_ok(ht_entry e, const struct position *pos)
{
	if (!ht_has_move(e))
		return true;

	move m = ht_move(e);
	if (pos_piece_at(pos, mfrom(m)) == 0)
		return false;
	if (pos_player_at(pos, mfrom(m)) != 0)
		return false;
	return ((pos_piece_at(pos, mto(m)) == 0)
	    || (pos_player_at(pos, mto(m)) == 1));
}

ht_entry
ht_lookup_fresh(const struct hash_table *ht,
		const struct position *pos)
{
	volatile struct bucket *bucket =
	    ht->table + (pos->zhash[0] % ht->bucket_count);

	unsigned index = DEEP_SLOT_COUNT;
	index += ((pos->zhash[0] / ht->bucket_count) % FRESH_SLOT_COUNT);

	struct slot slot = bucket->slots[index];

	if (thread_id_match(&slot)
	    && hash_match(&slot, pos->zhash)
	    && move_ok(slot.entry, pos))
		return slot.entry;
	return 0;
}

ht_entry
ht_lookup_deep(const struct hash_table *ht,
		const struct position *pos,
		int depth,
		int beta)
{
	ht_entry best = 0;
	volatile struct bucket *bucket =
	    ht->table + (pos->zhash[0] % ht->bucket_count);

	for (size_t i = 0; i < DEEP_SLOT_COUNT; ++i) {
		struct slot slot = bucket->slots[i];
		if (thread_id_match(&slot)
		    && hash_match(&slot, pos->zhash)
		    && move_ok(slot.entry, pos)) {
			if (ht_depth(slot.entry) >= depth) {
				if (ht_value_type(slot.entry) == vt_exact)
					return slot.entry;
				if (ht_value_type(slot.entry) == vt_lower_bound
				    && ht_value(slot.entry) >= beta)
					return slot.entry;
			}
			if (ht_depth(slot.entry) >= ht_depth(best)) {
				best = slot.entry;
			}
		}
	}
	return best;
}

/*
 * todo:
 * static uint64_t
 * increment_counter(uint64_t counter)
 * {
 * return (counter - COUNTER_MASK) & COUNTER_MASK;
 * }
 */

static void
overwrite_slot(volatile struct slot *dst,
		const uint64_t hash[static 2],
		ht_entry entry)
{
	// todo:
	// uint64_t counter = increment_counter(dst->hash_key & COUNTER_MASK);
	uint64_t counter = 0;
	dst->hash_key = (hash[1] & HASH_MASK) | counter;
	dst->entry = entry | counter;
}

static void
write_fresh_slot(struct hash_table *ht,
		volatile struct bucket *bucket,
		const uint64_t hash[static 2],
		ht_entry entry)
{
	unsigned index = DEEP_SLOT_COUNT;
	index += ((hash[0] / ht->bucket_count) % FRESH_SLOT_COUNT);

	overwrite_slot(bucket->slots + index, hash, entry);
}

void
ht_pos_insert(struct hash_table *ht,
		const struct position *pos,
		ht_entry entry)
{
	volatile struct bucket *bucket;
	const uint64_t *hash;
	if (!ht_is_set(entry))
		return;

	hash = pos->zhash;
	bucket = ht->table + (hash[0] % ht->bucket_count);

	write_fresh_slot(ht, bucket, hash, entry);
	if (ht_depth(entry) < PLY)
		return;

	unsigned depth_min_index = 0;
	int depth_min = 0x10000;

	for (unsigned i = 0; i < DEEP_SLOT_COUNT / 2; ++i) {
		struct slot tslot = bucket->slots[i];
		if (hash_match(&tslot, hash)
		    && move_ok(tslot.entry, pos)
		    && (ht_value_type(tslot.entry) == ht_value_type(entry))) {
			if (ht_depth(tslot.entry) <= ht_depth(entry))
				overwrite_slot(bucket->slots + i, hash, entry);
			return;
		}
		if (ht_depth(tslot.entry) < depth_min) {
			depth_min = ht_depth(tslot.entry);
			depth_min_index = i;
		}
	}
	if (depth_min < ht_depth(entry)) {
		if (bucket->slots[depth_min_index].hash_key == 0)
			ht->usage++;

		overwrite_slot(bucket->slots + depth_min_index, hash, entry);
	}
}

void
ht_extract_pv(const struct hash_table *ht,
		const struct position *pos,
		int depth,
		move pv[],
		int value)
{
	move moves[MOVE_ARRAY_LENGTH];
	ht_entry e;
	struct position child[1];

	pv[0] = 0;
	if (depth <= 0)
		return;
	(void) gen_moves(pos, moves);
	e = ht_lookup_deep(ht, pos, depth, max_value);
	if (!ht_is_set(e))
		return;
	if (!ht_has_move(e))
		return;
	if (ht_depth(e) < depth && depth > PLY)
		return;
	if (ht_value_type(e) != vt_exact || ht_value(e) != value)
		return;
	for (move *m = moves; *m != ht_move(e); ++m)
		if (*m == 0)
			return;
	pv[0] = ht_move(e);
	position_make_move(child, pos, pv[0]);
	ht_extract_pv(ht, child, depth - PLY, pv + 1, -value);
}

static void
slot_swap(struct hash_table *ht, volatile struct slot *slot)
{
	if (slot->hash_key != 0) {
		ht->usage--;
		slot[DEEP_SLOT_COUNT / 2] = *slot;
		slot->hash_key = 0;
		slot->entry = 0;
	}
}

void
ht_swap(struct hash_table *ht)
{
	for (unsigned i = 0; i != ht->bucket_count; ++i) {
		volatile struct bucket *bucket = ht->table + i;
		for (unsigned si = 0; si < DEEP_SLOT_COUNT / 2; ++si)
			slot_swap(ht, bucket->slots + si);
	}
}

uint64_t
position_polyglot_key(const struct position *pos, enum player turn)
{
	uint64_t key = UINT64_C(0);

	/*
	 * Taltos an polyglot have different table representations,
	 * while Taltos uses the 64 bit constants also used by Polyglot.
	 * Hence using here (7-row) and (7-file) for
	 * indexing the z_random array.
	 */
	for (int row = 0; row < 8; ++row) {
		for (int file = 0; file < 8; ++file) {
			enum piece p = pos_piece_at(pos, ind(row, file));
			enum player pl = pos_player_at(pos, ind(row, file));
			int index;

			if (p == nonpiece)
				continue;

			if (turn == white) {
				pl = opponent_of(pl);
				index = (7 - row) * 8 + (7 - file);
			}
			else {
				index = row * 8 + (7 - file);
			}
			key ^= z_random[p + pl][index];
		}
	}
	if (turn == white) {
		if (position_cr_opponent_queen_side(pos))
			key = z_toggle_castle_queen_side_opponent(key);
		if (position_cr_queen_side(pos))
			key = z_toggle_castle_queen_side(key);
		if (position_cr_opponent_king_side(pos))
			key = z_toggle_castle_king_side_opponent(key);
		if (position_cr_king_side(pos))
			key = z_toggle_castle_king_side(key);
	}
	else {
		if (position_cr_opponent_queen_side(pos))
			key = z_toggle_castle_queen_side(key);
		if (position_cr_queen_side(pos))
			key = z_toggle_castle_queen_side_opponent(key);
		if (position_cr_opponent_king_side(pos))
			key = z_toggle_castle_king_side(key);
		if (position_cr_king_side(pos))
			key = z_toggle_castle_king_side_opponent(key);
	}
	if (pos_has_ep_target(pos)) {
		if (is_nonempty(pos_pawn_attacks_player(pos)
		    & bit64(pos->ep_index + NORTH))) {
			int file = 7 - pos_en_passant_file(pos);
			key = z_toggle_ep_file(key, file);
		}
	}
	if (turn == white)
		key ^= UINT64_C(0xF8D626AAAF278509);
	return key;
}
