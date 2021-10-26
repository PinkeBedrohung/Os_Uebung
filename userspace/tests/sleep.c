#include "stdio.h"
#include "nonstd.h"

int main()
{
    printf("Will sleep 13 seconds\n");
    sleep(13);
    printf("Woken up\n");
    return 0;
}