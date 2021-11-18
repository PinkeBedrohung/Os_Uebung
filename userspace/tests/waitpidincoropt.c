#include "unistd.h"
#include "wait.h"
#include "stdio.h"

int main()
{
    int var = 1;
    int var2 = 1;
    var = fork();

    if(var2 != 0)
        var2 = fork();

    if(var != 0)
    {
        printf("waiting for pid: %d\n", var);
        pid_t pid = waitpid(var, NULL, 44);
        printf("pid %d returned: %ld\n", var, pid);
        
    }
    else
    {
        for (size_t i = 0; i < 999999999; i++)
        {
        }
    }
    
    return 0;
}