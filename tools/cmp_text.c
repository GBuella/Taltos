/* vim: set filetype=c : */
/* vim: set noet tw=80 ts=8 sw=8 cinoptions=+4,(0,t0: */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

static int
fgetc_wrapper(FILE *f)
{
	int c = fgetc(f);

	while (c == '\n' || c == '\r') {
		do {
			c = fgetc(f);
		} while (c == '\n' || c == '\r');
		ungetc(c, f);
		return '\n';
	}

	if (c == EOF) {
		if (errno != 0) {
			perror("fgetc");
			exit(1);
		}
	}

	return c;
}

int
main(int argc, char **argv)
{
	if (argc < 3)
		return 1;

	FILE *f0 = fopen(argv[1], "r");
	if (f0 == NULL) {
		perror(argv[1]);
		return 1;
	}
	FILE *f1 = fopen(argv[2], "r");
	if (f1 == NULL) {
		perror(argv[2]);
		return 1;
	}

	int c0;
	int c1;
	errno = 0;
	do {
		c0 = fgetc_wrapper(f0);
		c1 = fgetc_wrapper(f1);
	} while ((c0 == c1) && (c0 != EOF));

	if (c0 != EOF)
		return 1;

	return 0;
}
