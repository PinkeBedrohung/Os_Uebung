#include "unistd.h"
#include "stdio.h"
#include "string.h"
#include "stdlib.h"


int main(int argc, char* argv[])
{
    
    printf(" execv(/usr/execloop1.sweb\n");
    execv("/usr/execloop1.sweb",NULL);
    printf("ERROR - should not return\n");

    return 0;
}