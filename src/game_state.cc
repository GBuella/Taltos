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

#include "game_state.h"

#include <array>
#include <cctype>
#include <utility>
#include <sstream>

#include "str_util.h"

using std::string;
using std::array;
using std::unique_ptr;
using std::make_unique;

namespace taltos
{

const std::string initial_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

/*
 * Parsing/printing FEN strings:
 *
 * "rnbqkbnr/pp1ppppp/8/2p5/4P3/8/PPPP1PPP/RNBQKBNR   w   KQkq   c6   0   2"
 *      |                                             |   |      |    |   |
 *      parse_board                  parse_side_to_move   |      |    |   |
 *      print_board                  print_side_to_move   |      |    |   |
 *                                                        |      |    |   |
 *                                      parse_castle_rights      |    |   |
 *                                      print_castle_rights      |    |   |
 *                                                               |    |   |
 *                                                 parse_ep_target    |   |
 *                                                 print_ep_target    |   |
 *                                                                    |   |
 *                                                            half_move   |
 *                                                                        |
 *                                                                full_move
 */

static void parse_board(string str, array<int, 64>& full_board)
{
	int rank = rank_8;
	int file = file_a;
	bool rank_done = false;
	for (auto c : str) {
		if (c == '/') {
			if (rank == rank_1 or not rank_done)
				throw string("FEN syntax error");
			rank += rsouth;
			file = file_a;
		}
		else if (std::isdigit((unsigned char)c)) {
			int n = c - '0';
			if (n == 0)
				throw string("FEN syntax error");
			while (n > 0) {
				if (rank_done)
					throw string("FEN syntax error");

				full_board[ind(rank, file)] = 0;
				if (file == file_h)
					rank_done = true;
				else
					file += feast;
			}
		}
		else {
			full_board[ind(rank, file)] = char_to_piece(c);
			if (std::islower((unsigned char)c))
				full_board[ind(rank, file)] |= 1;
			if (file == file_h)
				rank_done = true;
			else
				file += feast;
		}
	}

	if (rank != rank_1 or not rank_done)
		throw string("FEN syntax error");
}

static void parse_side_to_move(string str, enum player& turn)
{
	if (str == "w" or str == "W")
		turn = white;
	else if (str == "b" or str == "B")
		turn = black;
	else
		throw string("FEN syntax error");
}

static void parse_castle_rights(string str, array<bool, 4>& rights)
{
	rights[0] = false;
	rights[1] = false;
	rights[2] = false;
	rights[3] = false;

	if (str == "-")
		return;

	for (auto c : str) {
		switch (c) {
			case 'K':
				if (rights[cri_king_side])
					throw string("FEN syntax error - castling rights");
				rights[cri_king_side] = true;
				break;
			case 'Q':
				if (rights[cri_queen_side])
					throw string("FEN syntax error - castling rights");
				rights[cri_queen_side] = true;
				break;
			case 'k':
				if (rights[cri_opponent_king_side])
					throw string("FEN syntax error - castling rights");
				rights[cri_opponent_king_side] = true;
				break;
			case 'q':
				if (rights[cri_opponent_queen_side])
					throw string("FEN syntax error - castling rights");
				rights[cri_opponent_queen_side] = true;
				break;
			default:
				throw string("FEN syntax error - castling rights");
		}
	}
}

static void parse_ep_target(string token, int turn, int& index)
{
	if (token == "-")
		index = 0;
	else if (token.size() == 2 and is_file(token[0]) and is_rank(token[1]))
		index = ind(char_to_rank(token[1], turn), char_to_file(token[0])) + south;
	else
		throw string("FEN syntax error - en passant square");
}

string next_FEN_token(std::stringstream& tokens)
{
	string token;

	tokens >> token;
	if (tokens.fail())
		throw string("FEN syntax error");

	return token;
}

static void
flip_full_board(array<int, 64>& board)
{
	for (int i = 0; i < 32; ++i) {
		char t = board[i];

		board[i] = board[flip_i(i)];
		board[flip_i(i)] = t;
	}
	for (int i = 0; i < 64; ++i) {
		if (board[i] != 0)
			board[i] ^= 1;
	}
}

static void flip_representation(array<int, 64>& board, array<bool, 4> castle_rights, int& ep_index)
{
	flip_full_board(board);

	std::swap(castle_rights[cri_king_side], castle_rights[cri_opponent_king_side]);
	std::swap(castle_rights[cri_queen_side], castle_rights[cri_opponent_queen_side]);

	if (ep_index != 0)
		ep_index = flip_i(ep_index);
}

game_state::game_state(string FEN)
{
	array<int, 64> full_board;
	array<bool, 4> castle_rights;

	std::stringstream tokens(FEN);

	parse_board(next_FEN_token(tokens), full_board);
	parse_side_to_move(next_FEN_token(tokens), turn);
	parse_castle_rights(next_FEN_token(tokens), castle_rights);
	parse_ep_target(next_FEN_token(tokens), turn, ep_target_index);

	if (turn == black)
		flip_representation(full_board, castle_rights, ep_target_index);

	position_reset(this, full_board, castle_rights, ep_target_index);

	tokens >> half_move;
	if (tokens.fail()) {
		half_move = 0;
		full_move = 1;
		return;
	}

	tokens >> full_move;
	if (tokens.fail()) {
		half_move = 0;
		full_move = 1;
	}
}

game_state::game_state(const game_state& parent, move m)
{
	make_move(this, &parent, m);
	if (is_move_irreversible(&parent, m))
		half_move = 0;
	else
		half_move++;

	if (parent.turn == white)  {
		turn = black;
		full_move = parent.full_move;
	}
	else {
		turn = black;
		full_move = parent.full_move + 1;
	}

	if (m.type == mt_pawn_double_push)
		ep_target_index = m.to;
	else
		ep_target_index = 0;
}

static string print_board(const struct position *pos, enum player turn)
{
	string str;
	int empty_count;

	for (int rank = rank_8; ; rank += rsouth) {
		empty_count = 0;
		for (int file = file_a; is_valid_file(file); file += feast) {
			int i = ind(rank, file);
			if (turn == black)
				i = flip_i(i);
			int p = position_piece_at(pos, i);
			int player = position_player_at(pos, i);
			if (p != nonpiece) {
				if (empty_count > 0) {
					str.push_back((char)('0' + empty_count));
					empty_count = 0;
				}
				if (turn == black)
					player = opponent_of(player);
				str.push_back(square_to_char(p, player));
			}
			else {
				++empty_count;
			}
		}
		if (empty_count > 0)
			str.push_back('0' + empty_count);
		if (rank == rank_1)
			return str;
		else
			str.push_back('/');
	}

	return str;
}

static string print_side_to_move(enum player turn)
{
	if (turn == white)
		return "w";
	else
		return "b";
}

static string print_castle_rights(const struct position *pos, enum player turn)
{
	string str;

	if (turn == white) {
		if (position_cr_king_side(pos))
			str.push_back('K');
		if (position_cr_queen_side(pos))
			str.push_back('Q');
		if (position_cr_opponent_king_side(pos))
			str.push_back('k');
		if (position_cr_opponent_queen_side(pos))
			str.push_back('q');
	}
	else {
		if (position_cr_opponent_king_side(pos))
			str.push_back('K');
		if (position_cr_opponent_queen_side(pos))
			str.push_back('Q');
		if (position_cr_king_side(pos))
			str.push_back('k');
		if (position_cr_queen_side(pos))
			str.push_back('q');
	}

	if (str.empty())
		str = "-";

	return str;
}

static string print_ep_target(int ep_target_index, enum player turn)
{
	if (ep_target_index == 0)
		return "-";
	else
		return print_index(ep_target_index + north, turn);
}

string game_state::to_FEN() const
{
	string result;

	result += print_board(this, turn);
	result += ' ';
	result += print_side_to_move(turn);
	result += ' ';
	result += print_castle_rights(this, turn);
	result += ' ';
	result += print_ep_target(ep_target_index, turn);
	result += ' ';
	result += std::to_string(half_move);
	result += ' ';
	result += std::to_string(full_move);

	return result;
}

bool game_state::is_in_check() const
{
	return taltos::is_in_check(this);
}

bool game_state::has_any_legal_move() const
{
	return pos_move_count(this) == 0;
}

bool game_state::has_insufficient_material() const
{
	return pos_has_insufficient_material(this);
}

bool game_state::has_single_response() const
{
	return pos_move_count(this) == 1;
}

bool game_state::is_checkmate() const
{
	return is_in_check() and not has_any_legal_move();
}

bool game_state::is_stalemate() const
{
	return is_in_check() and has_any_legal_move();
}

const position* game_state::get_pos() const
{
	return this;
}

string game_state::print_coor(move m) const
{
	string result;

	result += print_index(m.from, turn);
	result += print_index(m.to, turn);
	if (is_promotion(m))
		result += print_piece(m.result(), white);

	return result;
}

static char*
print_san_move_from(const struct position *pos, move m,
			char *str, enum player turn)
{
	string result;
	move moves[MOVE_ARRAY_LENGTH];
	uint64_t ambig_pieces = UINT64_C(0);
	enum piece p = pos_piece_at(pos, m.from);

	(void) gen_moves(pos, moves);
	for (move *im = moves; *im != 0; ++im) {
		if ((im->from != m.from) and (im-> == m.to)
		    and (pos_piece_at(pos, im->from) == p))
			ambig_pieces |= bb(im->from);
	}
	if ((p == pawn) and m.is_capture) {
		*(str++) = index_to_file_ch(mfrom(m));
	}
	else if (is_nonempty(ambig_pieces)) {
		if (is_nonempty(ambig_pieces & file64(mfrom(m)))) {
			if (is_nonempty(ambig_pieces & rank64(mfrom(m)))) {
				*(str++) = index_to_file_ch(mfrom(m));
			}
			*(str++) = index_to_rank_ch(mfrom(m), turn);
		}
		else {
			*(str++) = index_to_file_ch(mfrom(m));
		}
	}
	return str;
}

string game_state::print_SAN(move m) const
{
	string result;

	if (m.type == mt_castle_kingside)
		return "O-O";
	else if (m.type == mt_castle_queenside)
		return "O-O-O";

	int piece = pos_piece_at(pos, m.from);

	if (piece != pawn)
		result += print_piece(piece, turn);

	str = print_san_move_from(pos, m, str, turn);

	if (is_capture(m))
		*str++ = 'x';

	str = print_index(str, mto(m), turn);

	if (mtype(m) == mt_en_passant)
		return str + sprintf(str, "e.p.");

	str = print_san_promotion(m, str);
	str = print_san_check(pos, m, str);
	*str = '\0';
	return str;
}

}
