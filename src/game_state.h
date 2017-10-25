/* vim: set filetype=cpp : */
/* vim: set noet tw=100 ts=8 sw=8 cinoptions=+4,(0,t0: */
/*
 * Copyright 2017, Gabor Buella
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

#ifndef TALTOS_GAME_STATE_H
#define TALTOS_GAME_STATE_H

#include "position.h"

#include <memory>
#include <string>

namespace taltos
{

extern const std::string initial_FEN;

enum move_notation_type {
	mn_coordinate,
	mn_san,
	mn_fan
};

enum player_color {
	white = 0,
	black = 1
};

class game_state : protected position
{
	std::vector<move> moves;
public:
	using position::operator new;
	using position::operator new[];
	using position::operator delete;
	using position::operator delete[];
	using position::is_in_check;
	using position::draw_with_insufficient_material;

	const unsigned full_move;
	const unsigned half_move;
	enum player_color turn;

	game_state(std::string FEN = initial_FEN);
	game_state(const game_state& parent, move);
	std::string to_FEN() const;
	bool is_checkmate() const;
	bool is_stalemate() const;
	bool has_any_legal_move() const;
	bool has_single_response() const;
	const position *get_pos() const;
	std::string print_SAN(move) const;
	std::string print_coor(move) const;
	std::string print_move(move, move_notation_type) const;
};

}

#endif
