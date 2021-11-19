#include "pthread.h"
#include "stdio.h"

void* function_print()
{
    printf("Hello!");
    return NULL;
}


int main()
{
    pthread_t thread[4];
    for(int i = 0; i < 4; i++) {
        pthread_create(&(thread[i]), 0x0, &function_print, 0x0);
        printf("%ld", thread[i]);
    }
    sleep(2);

    for(int i = 0; i < 1000000; i++)
    {}

    return 0;
}