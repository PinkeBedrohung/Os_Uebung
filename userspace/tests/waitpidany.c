#include "unistd.h"
#include "wait.h"
#include "stdio.h"

int main()
{
    int var = 1;
    var = fork();

    if(var != 0)
    {
        printf("waiting for any pid\n");
        pid_t pid = waitpid(-1, NULL, 0);
        printf("returned: %ld\n", pid);
    }
    else
    {
        
    }
    
    
    return 0;
}
