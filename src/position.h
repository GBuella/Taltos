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

#ifndef TALTOS_POSITION_H
#define TALTOS_POSITION_H

namespace taltos
{

/*
 * piece_array_size - the length of an array that contains one item
 * for each player, followed by one for piece type of each player.
 * At index 0 is an item corresponding to the player next to move,
 * while at index 1 is the similar item corresponding to the opponent.
 * At indices 2, 4, 6, 8, 10, 12 are items corresponding to the players
 * pieces, and at odd indices starting at three are the opponent's items.
 * Such an array can be indexed by an index representing a player ( 0 or 1),
 * or by a piece, e.g. postion.map[pawn] is the map of pawns,
 * position.map[pawn + 1] == position.map[opponent_pawn] is the map of the
 * opponent's pawns.
 */
constexpr size_t piece_array_size = 14;

// make sure the piece emumeration values can be used for indexing such an array
static_assert(pawn >= 2 and pawn < piece_array_size);
static_assert(king >= 2 and king < piece_array_size);
static_assert(knight >= 2 and knight < piece_array_size);
static_assert(rook >= 2 and rook < piece_array_size);
static_assert(bishop >= 2 and bishop < piece_array_size);
static_assert(queen >= 2 and queen < piece_array_size);

class alignas(64) position {
public:
	static void* operator new(size_t);
	static void* operator new[](size_t);
	static void operator delete(void*);
	static void operator delete[](void*);
private:

	std::array<unsigned char, 64> board;

	/*
	 * In case the player to move is in check:
	 * A bitboard of all pieces that attack the king, and the squares of any
	 * sliding attack between the king and the attacking rook/queen/bishop.
	 * These extra squares are valid move destinations for blocking the
	 * attack, thus moving out of check. E.g. a rook attacking the king:
	 *
	 *    ........
	 *    ..r..K..
	 *    ........
	 *
	 *  The corresponding part of the king_attack_map:
	 *
	 *    ........
	 *    ..XXX...
	 *    ........
	 */
	bitboard king_attack_map;
	bitboard king_danger_map;

	bitboard padding;

	/*
	 * Index of a pawn that can be captured en passant.
	 * If there is no such pawn, ep_index is zero.
	 */
	uint64_t ep_index;

	// A bitboard of all pieces
	bitboard occupied_map;

	// The square of the king belonging to the player to move
	int32_t ki;
	int32_t opp_ki;

	/*
	 * The bitboards attack[0] and attack[1] contain maps all squares
	 * attack by each side. Attack maps of each piece type for each side
	 * start from attack[2] --- to be indexed by piece type.
	 */
	std::array<bitboard, piece_array_size> attack;

	/*
	 * All sliding attacks ( attacks by bishop, rook, or queen ) of
	 * each player.
	 */
	std::array<bitboard, 2> sliding_attacks;

	/*
	 * The map[0] and map[1] bitboards contain maps of each players
	 * pieces, the rest contain maps of individual pieces.
	 */
	std::array<bitboard, piece_array_size> map;

	/*
	 * Each square of every file left half open by players pawns,
	 * i.e. where player has no pawn, and another bitboard for
	 * those files where the opponent has no pawns.
	 */
	std::array<bitboard, 2> half_open_files_map;

	/*
	 * Each square that can be attacked by pawns, if they are pushed
	 * forward, per player.
	 */
	std::array<bitboard, 2> pawn_attack_reach_map;

	/*
	 * Thus given a bitboard of a players pawns:
	 *
	 * ........
	 * ........
	 * .......1
	 * ........
	 * ..11....
	 * .....1..
	 * 1.......
	 * ........
	 *
	 * half_open_files_map:
	 *
	 * .1....1.
	 * .1....1.
	 * .1....1.
	 * .1....1.
	 * .1....1.
	 * .1....1.
	 * .1....1.
	 * .1....1.
	 *
	 * pawn_attack_reach_map:
	 *
	 * .1111.1.
	 * .1111.1.
	 * .1111.1.
	 * .1111.1.
	 * .1..1.1.
	 * .1......
	 * ........
	 * ........
	 */

	alignas(64)
	std::array<bitboard, 2> rq; // map[rook] | map[queen]
	std::array<bitboard, 2> bq; // map[bishop] | map[queen]

	alignas(64)
	std::array<bitboard, 64> rook_rays;
	std::array<bitboard, 64> bishop_rays;

	alignas(64)
	/*
	 * The following four 64 bit contain two symmetric pairs, that can be
	 * swapped in make_move, as in:
	 *
	 * new->zhash[0] = old->zhash[1]
	 * new->zhash[1] = old->zhash[0]
	 * new->cr_and_material[0] = old->cr_and_material[1]
	 * new->cr_and_material[1] = old->cr_and_material[0]
	 */

	/*
	 * The zobrist hash key of the position is in zhash[0], while
	 * the key from the opponent's point of view is in zhash[1].
	 * During make_move these two are exchanged with each, and both
	 * are updated. This way, there is no need for information on the
	 * current side to move in the hash - actually in the whole
	 * position structure - and can recognize transpositions that
	 * appear with opposite players.
	 */
	std::array<uint64_t, 2> zhash;

	/*
	 * Two booleans corresponding to castling rights, and the sum of piece
	 * values - updated on each move - stored for one playes in 64 bits.
	 * The next 64 bits answer the same questions about the opposing side.
	 */
	int8_t cr_king_side;
	int8_t cr_queen_side;
	int8_t cr_padding0[2];
	int32_t material_value;

	int8_t cr_opponent_king_side;
	int8_t cr_opponent_queen_side;
	int8_t cr_padding1[2];
	int32_t opponent_material_value;

	// pinned pieces
	std::array<bitboard, 2> king_pins;

	// knights and bishops
	std::array<bitboard, 2> nb;

	// each players pieces not defended by other pieces of the same player
	std::array<bitboard, 2> undefended;

	bitboard all_rq;
	bitboard all_bq;

	alignas(64)
	std::array<uint8_t, 64> hanging;
	bitboard hanging_map;

public:
	bitboard pieces() const
	{
		return map[0];
	}

	bitboard opponent_pieces() const
	{
		return map[1];
	}

	bitboard occupied() const
	{
		return occupied_map;
	}

	bitboard pawns() const
	{
		return map[pawn];
	}

	bitboard opponent_pawns() const
	{
		return map[opponent_pawn];
	}

	bitboard knights() const
	{
		return map[knight];
	}

	bitboard opponent_knights() const
	{
		return map[opponent_knight];
	}

	bitboard queens() const
	{
		return map[queen];
	}

	bitboard opponent_queens() const
	{
		return map[opponent_queen];
	}

	bitboard rooks() const
	{
		return map[rook];
	}

	bitboard opponent_rooks() const
	{
		return map[opponent_rook];
	}

	bitboard bishops() const
	{
		return map[bishop];
	}

	bitboard opponent_bishops() const
	{
		return map[opponent_bishop];
	}

	bitboard all_pawns() const
	{
		return map[pawn] | map[opponent_pawn];
	}

	bitboard all_knights() const
	{
		return map[knight] | map[opponent_knight];
	}

	bitboard all_bishops() const
	{
		return map[bishop] | map[opponent_bishop];
	}

	bitboard all_kings() const
	{
		return map[king] | map[opponent_king];
	}

	bitboard pawn_attacks() const
	{
		return attacks[pawn];
	}

	bitboard opponent_pawn_attacks() const
	{
		return attacks[opponent_pawn];
	}

	bitboard half_open_files(int side) const
	{
		return half_open_files_map[side];
	}

	bitboard pawn_attack_reach(int side) const
	{
		return pawn_attack_reach_map[side];
	}

	bitboard rook_reach(int index) const
	{
		invariant(is_valid_index(index));
		return rook_rays[index];
	}

	bitboard bishop_reach(int index) const
	{
		invariant(is_valid_index(index));
		return bishop_rays[index];
	}

	bitboard diag_reach(int index) const
	{
		invariant(is_valid_index(index));
		return bishop_rays[index] & diag_masks[index];
	}

	bitboard adiag_reach(int index) const
	{
		invariant(is_valid_index(index));
		return bishop_rays[index] & adiag_masks[index];
	}

	bitboard hor_reach(int index) const
	{
		invariant(is_valid_index(index));
		return rook_rays[index] & hor_masks[index];
	}

	bitboard ver_reach(int index) const
	{
		invariant(is_valid_index(index));
		return rook_rays[index] & ver_masks[index];
	}

	bitboard king_attackers() const
	{
		return king_attack_map & map[1];
	}

	bitboard absolute_pins(int side) const
	{
		return king_pins[side] & map[side];
	}

	int en_passant_file() const
	{
		return ind_file(ep_index);
	}

	bool has_ep_target() const
	{
		return ep_index != 0;
	}

	uint64_t hash_key() const
	{
		uint64_t key = zhash[0];
		if (has_ep_target())
			key = z_toggle_ep_file(key, en_passant_file());
		return key;
	}

	bool is_in_check() const
	{
		return not king_attack_map.is_empty();
	}

	bool is_draw_with_insufficient_material() const;
	bool is_move_irreversible(move) const;

	int piece_at(int index) const
	{
		invariant(is_valid_index(index));
		return board[index];
	}

	int side_at(int index) const
	{
		invariant(is_valid_index(index));
		if (map[1].is_set(index))
			return 1;
		else
			return 0;
	}

	int square_at(int index) const
	{
		return piece_at(index) + side_at(index);
	}

	bitboard map_of(int piece) const
	{
		return map[piece];
	}

	bitboard attacks_of(int piece) const
	{
		return attack[piece];
	}

	bitboard king_attackers() const
	{
		return king_attack_map & map[1];
	}

	bool can_castle_king_size() const
	{
		return cr_king_side;
	}

	bool can_castle_queen_size() const
	{
		return cr_queen_side;
	}

	bool can_opponent_castle_king_size() const
	{
		return cr_opponent_king_side;
	}

	bool can_castle_opponent_queen_size() const
	{
		return cr_opponent_queen_side;
	}

	bool draw_with_insufficient_material() const;
	bool is_move_irreversible(move) const;

	int non_pawn_material() const
	{
		return material_value - pawn_value * map[pawn].popcnt();
	}

	int opponent_non_pawn_material() const;
	{
		return opponent_material_value - pawn_value * map[opponent_pawn].popcnt();
	}

	bool operator ==(const position&) const;

	unsigned move_gen(move*) const;
	unsigned q_move_gen(move*) const;

protected:
	void reset(std::array<int, 64>& full_board);
	void set_ep_index(int index);
	void clear_ep_index();
	void set_cr_king_side();
	void clear_cr_king_side();
	void set_cr_queen_side();
	void clear_cr_queen_side();
	void set_cr_opponent_king_side();
	void clear_cr_opponent_king_side();
	void set_cr_opponent_queen_side();
	void clear_cr_opponent_queen_side();

	void make_move(const position& parent, move) noexcept;

private:
};

}

#endif
