
#ifndef UTIL_H
#define UTIL_H

#include <stdio.h>
#include "macros.h"

void *xmalloc(size_t)
    attribute_warn_unused_result
    attribute_malloc
    attribute_returns_nonnull
    attribute_alloc_size(1);

void *xcalloc(size_t)
    attribute_warn_unused_result
    attribute_malloc
    attribute_returns_nonnull
    attribute_alloc_size(1);

void *xrealloc(void*, size_t)
    attribute_warn_unused_result
    attribute_alloc_size(2);

int bin_file_size(FILE*, size_t*)
    attribute_warn_unused_result
    attribute_args_nonnull(2);

#endif
