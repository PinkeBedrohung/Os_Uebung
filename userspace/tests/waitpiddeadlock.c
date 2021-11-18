#include "unistd.h"
#include "wait.h"
#include "stdio.h"

int main()
{
    int var = 1;
    size_t parent_pid = getpid();
    var = fork();

    if(var != 0)
    {
        printf("waiting for pid: %d\n", var);
        pid_t pid = waitpid(var, NULL, 0);
        printf("waited for pid %d returned: %ld\n", var, pid);
    }
    else
    {
        printf("waiting for pid: %ld\n", parent_pid);
        pid_t pid = waitpid(parent_pid, NULL, 0);
        printf("waited for pid %ld returned: %ld\n", parent_pid, pid);
    }
    
    
    return 0;
}
