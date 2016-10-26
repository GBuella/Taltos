
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=+4,(4: */

#include <assert.h>
#include <string.h>

#include "search.h"
#include "eval.h"
#include "move_order.h"


#define USED_MOVE 0xffff

void
move_fsm_setup(struct move_fsm *fsm, const struct position *pos,
		bool is_qsearch)
{
	if (is_qsearch)
		fsm->count = gen_captures(pos, fsm->moves);
	else
		fsm->count = gen_moves(pos, fsm->moves);
	fsm->index = 0;
	fsm->is_in_late_move_phase = false;
	fsm->very_late_moves_begin = fsm->count;
	fsm->hash_moves[0] = 0;
	fsm->hash_moves[1] = 0;
	fsm->hash_move_count = 0;
	fsm->has_hashmove = false;
	fsm->killerm_end = fsm->count;
	fsm->already_ordered = false;
}

static void
swap_moves(struct move_fsm *fsm, unsigned a, unsigned b)
{
	move m = fsm->moves[a];
	fsm->moves[a] = fsm->moves[b];
	fsm->moves[b] = m;
}

static bool
is_killer(const struct move_fsm *fsm, unsigned i)
{
	for (unsigned ki = 0; ki < ARRAY_LENGTH(fsm->killers); ++ki) {
		if (fsm->moves[i] == fsm->killers[ki])
			return true;
	}
	return false;
}

static bool
is_vlate_move(const struct position *pos, move m,
		uint64_t krook, uint64_t kbishop,
		uint64_t defensless)
{
	uint64_t from = mfrom64(m);
	uint64_t to = mto64(m);

	if (is_nonempty(pos->attack[opponent_pawn] & from)
	    && is_empty(pos->attack[opponent_pawn] & to))
		return false; // attacked by pawn, fleeing
	if (is_nonempty(pos->attack[1] & from)
	    && is_empty(pos->attack[0] & from))
		return false; // attacked by any piece, not defended
	if (is_nonempty(pos->attack[opponent_pawn] & to)) {
		if (mresultp(m) != pawn)	// a nonpawn piece moving to a
			return true;	// square attacked by opponent's pawn
	}
	if (mresultp(m) == knight) {
		if (is_nonempty(pos->attack[1] & to)
		    && is_empty(pos->attack[pawn] & to))
			return true;

		if (is_nonempty(knight_pattern(mto(m)) &
		    (pos->map[opponent_king]
		    | pos->map[opponent_rook]
		    | pos->map[opponent_queen]
		    | pos->map[opponent_bishop]
		    | defensless)))
			return false;

		if (is_nonempty(from & pos->sliding_attacks[0] & ~EDGES))
			// possibly was blocking and intersting sliding attack
			return false;
	}
	else if (mresultp(m) == pawn) {
		if (is_nonempty(pos->attack[1] & to)
		    && is_empty(pos->attack[pawn] & to))
			return true;

		if (is_nonempty(pos->map[1] & pawn_attacks_player(to))) { //
			// a pawn moving to attack a piece which can't capture
			return false;
		}
	}
	else if (mresultp(m) == king) {
		if (is_nonempty(king_moves_table[mto(m)] & defensless)) {
			// king threatening a defenless piece, might
			// be useful during the end-game
			return false;
		}
	}
	else if (is_empty(to & pos->attack[1])) {
		// sliding piece possibly attacking opponent's king
		if (mresultp(m) == rook || mresultp(m) == queen) {
			if (is_empty(krook & from) && is_nonempty(krook & to))
				return false;
		}
		if (mresultp(m) == bishop || mresultp(m) == queen) {
			if (is_empty(kbishop & from)
			    && is_nonempty(kbishop & to))
				return false;
		}
	}

	// nothing interesting found, move is eligible for aggressive pruning
	return true;
}

static void
prepare_killers_and_vlates(const struct position *pos, struct move_fsm *fsm)
{
	uint64_t krook = rook_pattern_table[bsf(pos->map[opponent_king])];
	uint64_t kbishop = bishop_pattern_table[bsf(pos->map[opponent_king])];
	uint64_t defensless = pos->map[1] & ~pos->attack[1];
	fsm->killerm_end = fsm->index;
	for (unsigned i = fsm->index; i < fsm->very_late_moves_begin; ++i) {
		if (is_killer(fsm, i))
			swap_moves(fsm, i, fsm->killerm_end++);
		else if (is_vlate_move(pos, fsm->moves[i], krook,
		    kbishop, defensless))
			swap_moves(fsm, i, --(fsm->very_late_moves_begin));
	}
}

static int
eval_capture(const struct position *pos, move m)
{
	int value = piece_value[mcapturedp(m)]
	    + pawn_value - piece_value[mresultp(m)] / 10;

	if (mtype(m) == mt_promotion)
		value += queen_value - pawn_value;
	else if (is_nonempty(mto64(m) & pos->attack[opponent_pawn]))
		value -= piece_value[mresultp(m)] / 2;

	return value;
}

static void
capture_bubble_down(struct move_fsm *fsm, move m, unsigned i, int value)
{
	while ((i > fsm->index) && (fsm->values[i - 1] < value)) {
		fsm->moves[i] = fsm->moves[i - 1];
		fsm->values[i] = fsm->values[i - 1];
		--i;
	}
	fsm->moves[i] = m;
	fsm->values[i] = value;
}

static bool
is_knight_fork(const struct position *pos, move m)
{
	if (mresultp(m) != knight)
		return false;

	if (is_empty(pos->attack[1] & mto64(m))) {
		if (popcnt(knight_pattern(mto(m)) &
		    (pos->map[opponent_king]
		    | pos->map[opponent_rook]
		    | pos->map[opponent_queen]
		    | pos->map[opponent_bishop])) > 1)
		return true;
	}

	return false;
}

static bool
is_bishop_threat(const struct position *pos, move m)
{
	if (mresultp(m) != bishop)
		return false;

	uint64_t attacks = sliding_map(pos->occupied, bishop_magics + mto(m));
	attacks &= ~pos->attack[bishop];
	if (is_nonempty(attacks & pos->attack[1] &
	    (pos->map[opponent_rook]
	    | pos->map[opponent_king]
	    | (pos->map[opponent_knight] & ~pos->attack[1]))))
		return true;

	if (is_nonempty(attacks & pos->map[opponent_queen])) {
		if (is_nonempty(mto64(m) &
		    (pos->attack[pawn]
		    | pos->attack[rook]
		    | pos->attack[knight]
		    | pos->attack[queen])))
			return true;
	}

	return false;
}

static bool
is_pawn_threat(const struct position *pos, move m)
{
	if (mresultp(m) != pawn)
		return false;

	uint64_t to = mto64(m);
	if (is_nonempty(to & (RANK_5 | RANK_6 | RANK_7))) {
		uint64_t blockers = north_of(to) | north_of(north_of(to));
		blockers &= pos->map[1];
		if (is_empty(blockers) &&
		    is_empty(to & pos->pawn_attack_reach[1]))
			return true;
	}

	if (is_nonempty(pawn_attacks_player(to)
	    & (pos->map[opponent_king] | pos->map[opponent_rook])))
		return true;

	return false;
}

static void
prepare_tacticals(const struct position *pos, struct move_fsm *fsm)
{
	fsm->tacticals_end = fsm->index;
	for (unsigned i = fsm->index; i < fsm->count; ++i) {
		move m = fsm->moves[i];
		if (mcapturedp(m) != 0
		    || (mtype(m) == mt_promotion && mresultp(m) == queen)) {
			fsm->moves[i] = fsm->moves[fsm->tacticals_end];
			capture_bubble_down(fsm, m,
			    fsm->tacticals_end++,
			    eval_capture(pos, m));
		}
		else if (is_knight_fork(pos, m)
		    || is_bishop_threat(pos, m)
		    || is_pawn_threat(pos, m)) {
			fsm->moves[i] = fsm->moves[fsm->tacticals_end];
			capture_bubble_down(fsm, m,
			    fsm->tacticals_end++,
			    pawn_value);
		}
	}
}

move
select_next_move(const struct position *pos, struct move_fsm *fsm)
{
	if (!fsm->already_ordered) {
		if (fsm->index == (fsm->has_hashmove ? 1 : 0))
			prepare_tacticals(pos, fsm);
		if (fsm->index == fsm->tacticals_end)
			prepare_killers_and_vlates(pos, fsm);
	}

	if (fsm->index == fsm->killerm_end)
		fsm->is_in_late_move_phase = true;

	return fsm->moves[fsm->index++];
}

void
move_fsm_reset(const struct position *pos, struct move_fsm *fsm, unsigned i)
{
	while (!move_fsm_done(fsm))
		(void) select_next_move(pos, fsm);

	fsm->index = i;
	fsm->already_ordered = true;
	fsm->is_in_late_move_phase = (i >= fsm->killerm_end);
}
