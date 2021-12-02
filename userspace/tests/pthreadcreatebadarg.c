#include "pthread.h"
#include "stdio.h"

int main()
{
    pthread_t thready;
    size_t retval = pthread_create(&thready, 0x0, NULL, 0x0);
    printf("Bye! %ld\n ", retval);

    return 0;
}