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

#ifndef TALTOS_GAME_H
#define TALTOS_GAME_H

#include "chess.h"
#include "position.h"
#include "move.h"
#include "macros.h"
#include "game_state.h"

#include <cstddef>
#include <memory>
#include <string>
#include <list>
#include <string>

namespace taltos
{

class game
{
protected:
	struct entry : public game_state {
		move move_to_next;

		entry(std::string FEN);
		entry(const entry& other, move);
	};

	std::list<entry> entries;
	std::list<entry>::iterator current;
	std::list<entry>::iterator next() const;

	std::list<entry> append_to_list(move);

public:
	game(std::string FEN);
	bool continues(const game& other) const;
	std::string print_FEN() const;
	void append(move);
	void truncate();
	const position* current_position() const;
	void revert();
	void forward();
	move move_to_next() const;
	size_t full_move_count() const;
	size_t half_move_count() const;
	bool is_ended() const;
	bool is_checkmate() const;
	bool is_stalemate() const;
	bool is_draw() const;
	bool is_draw_by_insufficient_material() const;
	bool is_draw_by_repetition() const;
	bool is_draw_by_50_move_rule() const;
	move get_single_response() const;
	bool has_single_response() const;
	unsigned length() const;
	~game();
};

}

#endif
