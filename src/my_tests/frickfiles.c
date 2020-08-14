#include <stdio.h>
#include <stdlib.h>

int main(void)
{
	FILE *f = fopen("image.png", "rb");

	fseek(f, 0L, SEEK_END);

	long int bufsize = ftell(f);

	fseek(f, 0L, SEEK_SET);

	unsigned char *buf;
	buf = malloc(bufsize * sizeof(char));

	fread(buf, 1, bufsize, f);

	FILE *f2 = fopen("image2.png", "wb");

	fwrite(buf, 1, bufsize, f2);

	fclose(f);
	fclose(f2);

	return 0;
}

