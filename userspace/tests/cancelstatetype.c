#include "pthread.h"
#include "stdio.h"

void* function_print()
{
    printf("Hello!");
    int stateret = pthread_setcancelstate(5, NULL);
    printf("\nstate retval: %d", stateret);
    int typeret = pthread_setcanceltype(5, NULL);
    printf("\nHello! type retval: %d\n", typeret);
    
    return NULL;
}


int main()
{
    pthread_t thread;
    pthread_create(&thread, 0x0, &function_print, 0x0);
    for(int i = 0; i < 10000000; i++)
    {}
    return 0;
}