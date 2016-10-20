
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=+4,(4: */

#include "trace.h"

#include <stdarg.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>

static FILE *trace_file;

void
trace_init(const char *path)
{
	errno = 0;

	trace_file = fopen(path, "w");

	if (trace_file == NULL) {
		perror(path);
		exit(EXIT_FAILURE);
	}

	(void) setvbuf(trace_file, NULL, _IONBF, 0);

	trace("Taltos debug log start");
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