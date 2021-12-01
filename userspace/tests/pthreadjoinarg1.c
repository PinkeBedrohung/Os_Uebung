#include <pthread.h>
#include <stdio.h>
#include <assert.h>

#define USER_BREAK 0x0000800000000000ULL

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

    void* ptr = (void*)USER_BREAK;
    
    int ret = pthread_join(joiner, ptr);

    printf("Join has been called! Retval = %d\n and address is%p\n", ret, ptr);
    
    assert(ret == -1);

    return 0;
}