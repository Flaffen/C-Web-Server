#include <stdio.h>
#include <stdlib.h>
#include "cache.h"

void put_file(struct cache *myc, char *path)
{
	FILE *f = fopen(path, "r");
	int len = 0;
	char *buffer = malloc(1024 * sizeof(char));
	char c;
	int i = 0;
	for (c = fgetc(f); c != EOF; c = fgetc(f)) {
		buffer[i] = c;
		i++;
	}
	fclose(f);

	cache_put(myc, path, "text/plain", buffer, i);
}

int main(int argc, char *argv[])
{
	struct cache* myc = cache_create(4, 4);

	/*
	for (int i = 1; i < argc; i++) {
		put_file(myc, argv[i]);
	}
	*/
	put_file(myc, "file.txt");
	put_file(myc, "file1.txt");
	put_file(myc, "file2.txt");
	put_file(myc, "file3.txt");
	put_file(myc, "file4.txt");

	struct cache_entry *ce = cache_get(myc, "file1.txt");

	if (ce == NULL) {
		printf("OOPSIES\n");
		return 1;
	}

	printf("----\n%d\n%s----\n", ce->content_length, (char *) ce->content);

	cache_free(myc);
}

