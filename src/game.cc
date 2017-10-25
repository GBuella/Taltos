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

#include "chess.h"
#include "move.h"
#include "game.h"
#include "util.h"

using std::unique_ptr;
using std::string;

namespace taltos
{

game::entry::entry(string FEN):
	game_state(FEN),
	move_to_next(null_move)
{
}

game::entry::entry(const entry& parent, move m):
	game_state(parent, m),
	move_to_next(m)
{
}

game::game(string FEN)
{
	entries.push_back(FEN);
	current = entries.begin();
}

string game::print_FEN() const
{
	return current->to_FEN();
}

std::list<game::entry>::iterator game::next() const
{
	auto it = current;
	it++;
	return it;
}

std::list<game::entry> game::append_to_list(move m)
{
	std::list<entry> new_list(entries.begin(), next());

	new_list.back().move_to_next = m;
	new_list.emplace_back(new_list.back(), m);

	return new_list;
}

void game::append(move m)
{
	if (m == null_move)
		throw std::logic_error("Invalid move");

	if (current->move_to_next == m) {
		current++;
		return;
	}

	entries = append_to_list(m);
	current = entries.end();
	current--;
}

void game::truncate()
{
	auto next = current;
	next++;
	entries.erase(next, entries.end());
	current->move_to_next = null_move;
}

const position* game::current_position() const
{
	return current->get_pos();
}

void game::revert()
{
	if (current != entries.begin())
		current--;
}

void game::forward()
{
	if (next() != entries.end())
		current++;
}

move game::move_to_next() const
{
	return current->move_to_next;
}

size_t game::full_move_count() const
{
	return current->full_move;
}

size_t game::half_move_count() const
{
	return current->half_move;
}

bool game::is_draw() const
{
	return is_stalemate()
	    or is_draw_by_insufficient_material()
	    or is_draw_by_repetition()
	    or is_draw_by_50_move_rule();
}

bool game::is_ended() const
{
	return is_checkmate() or is_draw();
}

bool game::is_checkmate() const
{
	return current->is_checkmate();
}

bool game::is_stalemate() const
{
	return current->is_stalemate();
}

bool game::is_draw_by_insufficient_material() const
{
	return current->has_insufficient_material();
}

bool game::is_draw_by_repetition() const
{
	unsigned repetitions = 1;

	for (auto& e : entries) {
		if (pos_equal(e.get_pos(), current_position()))
			repetitions++;
	}

	return repetitions == 3;
}

bool game::is_draw_by_50_move_rule() const
{
	return half_move_count() >= 100;
}

move game::get_single_response() const
{
	auto moves = pos_moves(current_position());
	if (moves.size() != 1)
		throw std::logic_error("Logic error!");

	return moves[0];
}

bool game::has_single_response() const
{
	return current->has_single_response() and not is_ended();
}

unsigned game::length() const
{
	return (unsigned)entries.size();
}

game::~game()
{
}

}
