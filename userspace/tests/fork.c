#include "unistd.h"
#include "stdio.h"

int main()
{
    int var = 0;
    //(void)var;
    fork();
    
    var++;
    printf("var: %d", var);
    /*
    if (var == 1)
    {
        var++;
    }
    */
    while (1)
    {
    }
    return 0;
}
