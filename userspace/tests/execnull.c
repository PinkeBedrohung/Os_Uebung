#include "unistd.h"
#include "stdio.h"

size_t execDone = 1;
int main()
{
    //int var = 0;
    //(void)var;
    pid_t Pid = fork();

    //execDone = 1;
    if(Pid != 0)
    {
        int test = 0;
        test = execv(NULL,NULL);
        if(test == -1)
        {
            printf("Exec is NULL \n");
        }
        else
        {
            printf("exec done \n");
        }
        

    }
   
    return 0;
}
