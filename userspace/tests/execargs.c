#include "unistd.h"
#include "stdio.h"
#include "string.h"
#include "stdlib.h"

int main(int argc, char* argv[])
{
    char *args[] = {"1","4", "7",NULL};

    printf("%s\n", args[1]);
    printf("%s\n", args[2]);

    //printf("%d\n", atoi(args[1]));
    //printf("%d\n", atoi(args[2]));
    int f = fork();
    
    if(f != 0)
    {
        printf("Hallo welt\n");
        execv("/usr/addarg.sweb", args);
        printf("Hallo welt\n");

    }
    return 0;
}