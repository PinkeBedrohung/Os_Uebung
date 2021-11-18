#include "unistd.h"
#include "wait.h"
#include "stdio.h"

int main()
{
    int var = 1;
    var = fork();

    if(var != 0)
    {
        
        for (size_t i = 0; i < 999999999; i++)
        {
        }
        int retval;
        printf("waiting for pid: %d\n", var);
        pid_t pid = waitpid(var, &retval, 0);
        printf("waited for pid %d returned: %ld with retval: %d\n", var, pid, retval);
    }
    
    return 123;
}
