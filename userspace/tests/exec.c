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
        //fork();
        execv("/usr/mult.sweb",NULL);
        //execDone = execv("/usr/multthreadfork.sweb",NULL);
        //execDone = execv("/usr/mult.sweb",NULL);
        //while(execDone == 1);
        printf("exec done \n");

    }
    else
    {
        //execv("/usr/mult.sweb",NULL);
       // while(execDone == 1);
    }
    //var++;
    //printf("var: %d", var);
    /*
    if (var == 1)
    {
        var++;
    }

    while (1)
    {
    }*/
    //size_t execDone = 0;
    return 0;
}
