
#include "taltos_config.h"

#ifdef TALTOS_CAN_USE_POSIX_FSTAT
#define _POSIX_C_SOURCE 1
#include <stdio.h>
#include <sys/stat.h>
#elif TALTOS_CAN_USE_W_GETFILESIZEEX
#include <Windows.h>
#else
#error dont know how to get filesize
#endif

#include "util.h"

#include <stdlib.h>

static void*
check_allocation(void *address, size_t size)
{
    if (address == NULL && size > 0) {
        fprintf(stderr, "Error allocating %zu bytes\n", size);
        abort();
    }
	return address;
}

void*
xmalloc(size_t size)
{
    return check_allocation(malloc(size), size);
}

void*
xcalloc(size_t size)
{
    return check_allocation(calloc(size, 1), size);
}

void*
xrealloc(void *address, size_t size)
{
    return check_allocation(realloc(address, size), size);
}

/*
https://www.securecoding.cert.org/confluence/display/c/FIO19-C.+Do+not+use+fseek%28%29+and+ftell%28%29+to+compute+the+size+of+a+regular+file

"Setting the file position indicator to end-of-file, as
with fseek(file, 0, SEEK_END), has undefined behavior for a binary stream"
                                            ISO-9899-2011 7.21.3 
    */
int
bin_file_size(FILE *file, size_t *size)
{
#ifdef TALTOS_CAN_USE_POSIX_FSTAT
    struct stat stat;
    if (fstat(fileno(file), &stat) != 0) {
        return -1;
    }
    *size = stat.st_size;
    return 0;
#elif TALTOS_CAN_USE_W_GETFILESIZEEX

#endif
}

