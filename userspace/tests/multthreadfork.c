#include "unistd.h"
#include "pthread.h"
#include "stdio.h"


void thread_function()
{
    int var = 0;

    for (size_t i = 0; i < 1000000; i++)
    {
        if(i%2 == 0)
        {
            var += 2;
        }
        else
        {
            var--;
        }
    }
}

int main()
{
    pthread_t thread[4];

    int ret = 0;
    int val = 0;
    for (size_t i = 0; i < 10; i++)
    {
        val++;
        ret = fork();
        printf("ret: %d val: %d\n", ret, val);
        if (ret == 0)
        {
            val += 1000000;
            for (int i = 0; i < 4; i++)
            {
                pthread_create(&(thread[i]), 0x0, (void*(*)())&thread_function, 0x0);
            }
            break;
        }
        else
        {
            val++;
            // pthread_create(&(thread[0]), 0x0, (void*(*)())&thread_function, 0x0);
        }
        
    }

    return 0;
}