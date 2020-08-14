#include <stdio.h>
#include <stdlib.h>
#include "hashtable.h"

struct person {
	int age;
	char *name;
};

int main(void)
{
	struct hashtable *ht = hashtable_create(128, NULL);

	struct person *me = malloc(sizeof(*me));
	me->age = 18;
	me->name = "Stepan";

	struct person *james = malloc(sizeof(*james));
	james->age = 30;
	james->name = "James Wowkins";

	hashtable_put(ht, "mystruct1", me);
	hashtable_put(ht, "mystruct2", james);

	struct person *me2 = hashtable_get(ht, "mystruct1");

	printf("%s: %d\n", me2->name, me2->age);
	
	return 0;
}

