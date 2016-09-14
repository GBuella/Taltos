
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=+4,(4: */

#ifdef NDEBUG
#undef NDEBUG
#endif

#include <assert.h>
#include <string.h>

#include "tests.h"
#include "str_util.h"
#include "position.h"

static void
test_coordinates(void)
{
	assert(is_coordinate("g6"));
	assert(!is_coordinate("g9"));
	assert(!is_coordinate("g0"));
	assert(!is_coordinate("6"));
	assert(is_coordinate("g6 lorem ipsum"));
	assert(!is_coordinate("lorem ipsum"));

	assert(str_to_index("g6", white) == ind(rank_6, file_g));
	assert(str_to_index("g6", black) == flip_i(ind(rank_6, file_g)));
}

static bool
is_invalid(const char *fen)
{
	struct position pos;
	return position_read_fen(&pos, fen, NULL, NULL) == NULL;
}

static void
test_invalid_fens(void)
{
	assert(is_invalid(""));
	assert(is_invalid(" "));
	assert(is_invalid("asdfgh"));
	assert(is_invalid("8/8/8"));
	assert(is_invalid("8/8/8/8/8/8/8/8"));
	assert(is_invalid("8/8/8/8/8/8/8/8 w - -"));

	// no black king
	assert(is_invalid(
	    "rnbq1bnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -"));

	// no white king
	assert(is_invalid(
	    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQ1BNR w KQkq -"));

	// two black kings
	assert(is_invalid(
	    "rnbqkbnr/pppppppp/8/5k2/8/8/PPPPPPPP/RNBQKBNR w KQkq -"));

	// two white kings
	assert(is_invalid(
	    "rnbqkbnr/pppppppp/8/5K2/8/8/PPPPPPPP/RNBQKBNR w KQkq -"));

	// too many black bishops
	assert(is_invalid(
	    "rnbqkbnr/pppppppp/8/5b2/8/8/PPPP1PPP/RNBQKBNR w KQkq -"));

	// too many white queens
	assert(is_invalid(
	    "rnbqkbnr/pppppppp/8/4QQ2/8/8/PPPPPPP1/RNBQKBNR w KQkq -"));

	// kings attacking each other
	assert(is_invalid(
	    "rnbq1bnr/pppppppp/8/4kK2/8/8/PPPPPPP1/RNBQ1BNR w - -"));

	// invalid castling rights
	assert(is_invalid(
	    "1nbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w q -"));
	assert(is_invalid(
	    "rnbqkbn1/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w k -"));
	assert(is_invalid(
	    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/1NBQKBNR w Q -"));
	assert(is_invalid(
	    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBN1 w K -"));
	assert(is_invalid(
	    "rnbq1bnr/pppppppp/8/1k6/8/8/PPPPPPPP/RNBQKBNR w kq -"));
	assert(is_invalid(
	    "rnbqkbnr/pppppppp/8/1K6/8/8/PPPPPPPP/RNBQ1BNR w KQ -"));
}

static const struct {
	const char *FEN;
	const char *board;
} positions[] = {
	{"7k/8/8/1p6/P7/8/8/7K b - -",
	"       k"
	"        "
	"        "
	" p      "
	"P       "
	"        "
	"        "
	"       K"},
	{"rnbqkbnr/ppppppp1/8/5b2/5Q2/8/PPPP1PPP/RNBQKBNR w KQkq -",
	"rnbqkbnr"
	"ppppppp "
	"        "
	"     b  "
	"     Q  "
	"        "
	"PPPP PPP"
	"RNBQKBNR"},
	{"r2qkbnr/ppp1pppp/n7/3pP3/8/8/PPPP1PPP/RNBQK2R w KQkq d6",
	"r  qkbnr"
	"ppp pppp"
	"n       "
	"   pP   "
	"        "
	"        "
	"PPPP PPP"
	"RNBQK  R"},
	{"rnbq2nr/pppppppp/5k2/8/8/1P6/P1PPP1PP/RNBQK2R w KQ -",
	"rnbq  nr"
	"pppppppp"
	"     k  "
	"        "
	"        "
	" P      "
	"P PPP PP"
	"RNBQK  R"}
};

void
run_string_tests(void)
{
	struct position pos;
	enum player turn;
	int ep_index;
	char str[1024];
	const char *end;
	move m;

	test_coordinates();
	test_invalid_fens();

	end = position_read_fen(&pos, positions[0].FEN, &ep_index, &turn);
	assert(end != NULL);
	assert(ep_index == 0);
	assert(turn == black);
	assert(end == (positions[0].FEN + strlen(positions[0].FEN)));
	end = position_print_fen(&pos, str, 0, black);
	assert(strcmp(str, positions[0].FEN) == 0);
	assert(end == (str + strlen(positions[0].FEN)));
	assert(read_move(&pos, "h8h7", &m, black) == 0);
	assert(m == create_move_g(sq_h1, sq_h2, king, 0));
	print_move(&pos, m, str, mn_coordinate, black);
	assert(strcmp(str, "h8h7") == 0);
	assert(read_move(&pos, "h8g8", &m, black) == 0);
	assert(m == create_move_g(sq_h1, sq_g1, king, 0));
	print_move(&pos, m, str, mn_coordinate, black);
	assert(strcmp(str, "h8g8") == 0);
	assert(read_move(&pos, "b5b4", &m, black) == 0);
	assert(m == create_move_g(sq_b4, sq_b5, pawn, 0));
	print_move(&pos, m, str, mn_coordinate, black);
	assert(strcmp(str, "b5b4") == 0);
	assert(read_move(&pos, "b5a4", &m, black) == 0);
	assert(m == create_move_g(sq_b4, sq_a5, pawn, pawn));
	print_move(&pos, m, str, mn_coordinate, black);
	assert(strcmp(str, "b5a4") == 0);
	assert(read_move(&pos, "b5c4", &m, black) != 0);
	assert(read_move(&pos, "Kh7", &m, black) == 0);
	assert(m == create_move_g(sq_h1, sq_h2, king, 0));
	print_move(&pos, m, str, mn_san, black);
	assert(strcmp(str, "Kh7") == 0);
	assert(read_move(&pos, "Kg8", &m, black) == 0);
	assert(m == create_move_g(sq_h1, sq_g1, king, 0));
	print_move(&pos, m, str, mn_san, black);
	assert(strcmp(str, "Kg8") == 0);
	assert(read_move(&pos, "b4", &m, black) == 0);
	assert(m == create_move_g(sq_b4, sq_b5, pawn, 0));
	print_move(&pos, m, str, mn_san, black);
	assert(strcmp(str, "b4") == 0);
	assert(read_move(&pos, "bxa4", &m, black) == 0);
	assert(m == create_move_g(sq_b4, sq_a5, pawn, pawn));
	print_move(&pos, m, str, mn_san, black);
	assert(strcmp(str, "bxa4") == 0);
	assert(read_move(&pos, "bxc4", &m, black) != 0);
	assert(read_move(&pos, "o-o", &m, black) != 0);
	assert(read_move(&pos, "o-o-o", &m, black) != 0);

	end = position_read_fen(&pos, positions[1].FEN, &ep_index, &turn);
	assert(end != NULL);
	assert(ep_index == 0);
	assert(turn == white);
	assert(end == (positions[1].FEN + strlen(positions[1].FEN)));
	end = position_print_fen(&pos, str, 0, white);
	assert(strcmp(str, positions[1].FEN) == 0);
	assert(end == (str + strlen(positions[1].FEN)));
	assert(read_move(&pos, "o-o", &m, white) != 0);
	assert(read_move(&pos, "o-o-o", &m, white) != 0);

	end = position_read_fen(&pos, positions[2].FEN, &ep_index, &turn);
	assert(end != NULL);
	assert(ep_index == sq_d5);
	assert(turn == white);
	assert(end == (positions[2].FEN + strlen(positions[2].FEN)));
	end = position_print_fen(&pos, str, sq_d5, white);
	assert(strcmp(str, positions[2].FEN) == 0);
	assert(end == (str + strlen(positions[2].FEN)));
	assert(read_move(&pos, "o-o", &m, white) == 0);
	assert(m == mcastle_king_side);
	assert(read_move(&pos, "O-O", &m, white) == 0);
	assert(m == mcastle_king_side);
	print_move(&pos, m, str, mn_coordinate, white);
	assert(strcmp(str, "e1g1") == 0);
	print_move(&pos, m, str, mn_san, white);
	assert(strcmp(str, "O-O") == 0);
	assert(read_move(&pos, "o-o-o", &m, white) != 0);
	assert(read_move(&pos, "e5d6", &m, white) == 0);
	assert(m == create_move_t(sq_e5, sq_d6, mt_en_passant, pawn, pawn));
	print_move(&pos, m, str, mn_coordinate, white);
	assert(strcmp(str, "e5d6") == 0);
	assert(read_move(&pos, "exd6", &m, white) == 0);
	assert(m == create_move_t(sq_e5, sq_d6, mt_en_passant, pawn, pawn));
	print_move(&pos, m, str, mn_san, white);
	assert(strcmp(str, "exd6e.p.") == 0);

	end = position_read_fen(&pos, positions[3].FEN, &ep_index, &turn);
	assert(end != NULL);
	assert(ep_index == 0);
	assert(turn == white);
	assert(end == (positions[3].FEN + strlen(positions[3].FEN)));
	end = position_print_fen(&pos, str, 0, white);
	assert(strcmp(str, positions[3].FEN) == 0);
	assert(end == (str + strlen(positions[3].FEN)));
	assert(read_move(&pos, "o-o", &m, white) == 0);
	assert(m == mcastle_king_side);
	assert(read_move(&pos, "O-O", &m, white) == 0);
	assert(m == mcastle_king_side);
	print_move(&pos, m, str, mn_coordinate, white);
	assert(strcmp(str, "e1g1") == 0);
	print_move(&pos, m, str, mn_san, white);
	assert(strcmp(str, "O-O") == 0);
	assert(read_move(&pos, "c1b2", &m, white) == 0);
	assert(m == create_move_g(sq_c1, sq_b2, bishop, 0));
	assert(read_move(&pos, "Bb2", &m, white) == 0);
	assert(m == create_move_g(sq_c1, sq_b2, bishop, 0));
	assert(read_move(&pos, "Bb2+", &m, white) == 0);
	assert(m == create_move_g(sq_c1, sq_b2, bishop, 0));
	print_move(&pos, m, str, mn_san, white);
	assert(strcmp(str, "Bb2+") == 0);

	end = position_read_fen(&pos,
	    "rnbqkbnr/ppppp2p/5p2/6p1/8/4P3/PPPP1PPP/RNBQKBNR w KQkq -",
	    NULL, NULL);
	assert(end != NULL);
	assert(read_move(&pos, "d1h5", &m, white) == 0);
	assert(m == create_move_g(sq_d1, sq_h5, queen, 0));
	assert(read_move(&pos, "Qh5", &m, white) == 0);
	assert(m == create_move_g(sq_d1, sq_h5, queen, 0));
	assert(read_move(&pos, "Qh5+", &m, white) == 0);
	assert(m == create_move_g(sq_d1, sq_h5, queen, 0));
	assert(read_move(&pos, "Qh5#", &m, white) == 0);
	assert(m == create_move_g(sq_d1, sq_h5, queen, 0));
	print_move(&pos, m, str, mn_san, white);
	assert(strcmp(str, "Qh5#") == 0);
}
