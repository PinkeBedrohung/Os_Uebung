#include "pthread.h"
#include "stdio.h"


int main()
{
    printf("Hello! Let's start!\n");
    int growing_stack[1024 * 20];
    for(int counter = 0; counter < (1024 * 20); counter++)
    {
        growing_stack[counter] = counter;
    }
    printf("Thread has finished.\n");
    if(growing_stack[(1024 * 20)-1] == (1024 * 20)-1)
        printf("SUCCESS: Stack has grown.\n");
    else
        printf("Sorry, seems like stack didn't grow.\n");
    return 0;
}