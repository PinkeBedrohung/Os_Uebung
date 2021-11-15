#include <pthread.h>
#include <stdio.h>
#include <assert.h>

void* function_joiner()
{
    printf("I am the joining thread and I can be joined now!\n");
    return (void*) 2;
}

int function_caller()
{
    printf("I am the calling thread!\n");
    return 0;
}

int main()
{
    pthread_t joiner;
    pthread_create(&joiner, NULL, function_joiner, NULL);

    int retval_main = function_caller();
    
    pthread_join(joiner, (void*)&retval_main);
    printf("Join has been called!\n");
    sleep(2);

    if(retval_main != 2)
        printf("Sorry, but join didn't work.\n");
    else
        printf("SUCCESS! Join is working!\n");

    return 0;
}