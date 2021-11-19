#include <pthread.h>
#include <stdio.h>

void* function()
{
    printf("Hello, I am thread 1.\n");
    sleep(10);
    printf("Thread 1 is done and returning 5.\n");
    return (void*) 5;
}

int main()
{
    pthread_t thread1;
    pthread_create(&thread1, NULL, function, NULL);

    pthread_detach(thread1);
    printf("Thread 1 is detached.\n");

    int retval1 = 1;

    printf("join is called on thread 1.\n");
    int error1 = pthread_join(thread1, (void*)&retval1);

    printf("Retval1 = %d\nerror1 = %d\n", retval1, error1);

    return 0;
}