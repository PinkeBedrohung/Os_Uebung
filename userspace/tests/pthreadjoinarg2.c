#include <pthread.h>
#include <stdio.h>
#include <assert.h>

#define ADDRESS 0x800000000

int function_joiner()
{
    printf("I am the joining thread and I can be joined now!\n");
    return 2;
}

int main()
{
    printf("Hello");
    pthread_t joiner;
    pthread_create(&joiner, NULL, (void*)&function_joiner, NULL);

    void* ptr = (void*)ADDRESS;
    
    int ret = pthread_join(joiner, ptr);

    printf("Exit should've been called");

    assert(ret == -1);

    return 0;
}