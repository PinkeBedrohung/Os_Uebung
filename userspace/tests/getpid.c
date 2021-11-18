#include "unistd.h"
#include "stdio.h"

int main()
{
    printf("PID of current Process: %ld\n", getpid());
}
