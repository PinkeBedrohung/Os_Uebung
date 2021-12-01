#include <pthread.h>
#include <stdio.h>
#include <assert.h>

int main()
{
    int retval_main;
    int ret = pthread_join(123456789, (void*)&retval_main);
    printf("Join has been called! Retval = %d\n", ret);
    
    assert(ret == -1);

    return 0;
}