
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "macros.h"

#include "hash.h"
#include "position.h"
#include "z_random.inc"
#include "search.h"
#include "trace.h"

#ifndef MAX_SLOT_COUNT
#define MAX_SLOT_COUNT 8
#endif

#ifndef HT_MIN_SIZE
#define HT_MIN_SIZE 16
#endif

#ifndef HT_MAX_SIZE
#define HT_MAX_SIZE 29
#endif

struct hash_table {
    ht_entry *table;
    size_t size;
    uint64_t zhash_mask;
    bool is_dual;
    unsigned long usage;
    unsigned long entry_count;
    unsigned slot_count;
    unsigned log2_size;
#   ifdef VERIFY_HASH_TABLE
    uint64_t *boards;
#   endif
};

static ht_entry *ht_allocate(size_t size)
{
#   if __STDC_VERSION__ >= 201112L
    return aligned_alloc(sizeof(ht_entry) * 4, size);
#   else
    return malloc(size);
#   endif
}

int ht_usage(const struct hash_table *ht)
{
    if (ht == NULL) return -1;
    return (int)((ht->usage * 1000) / ht->entry_count);
}

struct hash_table *
ht_create(unsigned log2_size, bool is_dual, unsigned slot_count)
{
    struct hash_table *ht;

    if (log2_size < HT_MIN_SIZE || log2_size > HT_MAX_SIZE) {
        return NULL;
    }
    if (slot_count < 1 || slot_count > MAX_SLOT_COUNT) {
        return NULL;
    }
    ht = malloc(sizeof *ht);
    if (ht == NULL) return NULL;
    ht->slot_count = slot_count;
    ht->is_dual = is_dual;
    ht->entry_count = (1 << log2_size) + MAX_SLOT_COUNT;
    ht->size = ht->entry_count * sizeof(ht_entry);
    ht->log2_size = log2_size;
    if (is_dual) {
        ht->size <<= 1;
    }
    ht->zhash_mask = (1 << log2_size) - 1;
    ht->table = ht_allocate(ht->size);
    if (ht->table == NULL) {
        free(ht);
        return NULL;
    }
#   ifdef VERIFY_HASH_TABLE
    ht->boards = malloc(ht->entry_count * (is_dual ? 2 : 1) * 5 * 8);
#   endif
    ht->usage = 0;
    ht_clear(ht);
    return ht;
}

struct hash_table *
ht_resize(struct hash_table *ht,
          unsigned log2_size,
          bool is_dual,
          unsigned slot_count)
{
    if (ht == NULL) {
        return ht_create(log2_size, is_dual, slot_count);
    }
    else if (ht->log2_size == log2_size && ht->is_dual == is_dual) {
        ht->slot_count = slot_count;
        return ht;
    }
    else {
        ht_destroy(ht);
        return ht_create(log2_size, is_dual, slot_count);
    }
}

void ht_clear(const struct hash_table *ht)
{
    if (ht != NULL) {
        memset((void*)(ht->table), 0, ht->size); 
    }
}

void ht_destroy(struct hash_table *ht)
{
    if (ht != NULL) {
        free((void*)ht->table);
        free(ht);
    }
}

void setup_zhash(struct position *pos)
{
    pos->hash[1] = pos->hash[0] = UINT64_C(0);
    for (int i = 0; i < 64; ++i) {
        z2_toggle_sq(pos->hash, i, get_piece_at(pos, i), get_player_at(pos, i));
    }
    if (pos->castle_left_1) z2_toggle_castle_left_1(pos->hash);
    if (pos->castle_left_0) z2_toggle_castle_left_0(pos->hash);
    if (pos->castle_right_1) z2_toggle_castle_right_1(pos->hash);
    if (pos->castle_right_0) z2_toggle_castle_right_0(pos->hash);
    if (ind_rank(pos->ep_ind) == RANK_5) {
        pos->hash[1] = z_toggle_ep_file(pos->hash[1], ind_file(pos->ep_ind));
    }
}

static bool hash_ok(uint64_t a, uint64_t b)
{
    /* Assuming both ht_entry and zobrist_hash types are
       uint64_t
     */
    return (a >> 29) == (b >> 29);
}

void ht_prefetch(const struct hash_table *ht, zobrist_hash hash)
{
    if (ht == NULL) return;

    unsigned index = (unsigned)(hash & ht->zhash_mask);
    if (ht->is_dual) {
        index <<= 1;
    }
    PREFETCH(ht->table + index);
}

ht_entry ht_lookup(const struct hash_table *ht, zobrist_hash hash)
{
    if (ht == NULL) return HT_NULL;

    ht_entry entry;

    unsigned index = (unsigned)(hash & ht->zhash_mask);

    if (ht->is_dual) {
        index <<= 1;
    }
    entry = ht->table[index];
    if (hash_ok(entry, hash)) {
        return entry;
    }
    if (ht->is_dual) {
        entry = ht->table[index+1];
        if (hash_ok(entry, hash)) {
            return entry;
        }
    }
    else {
        return HT_NULL;
    }
    return hash_ok(entry, hash) ? entry : HT_NULL;
}

static bool move_ok(ht_entry e, unsigned move_count)
{
    if (!ht_has_move(e)) return true;

    unsigned mi = ht_move_index(e);

    if (move_count == 0) return true;
    return mi < move_count;
}

ht_entry ht_pos_lookup(const struct hash_table *ht,
                       const struct position *pos,
                       unsigned m_count)
{
    if (ht == NULL) return HT_NULL;

    int tries;
    unsigned index = (unsigned)(pos->hash[1] & ht->zhash_mask);

    if (ht->is_dual) {
        index <<= 1;
    }

lookup_loop:
    tries = ht->slot_count;
    do {
        ht_entry entry = ht->table[index];
        if (!ht_is_set(entry)) {
            break;
        }
        if (hash_ok(entry, pos->hash[1]) && move_ok(entry, m_count)) {
#   ifdef VERIFY_HASH_TABLE
            if (memcmp(pos->bb, ht->boards+index*5, 5*7) != 0) {
                printf("Hash Collision!\n");
            }
#   endif
            return entry;
        }
        index += ht->is_dual ? 2 : 1;
        --tries;
    } while (tries != 0);
    if (ht->is_dual && (index & 1) == 0) {
        index = ((unsigned)(pos->hash[1] & ht->zhash_mask) << 1) + 1;
        goto lookup_loop;
    }
    return HT_NULL;
}

#ifdef VERIFY_HASH_TABLE
void ht_insert(struct hash_table *ht, zobrist_hash hash, ht_entry entry,
               const uint64_t bb[static 5])
#else
void ht_insert(struct hash_table *ht, zobrist_hash hash, ht_entry entry)
#endif
{
    if (ht == NULL) return;

    int tries = ht->slot_count;
    unsigned index = (unsigned)(hash & ht->zhash_mask);
    entry |= hash & UINT64_C(0xffffffffe0000000);

    if (ht->is_dual) {
        index <<= 1;
    }
    else if (ht->slot_count == 1) {
        ht->table[index] = entry;
        ht->usage++;
#       ifdef VERIFY_HASH_TABLE
        memcpy(ht->boards + index*5, bb, 5*8);
#       endif
        return;
    }
    do {
        ht_entry old = ht->table[index];
        if (!ht_is_set(old)) {
            ht->usage++;
            ht->table[index] = entry;
#           ifdef VERIFY_HASH_TABLE
            memcpy(ht->boards + index*5, bb, 5*8);
#           endif
            return;
        }
        if (hash_ok(entry, old) && ht_depth(old) <= ht_depth(entry)) {
            ht->table[index] = entry;
#           ifdef VERIFY_HASH_TABLE
            memcpy(ht->boards + index*5, bb, 5*8);
#           endif
            return;
        }
        index += ht->is_dual ? 2 : 1;
        --tries;
    } while (tries != 0);
}

void ht_pos_insert(struct hash_table *ht,
                   const struct position *pos,
                   ht_entry entry)
{
    assert(pos != NULL);

    if (ht_is_set(entry)) {
#       ifdef VERIFY_HASH_TABLE
        ht_insert(ht, pos->hash[1], entry, pos->bb);
#       else
        ht_insert(ht, pos->hash[1], entry);
#       endif
    }
}

void ht_extract_pv(const struct hash_table *ht,
                   const struct position *pos,
                   int depth,
                   move pv[])
{
    assert(pv != NULL);

    if (ht == NULL) {
        pv[0] = 0;
        return;
    }

    move moves[MOVE_ARRAY_LENGTH];
    unsigned move_count;
    ht_entry e;
    struct position child[1];

    pv[0] = 0;
    if (depth <= 0) return;
    move_count = (unsigned)(gen_plegal_moves(pos, moves) - moves);
    if (move_count == 0) return;
    e = ht_pos_lookup(ht, pos, move_count);
    if (!ht_is_set(e)) return;
    if (!ht_has_move(e)) return;
    if (ht_depth(e) < depth && depth > PLY) return;
    pv[0] = moves[ht_move_index(e)];
    memcpy(child, pos, sizeof child);
    make_move(child, pv[0]);
    ht_extract_pv(ht, child, depth - PLY, pv + 1);
}

void ht_swap(struct hash_table *ht)
{
    if (ht == NULL || !ht->is_dual) {
        return;
    }
    ht->usage = 0;
    for (unsigned i = 0; i != ht->entry_count; ++i) {
        ht->table[2*i+1] = ht->table[2*i];
        ht->table[2*i] = HT_NULL;
    }
}

