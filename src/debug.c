#include <stdio.h>

void hexdump(void *data, int len) {
	FILE *f = fopen("hexdump.bin", "w");

	fwrite(data, 1, len, f);

	fclose(f);
}
