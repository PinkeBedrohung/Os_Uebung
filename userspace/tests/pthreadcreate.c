#include "pthread.h"
#include "stdio.h"
#include "assert.h"
void* function_print()
{
    printf("Hello!");
    return NULL;
}


int main()
{
    pthread_t thread[1000];
    for(int i = 0; i < 1000; i++) {
        pthread_create(&(thread[i]), 0x0, &function_print, 0x0);
    }
    sleep(2);
    return 0;
}