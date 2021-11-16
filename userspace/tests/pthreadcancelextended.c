#include "pthread.h"
#include "stdio.h"


void* threadFunct()
{
    printf("We inside!");

    while(1)
    {

    }

    return 0;
}


int main()
{
    pthread_t arr[5];
    for(int i = 0; i < 5; i++)
        pthread_create(&arr[i], 0x0, &threadFunct, 0x0);


    for(int i = 0; i < 9999999; i++)
    {
    }

    for(int i = 0; i < 5; i++)
        pthread_cancel(arr[i]);

    return 0;
}