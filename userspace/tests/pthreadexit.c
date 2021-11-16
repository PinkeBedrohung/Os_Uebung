#include "pthread.h"
#include "assert.h"

int main()
{
    pthread_exit(0);
    assert(0);
    return 0;
}