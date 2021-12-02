#include "unistd.h"
#include "pthread.h"
#include "stdio.h"


void thread_function()
{
    for (size_t i = 0; i < 999999; i++)
    {
    }
    int ret = fork();
    if (ret != 0)
    {
        printf("In Forked Process of Thread\n");
    }

}

int main()
{
    pthread_t thread[20];

    for (int i = 0; i < 20; i++)
    {
        pthread_create(&(thread[i]), 0x0, (void*(*)())&thread_function, 0x0);

        printf("Created Thread: %d\n", i);
    }

    return 0;
}