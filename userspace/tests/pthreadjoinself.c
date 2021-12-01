#include <stdio.h>
#include <pthread.h>
#include <assert.h>

int main()
{
    int retval = 0;
    int ret = pthread_join(pthread_self(), (void*)&retval);
    printf("I wanna join! ret = %d\n", ret);

    assert(ret == -1);
    printf("Works!");
    return 0;
}