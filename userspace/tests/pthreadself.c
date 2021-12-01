#include <stdio.h>
#include "pthread.h"
#include "assert.h"

int main()
{
    size_t  thread_id = pthread_self();
    assert(thread_id > 0);
    printf("TID: %ld\n", pthread_self());
    assert(thread_id == pthread_self());
    return 0;
}