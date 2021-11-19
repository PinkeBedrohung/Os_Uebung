#include "stdio.h"

int main()
{
    int x = 666;
    int* address = &x;

    while(1)
    {
        printf("Address = %p\nValue = %d\n", address, *address);
        address = address + 1;
    }
    return 0;
}