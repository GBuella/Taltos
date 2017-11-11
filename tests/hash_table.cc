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

#include "tests.h"
#include "hash.h"
#include "position.h"

namespace taltos::test
{

static ht_entry
set_entry1(void)
{
	ht_entry entry = HT_NULL;

	entry = ht_set_depth(entry, 3);
	entry = ht_set_value(entry, vt_upper_bound, 77);
	entry = ht_set_move(entry, mcastle_king_side);

	return entry;
}

static void
verify_entry1(ht_entry entry)
{
	assert(ht_is_set(entry));
	assert(ht_depth(entry) == 3);
	assert(ht_value_type(entry) == vt_upper_bound);
	assert(ht_value(entry) == 77);
	assert(ht_has_move(entry));
	assert(ht_move(entry) == mcastle_king_side);
}

static ht_entry
set_entry2(void)
{
	ht_entry entry = HT_NULL;

	entry = ht_set_depth(entry, 57);
	entry = ht_set_value(entry, vt_lower_bound, -1234);
	entry = ht_set_move(entry, mcastle_queen_side);

	return entry;
}

static void
verify_entry2(ht_entry entry)
{
	assert(ht_is_set(entry));
	assert(ht_depth(entry) == 57);
	assert(ht_value_type(entry) == vt_lower_bound);
	assert(ht_value(entry) == -1234);
	assert(ht_has_move(entry));
	assert(ht_move(entry) == mcastle_queen_side);
}

static ht_entry
set_entry3(void)
{
	ht_entry entry = HT_NULL;

	entry = ht_set_depth(entry, 59);
	entry = ht_set_value(entry, vt_exact, 3);
	entry = ht_set_move(entry, create_move_g(a1, a2, rook, 0));

	return entry;
}

void
verify_entry3(ht_entry entry)
{
	assert(ht_is_set(entry));
	assert(ht_depth(entry) == 59);
	assert(ht_value_type(entry) == vt_exact);
	assert(ht_value(entry) == 3);
	assert(ht_has_move(entry));
	assert(ht_move(entry) == create_move_g(a1, a2, rook, 0));
}

static ht_entry
set_entry4(void)
{
	ht_entry entry = HT_NULL;

	entry = ht_set_depth(entry, 99);
	entry = ht_set_value(entry, vt_lower_bound, -33);
	entry = ht_set_move(entry, create_move_g(c2, c3, queen, 0));

	return entry;
}

void
verify_entry4(ht_entry entry)
{
	assert(ht_is_set(entry));
	assert(ht_depth(entry) == 99);
	assert(ht_value_type(entry) == vt_lower_bound);
	assert(ht_value(entry) == -33);
	assert(ht_has_move(entry));
	assert(ht_move(entry) == create_move_g(c2, c3, queen, 0));
}

void
run_tests(void)
{
	ht_entry entry;
	move pv[16];
	struct position pos1, pos2, pos3;

	struct hash_table *table = ht_create(6);
	position_read_fen(&pos1, "4k3/8/8/8/8/8/8/R3K2R w KQ - 0 1",
	    NULL, NULL);
	position_read_fen(&pos2, "4k3/pppppppp/8/8/8/8/2Q5/R3K2R w KQ - 0 1",
	    NULL, NULL);

	assert(pos1.zhash[0] != pos2.zhash[0]);
	assert(pos1.zhash[1] != pos2.zhash[1]);
	assert(table != NULL);
	assert(ht_usage(table) == 0);
	// 16 bytes in a slot, 16 slots in a bucket
	assert(ht_size(table) == (8 * 16) * (1 << 6));

	assert(!ht_is_set(ht_lookup_deep(table, &pos1, 3, 0)));
	assert(!ht_is_set(ht_lookup_deep(table, &pos2, 3, 0)));
	assert(!ht_is_set(ht_lookup_fresh(table, &pos1)));
	assert(!ht_is_set(ht_lookup_fresh(table, &pos2)));

	ht_extract_pv(table, &pos1, 16, pv, 43);
	assert(pv[0] == 0);

	entry = set_entry1();
	verify_entry1(entry);
	ht_pos_insert(table, &pos1, entry);
	assert(ht_usage(table) == 1);
	verify_entry1(ht_lookup_deep(table, &pos1, 3, 0));
	verify_entry1(ht_lookup_fresh(table, &pos1));
	assert(!ht_is_set(ht_lookup_deep(table, &pos2, 3, 0)));

	ht_extract_pv(table, &pos1, 4, pv, 987);
	assert(pv[0] == 0);
	ht_extract_pv(table, &pos1, 3, pv, 0);
	assert(pv[0] == 0);


	entry = set_entry2();
	verify_entry2(entry);
	ht_pos_insert(table, &pos2, entry);
	assert(ht_usage(table) == 2);
	verify_entry2(ht_lookup_deep(table, &pos2, 3, 0));
	verify_entry1(ht_lookup_deep(table, &pos1, 3, 0));
	verify_entry2(ht_lookup_fresh(table, &pos2));
	ht_extract_pv(table, &pos2, 59, pv, 0);
	assert(pv[0] == 0);
	ht_extract_pv(table, &pos2, 2, pv, 0);
	assert(pv[0] == 0);

	/*
	 * multiple slots used in the same bucket
	 * same zhash[0] ( used for indexing )
	 * differenet zhash[1]
	 */
	pos3 = pos2;
	pos3.zhash[1] = 123456;
	entry = set_entry3();
	ht_pos_insert(table, &pos3, entry);
	assert(ht_usage(table) == 3);
	verify_entry3(ht_lookup_deep(table, &pos3, 3, 0));
	verify_entry2(ht_lookup_deep(table, &pos2, 3, 0));
	verify_entry3(ht_lookup_fresh(table, &pos3));
	ht_extract_pv(table, &pos2, 59, pv, 0);
	assert(pv[0] == 0);
	ht_extract_pv(table, &pos3, 22, pv, 3);
	assert(pv[0] == ht_move(entry));
	assert(pv[1] == 0);
	ht_extract_pv(table, &pos3, 22, pv, 4);
	assert(pv[0] == 0);

	entry = set_entry4();
	verify_entry4(entry);
	ht_pos_insert(table, &pos2, entry);
	assert(ht_usage(table) == 3);
	verify_entry4(ht_lookup_deep(table, &pos2, 90, 0));
	verify_entry4(ht_lookup_deep(table, &pos2, 3, 0));
	verify_entry4(ht_lookup_fresh(table, &pos2));
	ht_pos_insert(table, &pos2, set_entry3());
	verify_entry4(ht_lookup_deep(table, &pos2, 90, 0));
	verify_entry3(ht_lookup_deep(table, &pos2, 3, 0));
	verify_entry3(ht_lookup_fresh(table, &pos2));

	ht_destroy(table);
}

}
