#include "pthread.h"
#include "stdio.h"

void* CanceledThread()
{
    printf("We are in the canceled thread!\n");
    while(1){}
    return 0;
}


int main()
{
    pthread_t cancd_thread;

    pthread_create(&cancd_thread, 0x0, &CanceledThread, 0x0);
    for(int i = 0; i < 99999998; i++)
    {

    }
    
    int cancel_retval = pthread_cancel(cancd_thread);
    printf("Thread canceled with retval: %d\n", cancel_retval);
    return 0;
}