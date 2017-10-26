/* vim: set filetype=c : */
/* vim: set noet tw=80 ts=8 sw=8 cinoptions=+4,(0,t0: */
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
