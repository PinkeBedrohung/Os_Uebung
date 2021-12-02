#include "unistd.h"
#include "stdio.h"
#include "string.h"
#include "stdlib.h"

int main(int argc, char* argv[])
{
   // int f = fork();
    
   
        printf(" execv(/usr/execloop2.sweb\n");
        execv("/usr/execloop2.sweb",NULL);
        printf("ERROR - should not return\n");

   
    return 0;
}