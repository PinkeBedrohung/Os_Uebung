#include "stdio.h"

int main()
{
    int x = 20480;
    int* address = &x;

    while(1)
    {
        printf("Address = %p\nValue = %d\n", address, *address);
        address = address - 128;
    }

    return 0;
}