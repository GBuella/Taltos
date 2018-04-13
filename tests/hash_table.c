/* vim: set filetype=c : */
/* vim: set noet tw=80 ts=8 sw=8 cinoptions=+4,(0,t0: */
/*
 * Copyright 2014-2018, Gabor Buella
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
#include "tt.h"
#include "position.h"

static struct tt_entry
set_entry1(void)
{
	struct tt_entry entry = tt_null();

	entry.depth = 3;
	entry.value = 77;
	entry.is_upper_bound = true;
	entry = tt_set_move(entry, mcastle_white_king_side());

	return entry;
}

static void
verify_entry1(struct tt_entry entry)
{
	assert(tt_entry_is_set(entry));
	assert(entry.depth == 3);
	assert(entry.is_upper_bound);
	assert(!entry.is_lower_bound);
	assert(entry.value == 77);
	assert(tt_has_move(entry));
	assert(move_eq(tt_move(entry), mcastle_white_king_side()));
}

static struct tt_entry
set_entry2(void)
{
	struct tt_entry entry = tt_null();

	entry.depth = 57;
	entry.is_lower_bound = true;
	entry.value = -1234;

	entry = tt_set_move(entry, mcastle_white_queen_side());

	return entry;
}

static void
verify_entry2(struct tt_entry entry)
{
	assert(tt_entry_is_set(entry));
	assert(entry.depth == 57);
	assert(entry.is_lower_bound);
	assert(!entry.is_upper_bound);
	assert(entry.value == -1234);
	assert(tt_has_move(entry));
	assert(move_eq(tt_move(entry), mcastle_white_queen_side()));
}

static const struct move m3 = {
	.from = a1,
	.to = a2,
	.result = rook,
	.captured = 0,
	.type = mt_general,
};

static struct tt_entry
set_entry3(void)
{
	struct tt_entry entry = tt_null();

	entry.depth = 50;
	entry.is_lower_bound = entry.is_upper_bound = true;
	entry.value = 3;
	entry = tt_set_move(entry, m3);

	return entry;
}

void
verify_entry3(struct tt_entry entry)
{
	assert_true(tt_entry_is_set(entry));
	assert_int(entry.depth, ==, 50);
	assert_true(tt_has_exact_value(entry));
	assert_int(entry.value, ==, 3);
	assert_true(tt_has_move(entry));
	assert_true(move_eq(tt_move(entry), m3));
}

static const struct move m4 = {
	.from = c2,
	.to = c3,
	.result = queen,
	.captured = 0,
	.type = mt_general,
};

static struct tt_entry
set_entry4(void)
{
	struct tt_entry entry = tt_null();

	entry.depth = 99;
	entry.is_lower_bound = true;
	entry.value = -33;
	entry = tt_set_move(entry, m4);

	return entry;
}

void
verify_entry4(struct tt_entry entry)
{
	assert_true(tt_entry_is_set(entry));
	assert_int(entry.depth, ==, 99);
	assert_true(entry.is_lower_bound);
	assert_false(entry.is_upper_bound);
	assert_int(entry.value, ==, -33);
	assert_true(tt_has_move(entry));
	assert_true(move_eq(tt_move(entry), m4));
}

void
run_tests(void)
{
	struct tt_entry entry;
	struct move pv[16];
	struct position pos1, pos2, pos3;

	struct tt *table = tt_create(6);
	assert(table != NULL);

	position_read_fen("4k3/8/8/8/8/8/8/R3K2R w KQ - 0 1", &pos1);
	position_read_fen("4k3/pppppppp/8/8/8/8/2Q5/R3K2R w KQ - 0 1", &pos2);

	assert_int64(pos1.zhash, !=, pos2.zhash);
	assert_int(tt_usage(table), ==, 0);
	// 16 bytes in a slot, 16 slots in a bucket
	assert_int64(tt_size(table), ==, (8 * 16) * (1 << 6));

	assert_false(tt_entry_is_set(tt_lookup(table, &pos1)));
	assert_false(tt_entry_is_set(tt_lookup(table, &pos2)));

	tt_extract_pv(table, &pos1, 16, pv, 43);
	assert_true(is_null_move(pv[0]));

	entry = set_entry1();
	verify_entry1(entry);
	tt_pos_insert(table, &pos1, entry);
	assert_int64(tt_usage(table), ==, 1);
	verify_entry1(tt_lookup(table, &pos1));

	tt_extract_pv(table, &pos1, 4, pv, 987);
	assert_true(is_null_move(pv[0]));
	tt_extract_pv(table, &pos1, 3, pv, 0);
	assert_true(is_null_move(pv[0]));

	entry = set_entry2();
	verify_entry2(entry);
	tt_pos_insert(table, &pos2, entry);
	assert_int(tt_usage(table), ==, 2);
	verify_entry2(tt_lookup(table, &pos2));
	verify_entry1(tt_lookup(table, &pos1));
	tt_extract_pv(table, &pos2, 59, pv, 0);
	assert_true(is_null_move(pv[0]));
	tt_extract_pv(table, &pos2, 2, pv, 0);
	assert_true(is_null_move(pv[0]));

	pos3 = pos2;
	pos3.zhash = 123456;
	entry = set_entry3();
	tt_pos_insert(table, &pos3, entry);
	assert_int(tt_usage(table), ==, 3);
	verify_entry3(tt_lookup(table, &pos3));
	verify_entry2(tt_lookup(table, &pos2));
	tt_extract_pv(table, &pos2, 59, pv, 0);
	assert_true(is_null_move(pv[0]));
	tt_extract_pv(table, &pos3, 22, pv, 3);
	assert_true(move_eq(pv[0], tt_move(entry)));
	assert_true(is_null_move(pv[1]));
	tt_extract_pv(table, &pos3, 22, pv, 4);
	assert_true(is_null_move(pv[0]));

	entry = set_entry4();
	verify_entry4(entry);
	tt_pos_insert(table, &pos2, entry);
	assert_int(tt_usage(table), ==, 4);
	verify_entry2(tt_lookup(table, &pos2));

	tt_destroy(table);
}
