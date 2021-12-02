#include "unistd.h"
#include "stdio.h"

size_t execDone = 1;
int main()
{
    execv("/usr/basicfork.sweb",NULL);

    printf("exec done \n");
    return 0;
}
