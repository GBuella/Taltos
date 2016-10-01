
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=+4,(4: */

#include "tests.h"

#include "position.h"

static const struct {
	const char *FEN;
	const uint64_t polyglot_key;
} data[] = {
{"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
	UINT64_C(0x463b96181691fc9c)},
{"rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1",
	UINT64_C(0x823c9b50fd114196)},
{"rnbqkbnr/ppp1pppp/8/3p4/4P3/8/PPPP1PPP/RNBQKBNR w KQkq d6 0 2",
	UINT64_C(0x0756b94461c50fb0)},
{"rnbqkbnr/ppp1pppp/8/3pP3/8/8/PPPP1PPP/RNBQKBNR b KQkq - 0 2",
	UINT64_C(0x662fafb965db29d4)},
{"rnbqkbnr/ppp1p1pp/8/3pPp2/8/8/PPPP1PPP/RNBQKBNR w KQkq f6 0 3",
	UINT64_C(0x22a48b5a8e47ff78)},
{"rnbqkbnr/ppp1p1pp/8/3pPp2/8/8/PPPPKPPP/RNBQ1BNR b kq - 0 3",
	UINT64_C(0x652a607ca3f242c1)},
{"rnbq1bnr/ppp1pkpp/8/3pPp2/8/8/PPPPKPPP/RNBQ1BNR w - - 0 4",
	UINT64_C(0x00fdd303c946bdd9)},
{"rnbqkbnr/p1pppppp/8/8/PpP4P/8/1P1PPPP1/RNBQKBNR b KQkq c3 0 3",
	UINT64_C(0x3c8123ea7b067637)},
{"rnbqkbnr/p1pppppp/8/8/P6P/R1p5/1P1PPPP1/1NBQKBNR b Kkq - 0 4",
	UINT64_C(0x5c3f9b829b279560)}
};

void
run_tests(void)
{
	struct position position[1];

	for (size_t i = 0; i < ARRAY_LENGTH(data); ++i) {
		enum player turn;

		position_read_fen(position, data[i].FEN, NULL, &turn);
		assert(position_polyglot_key(position, turn)
		    == data[i].polyglot_key);
	}
}
