
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=(4: */

#include <stdio.h>
#include <inttypes.h>

#include "print.h"

void
print_table(size_t size, const uint64_t table[static size], const char *name)
{
	printf("const uint64_t %s[%zu] = {\n", name, size);
	printf("0x%016" PRIX64, table[0]);

	for (size_t i = 1; i < size; ++i)
		printf(",%s0x%016" PRIX64, (i % 4 == 0) ? "\n" : "", table[i]);

	puts("\n};\n");
}

void
print_table_2d(size_t s0, size_t s1, const uint64_t table[static s0 * s1],
                           const char *name)
{
	printf("const uint64_t %s[%zu][%zu] = {\n", name, s0, s1);
	for (size_t i = 0; i < s0; ++i) {
		printf("{\n0x%016" PRIX64, table[s0 * i + 0]);
		for (size_t j = 1; j < s1; ++j) {
			uint64_t value = table[i * s0 + j];
			printf(",%s0x%016" PRIX64, (j % 4 == 0) ? "\n " : "", value);
		}
		printf("\n}%s\n", (i + 1 < s0) ? "," : "");
	}
	puts("\n};\n");
}

void
print_table_byte(size_t size, const uint8_t table[static size], const char *name)
{
	printf("const uint8_t %s[%zu] = {\n", name, size);
	printf("0x%02" PRIX8, table[0]);

	for (size_t i = 1; i < size; ++i)
		printf(",%s0x%02" PRIX8, (i % 8 == 0) ? "\n" : "", table[i]);

	puts("\n};\n");
}
