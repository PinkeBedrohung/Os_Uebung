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
        printf("waiting for any pid\n");
        pid_t pid = waitpid(-1, NULL, 0);
        printf("pid %d returned: %ld\n", var, pid);
    }
    else
    {
        
    }
    
    
    return 0;
}
