#include "unistd.h"
#include "wait.h"
#include "stdio.h"

int main()
{
    int var = 1;
    var = fork();

    if(var != 0)
    {
        int retval;
        printf("waiting for pid: %d\n", var);
        pid_t pid = waitpid(var, &retval, 0);
        printf("waited for pid %d returned: %ld with retval: %d\n", var, pid, retval);
    }
    else
    {
        for (size_t i = 0; i < 999999999; i++)
        {
        }
    }
    
    
    return 123;
}
