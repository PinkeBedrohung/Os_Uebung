#include "unistd.h"
#include "stdio.h"

int main()
{
    int var = 0;
    //(void)var;
    var = fork();
    
    //var++;
    
    if (var != 0)
    {
        printf("var: %d\n", var);
        while (0)
        {
        }
    }
    else
    {
        var = fork();
        printf("var: %d\n", var);
        if (var != 0)
        {
            while (0)
            {
            }
        }
    }

    

    return 0;
}
