#include "unistd.h"
#include "stdio.h"
#include "string.h"
#include "stdlib.h"

int main(int argc, char* argv[])
{
    char *args[] = {"1", "2", "3", "4"};

    printf("%s\n", args[1]);
    printf("%s\n", args[2]);

    //printf("%d\n", atoi(args[1]));
    //printf("%d\n", atoi(args[2]));
    int f = fork();

    if(f != 0)
    {
        printf("exec addarg.sweb\n");
        execv("/usr/addarg.sweb", args);
        printf("ERROR - should not return\n");

    }
    return 0;
}