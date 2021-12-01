#include <stdio.h>
#include "pthread.h"

pthread_t  threadarr[7];
pthread_mutex_t lock;

void* func()
{

    pthread_mutex_lock(&lock);

    printf("thread %ld owns a lock \n", pthread_self());

    for (int i = 0; i < 1000000; i++)
    {

    }
    printf("thread %ld disowns the lock \n", pthread_self());
    pthread_mutex_unlock(&lock);

    return 0;
}

int main()
{
    pthread_mutex_init(&lock, 0);
    for (int i = 0; i < 7; i++)
    {
        pthread_create(&threadarr[7], NULL, func, NULL);
    }
    for (int i = 0; i < 1000000; i++)
    {

    }
    for (int i = 0; i < 7; i++)
    {
        pthread_join(threadarr[i], NULL);
    }
    pthread_mutex_destroy(&lock);

    return 0;
}