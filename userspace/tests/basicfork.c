#include "unistd.h"
#include "stdio.h"

int main()
{
    int var = 1;

    var = fork();

    printf("var: %d\n", var);

    return 0;
}
