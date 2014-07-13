
#ifndef HASH_H
#define HASH_H

#include <stdbool.h>

#include <stdio.h>

#include "chess.h"

struct hash_table;

enum value_type {
    vt_none = 0,
    vt_upper_bound = 1,
    vt_lower_bound = 2,
    vt_exact = 3
};

typedef enum value_type value_type;

typedef uint64_t ht_entry;
#define HT_NULL UINT64_C(0)

typedef uint64_t zobrist_hash;

static inline int ht_move_index(ht_entry e)
{
    return e & 0x7f;
}

static inline ht_entry ht_set_move_index(ht_entry e, unsigned i)
{
    assert(i < 0x7f);
    return e | (ht_entry)i;
}

static inline ht_entry ht_copy_move(ht_entry dst, ht_entry src)
{
    return dst | (src & UINT64_C(0x7f));
}

static inline ht_entry ht_set_depth(ht_entry e, int depth)
{
    assert(depth <= MAX_PLY * PLY);
    assert(depth <= 0xff);
    if (depth < 0) return e;
    return e | (ht_entry)(depth << 7);
}

static inline ht_entry ht_set_value(ht_entry e, value_type vt, int value)
{
    assert(value >= -0x800 && value < 0x800);
    assert((vt & 3) == vt);
    return e | ((ht_entry)(vt << 15))
        | (ht_entry)(((ht_entry)value+(ht_entry)0x800) << 17);
}

static inline bool ht_has_move(ht_entry e)
{
    return (e & 0x7f) != 0x7f;
}

static inline ht_entry ht_reset_move(ht_entry dst, ht_entry src)
{
    return (dst & ~UINT64_C(0x7f)) | (src & UINT64_C(0x7f));
}

static inline ht_entry ht_set_no_move(ht_entry e)
{
    return e | 0x7f;
}

static inline int ht_depth(ht_entry e)
{
    return (e >> 7) & 0xff;
}

static inline value_type ht_value_type(ht_entry e)
{
    return (e >> 15) & 3;
}

static inline int ht_value(ht_entry e)
{
    return ((int)(e >> 17) & 0xfff) - 0x800;
}

static inline ht_entry ht_copy_value(ht_entry dst, ht_entry src)
{
    dst = dst | (src & UINT64_C(0x1fff8000));
    assert(ht_value_type(dst) == ht_value_type(src));
    assert(ht_value(dst) == ht_value(src));
    return dst;
}

static inline ht_entry
create_ht_entry(int value, value_type vt, int mindex, int depth)
{
    assert(value >= -0x800 && value < 0x800);
    assert((vt & 3) == vt);
    assert(mindex >= 0 && mindex < 0x7f);
    if (depth < 0) depth = 0;
    assert(depth <= 0xff);

    ht_entry e = (ht_entry)mindex | (ht_entry)(depth << 7)
                 | (ht_entry)(vt << 15) | (ht_entry)((value+0x800) << 17);

    assert(ht_move_index(e) == mindex);
    assert(ht_value_type(e) == vt);
    assert(ht_value(e) == value);

    return e;
}

#ifdef NO_VECTOR_SUBSCRIPT
typedef uint64_t ht_entry_cont[4];
#else
typedef uint64_t ht_entry_cont __attribute__ ((__vector_size__ (32)));
#endif

static inline zobrist_hash z_toggle_ep_file(zobrist_hash hash, file_t file)
{
    assert(file <= 7);

    static const uint64_t zobrist_value[8] = {
        UINT64_C( 0x31D71DCE64B2C310 ), UINT64_C( 0xF165B587DF898190 ),
        UINT64_C( 0xA57E6339DD2CF3A0 ), UINT64_C( 0x1EF6E6DBB1961EC9 ),
        UINT64_C( 0x70CC73D90BC26E24 ), UINT64_C( 0xE21A6B35DF0C3AD7 ),
        UINT64_C( 0x003A93D8B2806962 ), UINT64_C( 0x1C99DED33CB890A1 ) };
    
    return hash ^ zobrist_value[file];
}

static inline zobrist_hash
z_toggle_sq(zobrist_hash hash, int i, piece p, player pl)
{
    assert(ivalid(i));

    extern const zobrist_hash z_random[7][128];

    if (p != nonpiece) {
        return hash ^ z_random[p-1][pl*64 + i];
    }
    else {
        return hash;
    }
}

static inline void
z2_toggle_sq(zobrist_hash hash[static 2], int i, piece p, player pl)
{
    extern const zobrist_hash z_random[7][128];

    if (p != nonpiece) {
        unsigned index = pl*64 + i;
        hash[1] ^= z_random[p-1][index];
        hash[0] ^= z_random[p-1][index ^ (0x78)];
    }
}

static inline zobrist_hash z_toggle_castle_left_1(zobrist_hash hash)
{
    return hash ^ UINT64_C(0xCF3145DE0ADD4289);
}

static inline zobrist_hash z_toggle_castle_left_0(zobrist_hash hash)
{
    return hash ^ UINT64_C(0xD0E4427A5514FB72);
}

static inline zobrist_hash z_toggle_castle_right_1(zobrist_hash hash)
{
    return hash ^ UINT64_C(0x77C621CC9FB3A483);
}

static inline zobrist_hash z_toggle_castle_right_0(zobrist_hash hash)
{
    return hash ^ UINT64_C(0x67A34DAC4356550B);
}

static inline void z2_toggle_castle_left_1(zobrist_hash hash[static 2])
{
    hash[1] = z_toggle_castle_left_1(hash[1]);
    hash[0] = z_toggle_castle_left_0(hash[0]);
}

static inline void z2_toggle_castle_left_0(zobrist_hash hash[static 2])
{
    hash[1] = z_toggle_castle_left_0(hash[1]);
    hash[0] = z_toggle_castle_left_1(hash[0]);
}

static inline void z2_toggle_castle_right_1(zobrist_hash hash[static 2])
{
    hash[1] = z_toggle_castle_right_1(hash[1]);
    hash[0] = z_toggle_castle_right_0(hash[0]);
}

static inline void z2_toggle_castle_right_0(zobrist_hash hash[static 2])
{
    hash[1] = z_toggle_castle_right_0(hash[1]);
    hash[0] = z_toggle_castle_right_1(hash[0]);
}

static inline bool ht_is_set(ht_entry e)
{
    return e != HT_NULL;
}

void setup_zhash(struct position *);

struct hash_table *
ht_create(unsigned log2_size, bool is_dual, unsigned slot_count);
void ht_destroy(struct hash_table *);
ht_entry ht_lookup(const struct hash_table *, zobrist_hash);
ht_entry ht_pos_lookup(const struct hash_table *,
                       const struct position *,
                       unsigned move_count);
#ifdef VERIFY_HASH_TABLE
void ht_insert(struct hash_table *, zobrist_hash, ht_entry,
               const uint64_t[static 5]);
#else
void ht_insert(struct hash_table *, zobrist_hash, ht_entry);
#endif
void ht_pos_insert(struct hash_table *,
                   const struct position *,
                   ht_entry);
void ht_clean_up_after_move(const struct position*, move);
void ht_extract_pv(const struct hash_table *,
                   const struct position *,
                   int depth,
                   move pv[]);
void ht_clear(const struct hash_table *);
void ht_swap(struct hash_table *);
int ht_usage(const struct hash_table *);

#endif

