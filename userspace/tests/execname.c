#include "unistd.h"
#include "stdio.h"
#include "string.h"
#include "stdlib.h"

int main(int argc, char* argv[])
{
    char *args[] = {"1","2","3","4","5","33333333333",NULL};

    printf("%s\n", args[1]);
    printf("%s\n", args[2]);

    //printf("%d\n", atoi(args[1]));
    //printf("%d\n", atoi(args[2]));
   // int f = fork();
    
   // if(f != 0)
   // {
        printf("exec addargggggggg.sweb\n");
        execv("/usr/addargggggggg.sweb", args);
        printf("ERROR - should not return\n");

   // }
    return 0;
}