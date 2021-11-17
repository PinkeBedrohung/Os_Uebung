#include "unistd.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

int main(int argc, char* argv[])
{
    if (argc == 3)
    {
        printf("Opened addarg with argc=%d and &argv=%lx\n",argc,(size_t)&argv);
        printf("&argv[1]=%lx\n",(size_t)argv);
        printf("&argv[0][0]=%lx\n",(size_t)&argv[0][0]);
        printf("&argv[0][1]=%lx\n",(size_t)&argv[0][1]);
        printf("&argv[0][2]=%lx\n",(size_t)&argv[0][2]);
        printf("&argv[0][3]=%lx\n",(size_t)&argv[0][3]);
        printf("&argv[0][4]=%lx\n",(size_t)&argv[0][4]);
        int var[2];

        (void)var;
        printf("argv[1] = %s \n" , argv[1]);
        var[0] = atoi(argv[1]);
        var[1] = atoi(argv[2]);
        printf("arg1: %s\n arg2: %s\n", argv[1], argv[2]);

        int res = var[0] + var[1];
        printf("Res: %d\n", res);
    }
    else
    {
        printf("argc != 3\n");
    }


    return 0;
}