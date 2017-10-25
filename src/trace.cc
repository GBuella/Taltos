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

#include "trace.h"

#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>

namespace taltos
{

static FILE *trace_file;

void
trace_init(char **argv)
{
	const char *path = NULL;

	for (char **arg = argv; *arg != NULL; ++arg) {
		if (strcmp(*arg, "--trace") == 0) {
			++arg;
			if (*arg == NULL)
				exit(EXIT_FAILURE);
			path = *arg;
			break;
		}
	}
	if (path == NULL)
		return;

	errno = 0;
	trace_file = fopen(path, "w");

	if (trace_file == NULL) {
		perror(path);
		exit(EXIT_FAILURE);
	}

	(void) setvbuf(trace_file, NULL, _IONBF, 0);

	fputs(argv[0], trace_file);
	for (char **arg = argv + 1; *arg != NULL; ++arg) {
		if (strcmp(*arg, "--trace") == 0) {
			++arg;
			continue;
		}
		fprintf(trace_file, " %s", *arg);
	}
	fputc('\n', trace_file);

	trace("repro: force");
	trace("repro: verbose on");
}

static char*
trace_stamp(char *buf)
{
	time_t now = time(NULL);

	char *t = ctime(&now);

	while (*t != '\0' && *t != '\n' && *t != '\r')
		*buf++ = *t++;

	*buf++ = ' ';

	return buf;
}

void
trace(const char *str)
{
	if (trace_file == NULL)
		return;

	char buffer[0x400];
	char *b = trace_stamp(buffer);

	while (*str != '\0' && b + 1 < buffer + sizeof(buffer))
		*b++ = *str++;

	*b++ = '\n';

	fwrite(buffer, b - buffer, 1, trace_file);
}

void
tracef(const char *format, ...)
{
	if (trace_file == NULL)
		return;

	char buffer[0x400];
	char *b = trace_stamp(buffer);

	va_list ap;

	va_start(ap, format);
	b += vsnprintf(b, buffer + sizeof(buffer) - b - 1, format, ap);
	va_end(ap);

	*b++ = '\n';

	fwrite(buffer, b - buffer, 1, trace_file);
}

}
