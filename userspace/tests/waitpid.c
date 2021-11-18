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

    //printf("var: %d\n", var);

    if(var != 0)
    {
        /*
        for (size_t i = 0; i < 999999999; i++)
        {
        }*/
        printf("waiting for pid: %d\n", var);
        pid_t pid = waitpid(var, NULL, 0);
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
