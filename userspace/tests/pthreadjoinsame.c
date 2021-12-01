#include <pthread.h>
#include <stdio.h>
#include <assert.h>

pthread_t joiner, caller;

void* function_joiner()
{
    printf("I am the joining thread and I can be joined now!\n");
    sleep(5);
    return (void*) 2;
}

void* function_caller()
{
    int retval;
    printf("I am the calling thread!\n");
    pthread_join(joiner, (void*)&retval);
    return (void*) 4;
}

int main()
{
    
    pthread_create(&joiner, NULL, function_joiner, NULL);
    sleep(1);
    pthread_create(&caller, NULL, function_caller, NULL);

    int retval = 0;

    sleep(1);
    int ret = pthread_join(joiner, (void*)&retval);
    printf("Join has been called! %d\n", ret);
    
    assert(ret == -1);
    printf("Works");

    return 0;
}