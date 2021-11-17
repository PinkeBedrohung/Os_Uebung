#include "pthread.h"
#include "stdio.h"

int main()
{
    pthread_t thready;
    pthread_create(&thready, 0x0, NULL, 0x0);
    printf("Bye!\n");

    return 0;
}