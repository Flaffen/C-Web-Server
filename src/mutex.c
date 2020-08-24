#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>

#define NUM_THREADS 10

struct data {
	int x;
	int y;
	int z;
	int m;

	int sumxy;
	int sumzm;
	int sum;
};

struct data *mydata;
pthread_t callThd[NUM_THREADS];
pthread_mutex_t mutexsum;

void *sumvals(void *arg)
{
	int i = (int) arg;

	pthread_mutex_lock(&mutexsum);
	if (i % 2 == 0) {
		mydata->sumxy = mydata->x + mydata->y;
		mydata->sum += mydata->sumxy;
	} else {
		mydata->sumzm = mydata->z + mydata->m;
		mydata->sum += mydata->sumzm;
	}
	pthread_mutex_unlock(&mutexsum);

	pthread_exit((void *) 0);
}

int main(void)
{
	pthread_attr_t attr;
	void *status;

	mydata = malloc(sizeof(struct data));

	mydata->x = 10;
	mydata->y = 20;
	mydata->z = 1;
	mydata->m = 2;
	mydata->sumxy = 0;
	mydata->sumzm = 0;
	mydata->sum = 0;

	pthread_mutex_init(&mutexsum, NULL);

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	for (int i = 0; i < NUM_THREADS; i++) {
		pthread_create(&callThd[i], &attr, sumvals, (void *) i);
	}

	pthread_attr_destroy(&attr);

	for (int i = 0; i < NUM_THREADS; i++) {
		pthread_join(callThd[i], &status);
	}

	printf("%d %d %d\n", mydata->sumxy, mydata->sumzm, mydata->sum);

	free(mydata);

	pthread_mutex_destroy(&mutexsum);
	pthread_exit(NULL);

	return 0;
}
