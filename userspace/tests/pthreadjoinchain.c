#include <pthread.h>
#include <stdio.h>
#include <assert.h>

pthread_t tid1, tid2, tid3;

void* thread_one()
{
    printf("I am thread 1 and I want to join thread 3.\n");
    sleep(5);
    int retval = 0;
    printf("Thread1: calling join.\n");
    retval = pthread_join(tid3, (void*)&retval);
    if(retval == -1)
        printf("ERROR: Thread1 detected chain!\n");
    else
        printf("Retval1: %d\n", retval);
    return (void*) 1;
}

void* thread_two()
{
    printf("I am thread 2 and I want to join thread 1.\n");
    int retval = 0;
    printf("Thread2: calling join.\n");
    retval = pthread_join(tid1, (void*)&retval);
    if(retval == -1)
        printf("ERROR: Thread2 detected chain!\n");
    else
        printf("Retval2: %d\n", retval);
    return (void*) 2;
}

void* thread_three()
{
    printf("I am thread 3 and I want to join thread 2.\n");
    int retval = 0;
    printf("Thread3: calling join.\n");
    retval = pthread_join(tid2, (void*)&retval);
    if(retval == -1)
        printf("ERROR: Thread3 detected chain!\n");
    else
        printf("Retval3: %d\n", retval);
    return (void*) 3;
}

int main()
{
    pthread_create(&tid1, NULL, thread_one, NULL);
    pthread_create(&tid2, NULL, thread_two, NULL);
    pthread_create(&tid3, NULL, thread_three, NULL);

    int retval = 0;
    retval = pthread_join(tid3, (void*)&retval);
    if(retval == -1)
        printf("ERROR: Main detected chain!\n");
    else
        printf("Return value is %d \n", retval);
    return 0;
}