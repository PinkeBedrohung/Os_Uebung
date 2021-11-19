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
        int status;
        printf("waiting for pid: %d\n", var);
        pid_t pid = waitpid(var, &status, 0);
        //status&(1<<8) represents the bit that's set if a exit code has been added
        printf("waited for pid %d returned: %ld with status: %d = %d + retval %d\n", var, pid, status, status&(1<<8), status&0xFF);
        
    }
    else
    {
        for (size_t i = 0; i < 999999999; i++)
        {
        }
    }
    
    return 123;
}