#include "pthread.h"
#include <stdio.h>

pthread_t tid[2];
pthread_mutex_t m;
int counter;
int retval;
void* func( )
{
    retval = pthread_mutex_lock(&m);
    printf("locking retval: %d\n", retval);
	unsigned long i = 0;
	counter += 1;
	printf("\n Worker %d has started\n", counter);

	for (i = 0; i < 999999999; i++);
	printf("\n Worker %d has finished\n", counter);

    pthread_mutex_unlock(&m);
	return 0;
}

int main(void)
{
	int i = 0;
	
    pthread_mutex_init(&m, NULL);
    printf("%d", m.init);
    for(i = 0; i < 99999999; i++){};
    i = 0;
	while (i < 2) {
		retval = pthread_create(&(tid[i]), NULL, &func, NULL);
		if (retval != 0)
			printf("\nCreation error \n");
		i++;
	}

	pthread_join(tid[0], NULL);
	pthread_join(tid[1], NULL);
    pthread_mutex_destroy(&m);
	return 0;
}
