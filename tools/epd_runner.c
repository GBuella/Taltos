
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=(4: */

#define _POSIX_C_SOURCE 200112L

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdnoreturn.h>

unsigned line_no;

static noreturn void
epd_error(void)
{
	fprintf(stderr, "Unrecognized EPD format on line %u\n", line_no);
	exit(1);
}

static noreturn void
epd_error_multi_acn(void)
{
	fprintf(stderr, "Multiple acn opcodes on line %u\n", line_no);
	exit(1);
}

static char engine_in[L_tmpnam];
static char engine_out[L_tmpnam];
static char *best_moves[0x100];
static unsigned best_move_count;
static char *avoid_moves[0x100];
static unsigned avoid_move_count;
static const char *board;
static const char *turn;
static const char *castle_rights;
static const char *ep_target;
static const char *half_moves;
static const char *full_moves;
static const char *id;
static unsigned long long default_node_count_limit = 10000000;
static unsigned long long node_count_limit = 0;

static char engine_command_line[0x10000];

static const char *epd_path;
static const char *engine_path;

static unsigned success_count;

static unsigned long long
parse_acn(const char *token)
{
	char *endptr;
	unsigned long long n = strtoull(token, &endptr, 0);

	if (*endptr == '\0' && n != ULLONG_MAX && n != 0)
		return 0;

	if (line_no > 0)
		fprintf(stderr, "Invalid node count on line %u\n", line_no);
	else
		fputs("Invalid node count", stderr);
	exit(1);
}

enum token_type {
	t_opcode,
	t_bm,
	t_am,
	t_acn,
	t_id
};

static enum token_type
parse_next_opcode(char *token)
{
	if (strcmp(token, "bm") == 0)
		return t_bm;
	if (strcmp(token, "am") == 0)
		return t_am;
	if (strcmp(token, "acn") == 0)
		return t_acn;
	if (strcmp(token, "id") == 0)
		return t_id;

	if (half_moves == NULL) {
		if (!isdigit(token[0]))
			epd_error();

		half_moves = token;
		full_moves = strtok(NULL, " \n");

		if (full_moves == NULL)
			epd_error();

		return t_opcode;
	}

	epd_error();
}

static void
parse_line(char *line)
{
	node_count_limit = 0;
	best_move_count = 0;
	avoid_move_count = 0;
	id = NULL;
	board = strtok(line, " \n");
	turn = strtok(NULL, " \n");
	castle_rights = strtok(NULL, " \n");
	ep_target = strtok(NULL, " \n");

	half_moves = NULL;

	if (ep_target == NULL)
		epd_error();

	char *token;
	char *semicolon = NULL;
	enum token_type next = t_opcode;

	while ((token = strtok(NULL, " \n")) != NULL && semicolon == NULL) {
		char *semicolon = strchr(token, ';');
		if (semicolon != NULL) {
			*semicolon = '\0';
			if (token[0] == '\0')
				break;
		}

		switch (next) {
			case t_opcode:
				next = parse_next_opcode(token);
				break;
			case t_bm:
				best_moves[best_move_count++] = token;
				break;
			case t_am:
				avoid_moves[avoid_move_count++] = token;
				break;
			case t_acn:
				if (node_count_limit != 0)
					epd_error_multi_acn();
				node_count_limit = parse_acn(token);
				break;
			default:
				epd_error();
		}

		if (next == t_id) {
			if ((id = strtok(NULL, "\"")) == NULL)
				epd_error();
			next = t_opcode;
			continue;
		}

		if (semicolon != NULL)
			next = t_opcode;
	}

	if (half_moves == NULL) {
		half_moves = "0";
		full_moves = "1";
	}
}

static void
generate_input(void)
{
	FILE *f = fopen(engine_in, "w");
	if (f == NULL) {
		perror(engine_in);
		exit(1);
	}

	fputs("force\n", f);
	fputs("nopost\n", f);
	fprintf(f, "setboard %s %s %s %s %s %s\n",
	    board, turn, castle_rights, ep_target, half_moves, full_moves);
	fprintf(f, "nps %llu\n", node_count_limit);
	fputs("st 1\n", f);
	fputs("search_sync\n", f);

	fclose(f);
}

static void
process_args(char **arg)
{
	++arg;

	while (*arg != NULL) {
		if (strcmp(*arg, "--acn") == 0)
			default_node_count_limit = parse_acn(*++arg);
		else if (strcmp(*arg, "--engine") == 0)
			engine_path = *++arg;
		else if (strcmp(*arg, "--epd") == 0)
			epd_path = *++arg;
		else
			exit(2);
		++arg;
	}

	if (engine_path == NULL)
		engine_path = "./taltos";
}

static void
print_result(const char *result, const char *move)
{
	printf("#%u ", line_no);
	if (id != NULL)
		printf("\"%s\" ", id);
	fputs(result, stdout);
	if (move != NULL)
		printf(": %s", move);
	putchar('\n');
}

static void
print_success(const char *move)
{
	++success_count;
	print_result("success", move);
}

static void
print_fail(const char *move)
{
	print_result("fail", move);
}

static int
check_output_line(char *line)
{
	if (strtok(line, " \n") == NULL)
		return -1;

	if (tolower(turn[0]) == 'b') {
		if (strtok(NULL, " \n") == NULL)
			return -1;
	}

	char *move = strtok(NULL, " \n");
	if (move == NULL)
		return -1;

	if (best_move_count > 0) {
		for (unsigned i = 0; i < best_move_count; ++i) {
			if (strcmp(move, best_moves[i]) == 0) {
				print_success(move);
				return 0;
			}
		}
		print_fail(move);
	}
	else if (avoid_move_count > 0) {
		for (unsigned i = 0; i < avoid_move_count; ++i) {
			if (strcmp(move, avoid_moves[i]) == 0) {
				print_fail(move);
				return 0;
			}
		}
		print_success(move);
	}
	else {
		print_success(move);
	}

	return 0;
}

static void
check_output(void)
{
	errno = 0;

	FILE *f = fopen(engine_out, "r");
	if (f == NULL) {
		perror(engine_out);
		exit(1);
	}

	char line[0x100];

	if (fgets(line, sizeof(line), f) == NULL) {
		if (errno != 0) {
			perror("checking output");
			exit(1);
		}
	}

	if (check_output_line(line) != 0)
		print_result("fail: invalid output", NULL);

	fclose(f);
}

int
main(int argc, char **argv)
{
	char line[0x100];
	FILE *in = stdin;

	(void) argc;
	process_args(argv);

	errno = 0;
	if (epd_path != NULL) {
		if ((in = fopen(epd_path, "r")) == NULL) {
			perror(epd_path);
			exit(1);
		}
	}
	if (tmpnam(engine_in) == NULL)
		abort();
	if (tmpnam(engine_out) == NULL)
		abort();

	snprintf(engine_command_line, sizeof(engine_command_line),
	    "%s < %s > %s", engine_path, engine_in, engine_out);

	line_no = 1;

	while (fgets(line, sizeof(line), in) != NULL) {
		line[sizeof(line) - 1] = '\0';

		parse_line(line);

		if (node_count_limit == 0)
			node_count_limit = default_node_count_limit;

		if (best_move_count > 0 || avoid_move_count > 0) {
			generate_input();
			if (system(engine_command_line) != 0)
				exit(1);
			check_output();
		}

		++line_no;
	}

	printf("%u / %u\n", success_count, (line_no - 1));
}
