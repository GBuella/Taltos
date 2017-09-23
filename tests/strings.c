/* vim: set filetype=c : */
/* vim: set noet tw=80 ts=8 sw=8 cinoptions=+4,(0,t0: */

#include "tests.h"

#include <string.h>

#include "str_util.h"
#include "position.h"

static void
test_chars(void)
{
	assert(char_to_file('a') == file_a);
	assert(char_to_file('b') == file_b);
	assert(char_to_file('h') == file_h);
	assert(char_to_file('A') == file_a);
	assert(char_to_file('B') == file_b);
	assert(char_to_file('H') == file_h);

	assert(char_to_rank('1', white) == rank_1);
	assert(char_to_rank('2', white) == rank_2);
	assert(char_to_rank('3', white) == rank_3);
	assert(char_to_rank('4', white) == rank_4);
	assert(char_to_rank('5', white) == rank_5);
	assert(char_to_rank('8', white) == rank_8);
	assert(char_to_rank('1', black) == rank_8);
	assert(char_to_rank('2', black) == rank_7);
	assert(char_to_rank('3', black) == rank_6);
	assert(char_to_rank('4', black) == rank_5);
	assert(char_to_rank('5', black) == rank_4);
	assert(char_to_rank('8', black) == rank_1);

	assert(index_to_file_ch(0) == 'h');
	assert(index_to_file_ch(1) == 'g');
	assert(index_to_file_ch(7) == 'a');
	assert(index_to_file_ch(63) == 'a');

	assert(index_to_rank_ch(0, white) == '8');
	assert(index_to_rank_ch(1, white) == '8');
	assert(index_to_rank_ch(8 + 7, white) == '7');
	assert(index_to_rank_ch(63, white) == '1');
	assert(index_to_rank_ch(0, black) == '1');
	assert(index_to_rank_ch(1, black) == '1');
	assert(index_to_rank_ch(8 + 7, black) == '2');
	assert(index_to_rank_ch(63, black) == '8');

	assert(piece_to_char(queen) == 'q');

	assert(square_to_char(queen, white) == 'Q');
	assert(square_to_char(queen, black) == 'q');

	assert(is_file('f'));
	assert(is_file('F'));
	assert(!is_file('4'));
	assert(!is_file('i'));
	assert(!is_file(' '));

	assert(is_rank('1'));
	assert(is_rank('6'));
	assert(!is_rank('9'));
	assert(!is_rank('0'));
	assert(!is_rank('a'));
	assert(!is_rank(' '));

}

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
	"RNBQK  R"},
	{"rnbqkbnr/ppPppppp/8/8/8/8/PPPPPP1P/RNBQKBNR w KQkq -",
	"rnbqkbnr"
	"ppPppppp"
	"        "
	"        "
	"        "
	"        "
	"PPPPPP P"
	"RNBQKBNR"}
};

static void
test_fen_basic(void)
{
	struct position *position;
	struct position *next;
	move move;
	char str[FEN_BUFFER_LENGTH + MOVE_STR_BUFFER_LENGTH];
	static const char *empty_fen = "8/8/8/8/8/8/8/8 w - - 0 1";
	unsigned hm, fm;
	int ep_index;
	enum player turn;

	position = position_allocate();
	next = position_allocate();
	assert(position_print_fen_full(position, str, 0, 1, 0, white)
	    == str + strlen(empty_fen));
	assert(strcmp(empty_fen, str) == 0);
	assert(NULL != position_read_fen_full(position, start_position_fen,
	    &ep_index, &fm, &hm, &turn));
	assert(ep_index == 0);
	assert(hm == 0 && fm == 1 && turn == white);
	assert(position_print_fen_full(position, str, 0, 1, 0, white)
	    == str + strlen(start_position_fen));
	assert(strcmp(str, start_position_fen) == 0);
	move = create_move_pd(str_to_index("e2", white),
	    str_to_index("e4", white));
	make_move(next, position, move);
	assert(position_piece_at(next, str_to_index("e2", black)) == nonpiece);
	assert(position_piece_at(next, str_to_index("e4", black)) == pawn);
	position_destroy(position);
	position_destroy(next);
}

static void
test_move_str(void)
{
	struct position pos;
	enum player turn;
	int ep_index;
	char str[1024];
	const char *end;
	move m;

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
	assert(m == create_move_ep(sq_e5, sq_d6));
	print_move(&pos, m, str, mn_coordinate, white);
	assert(strcmp(str, "e5d6") == 0);
	assert(read_move(&pos, "exd6", &m, white) == 0);
	assert(m == create_move_ep(sq_e5, sq_d6));
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

	end = position_read_fen(&pos, positions[4].FEN, &ep_index, &turn);
	assert(end != NULL);
	assert(ep_index == 0);
	assert(turn == white);
	assert(end == (positions[4].FEN + strlen(positions[4].FEN)));
	end = position_print_fen(&pos, str, 0, white);
	assert(strcmp(str, positions[4].FEN) == 0);
	assert(end == (str + strlen(positions[4].FEN)));
	assert(read_move(&pos, "c7b8q", &m, white) == 0);
	assert(m == create_move_pr(sq_c7, sq_b8, queen, knight));
	assert(read_move(&pos, "c7b8Q", &m, white) == 0);
	assert(m == create_move_pr(sq_c7, sq_b8, queen, knight));
	assert(read_move(&pos, "c7b8n", &m, white) == 0);
	assert(m == create_move_pr(sq_c7, sq_b8, knight, knight));
	assert(read_move(&pos, "c7b8r", &m, white) == 0);
	assert(m == create_move_pr(sq_c7, sq_b8, rook, knight));
	assert(read_move(&pos, "c7b8b", &m, white) == 0);
	assert(m == create_move_pr(sq_c7, sq_b8, bishop, knight));
	assert(read_move(&pos, "c7d8q", &m, white) == 0);
	assert(m == create_move_pr(sq_c7, sq_d8, queen, queen));
	assert(read_move(&pos, "c7d8Q", &m, white) == 0);
	assert(m == create_move_pr(sq_c7, sq_d8, queen, queen));
	assert(read_move(&pos, "cxd8=Q", &m, white) == 0);
	assert(m == create_move_pr(sq_c7, sq_d8, queen, queen));
	assert(read_move(&pos, "cxd8=Q+", &m, white) == 0);
	assert(m == create_move_pr(sq_c7, sq_d8, queen, queen));
	assert(read_move(&pos, "cxd8=R+", &m, white) == 0);
	assert(m == create_move_pr(sq_c7, sq_d8, rook, queen));

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

void
run_tests(void)
{
	test_chars();
	test_coordinates();
	test_invalid_fens();
	test_fen_basic();
	test_move_str();
}
