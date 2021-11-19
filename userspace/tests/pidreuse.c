#include "unistd.h"
#include "wait.h"
#include "stdio.h"

int main()
{

    // DOESN'T WORK
    pid_t child = 1;
    pid_t pid;
    child = fork();

    child = fork();
    if(child == 0)
    {
        execv("/usr/emptymain.sweb", NULL);
    }
    pid = waitpid(child, NULL, 0);
    printf("\nWaited for %ld\n", pid);

    child = fork();
    if(child == 0)
    {
        execv("/usr/emptymain.sweb", NULL);
    }
    pid = waitpid(child, NULL, 0);
    printf("\nWaited for %ld\n", pid);

    child = fork();
    if(child == 0)
    {
        execv("/usr/emptymain.sweb", NULL);
    }
    pid = waitpid(child, NULL, 0);
    printf("\nWaited for %ld\n", pid);
    
    (void)pid;
    return 0;
}
