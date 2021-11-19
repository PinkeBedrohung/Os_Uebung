#include "unistd.h"
#include "wait.h"
#include "stdio.h"

int main()
{
    int var = 0;
    printf("waiting for pid: %d\n", var);
    pid_t pid = waitpid(var, NULL, -2);
    printf("waited for pid %d returned: %ld\n", var, pid);
    
    return 0;
}
