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

#include "z_random.inc"

#include "zobrist.h"

uint64_t
position_polyglot_key(const struct position *pos)
{
	uint64_t key = UINT64_C(0);

	/*
	 * Taltos an polyglot have different table representations,
	 * while Taltos uses the 64 bit constants also used by Polyglot.
	 * Hence using here (7-row) and (7-file) for
	 * indexing the z_random array.
	 */
	for (int row = 0; row < 8; ++row) {
		for (int file = 0; file < 8; ++file) {
			int index = ind(row, file);
			enum piece p = position_piece_at(pos, index);
			enum player pl = position_player_at(pos, index);

			if (p != nonpiece) {
				int polyglot_index = (7 - row) * 8 + (7 - file);
				key ^= zobrist_random[p + 1 - pl][polyglot_index];
			}
		}
	}

	if (position_cr_white_queen_side(pos))
		key = z_toggle_white_castle_queen_side(key);
	if (position_cr_white_king_side(pos))
		key = z_toggle_white_castle_king_side(key);
	if (position_cr_black_queen_side(pos))
		key = z_toggle_black_castle_queen_side(key);
	if (position_cr_black_king_side(pos))
		key = z_toggle_black_castle_king_side(key);

	if (position_has_en_passant_index(pos)) {
		int target = position_get_en_passant_index(pos);
		key = z_toggle_ep_file(key, 7 - ind_file(target));
	}

	if (position_turn(pos) == white)
		key ^= UINT64_C(0xF8D626AAAF278509);

	return key;
}
