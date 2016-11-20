
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=+4,(4: */

#ifndef TALTOS_TRACE_H
#define TALTOS_TRACE_H

#include "macros.h"

#include <stdio.h>

void trace_init(char **argv)
	attribute(nonnull);

void trace(const char *str)
	attribute(nonnull);

void tracef(const char *format, ...)
	attribute(format(printf, 1, 2));

#endif
