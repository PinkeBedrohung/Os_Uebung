#include "unistd.h"
#include "pthread.h"
#include "stdio.h"


void thread_function(int* growing_stack)
{
    for (size_t i = 0; i < 999999; i++)
    {
    }
    
    growing_stack[1024 * (1 + pthread_self() % 5)] = 123;
}

int main()
{
    pthread_t thread[10];
    int growing_stack[1024 * 20];
    for(int counter = 0; counter < (1024 * 20); counter++)
    {
        growing_stack[counter] = counter;
    }
    int ret = 0;
    int val = 0;

    for (size_t i = 0; i < 5; i++)
    {
        val++;
        ret = fork();
        printf("ret: %d val: %d\n", ret, val);
        if (ret == 0)
        {
            for (size_t i = 0; i < 4; i++)
            {
                pthread_create(&(thread[i]), 0x0, (void*(*)())&thread_function, growing_stack);
            }
            for (size_t i = 0; i < 4; i++)
            {
                pthread_join(thread[i], NULL);
            }
        }
        else
        {
            break;
        }
    }

    return 0;
}