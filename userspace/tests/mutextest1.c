#include <stdio.h>
#include "pthread.h"

int main()
{
    pthread_mutex_t lock;
    if (pthread_mutex_init(&lock, 0)) {
        printf("NOPE.\n");
        return -1;
    }
    printf("YEP!\n");


    return 0;
}