#include "unistd.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

int main(int argc, char* argv[])
{
    if(argc > 0)
    {
        printf("argv[0] - string_length = %ld\n",strlen(argv[0]));
        printf("Opened printexecbigarg with argc=%d\n",argc);
        printf("argv[0]=%s\n",argv[0]);
        printf("\n");
        printf("argv[1]=%s\n",argv[1]);
        printf("argv[2]=%s\n",argv[2]);
    }
    return 0;
}