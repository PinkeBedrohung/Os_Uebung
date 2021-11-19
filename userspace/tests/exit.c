#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

void* function()
{
    printf("Hello, I am thread. Going to sleep.\n");

    sleep(2);

    printf("I'm awake. Let's exit.\n");

    exit(8);
    return (void*) 0;
}

int main()
{
    pthread_t thread;
    pthread_create(&thread, NULL, function, NULL);

    while(1){}
    printf("I am still alive.\n");

    return 0;
}