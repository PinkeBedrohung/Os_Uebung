#include "unistd.h"
#include "wait.h"
#include "stdio.h"

int main()
{
    pid_t child = 1;
    pid_t pid;

    child = fork();
    if(child == 0)
    {
        printf("\nRunning waitpid.sweb\n");
        execv("/usr/waitpid.sweb", NULL);
    }
    pid = waitpid(child, NULL, 0);

    child = fork();
    if(child == 0)
    {
        printf("\nRunning waitpidany.sweb\n");
        execv("/usr/waitpidany.sweb",NULL);
    }
    pid = waitpid(child, NULL, 0);

    child = fork();
    if(child == 0)
    {
        printf("\nRunning waitpiddeadlock.sweb\n");
        execv("/usr/waitpiddeadlock.sweb",NULL);
    }
    pid = waitpid(child, NULL, 0);

    child = fork();
    if(child == 0)
    {
        printf("\nRunning waitpiddirectreturn.sweb\n");
        execv("/usr/waitpiddirectreturn.sweb",NULL);
    }
    pid = waitpid(child, NULL, 0);

    child = fork();
    if(child == 0)
    {
        printf("\nRunning waitpidincoropt.sweb\n");
        execv("/usr/waitpidincoropt.sweb",NULL);
    }
    pid = waitpid(child, NULL, 0);

    child = fork();
    if(child == 0)
    {
        printf("\nRunning waitpidltmone.sweb\n");
        execv("/usr/waitpidltmone.sweb",NULL);
    }
    pid = waitpid(child, NULL, 0);

    child = fork();
    if(child == 0)
    {
        printf("\nRunning waitpidretvaldir.sweb\n");
        execv("/usr/waitpidretvaldir.sweb",NULL);
    }
    pid = waitpid(child, NULL, 0);

    child = fork();
    if(child == 0)
    {
        printf("\nRunning waitpidretvalmultwait.sweb\n");
        execv("/usr/waitpidretvalmultwait.sweb",NULL);
    }
    pid = waitpid(child, NULL, 0);

    child = fork();
    if(child == 0)
    {
        printf("\nRunning waitpidretvalwait.sweb\n");
        execv("/usr/waitpidretvalwait.sweb",NULL);
    }
    pid = waitpid(child, NULL, 0);

    child = fork();
    if(child == 0)
    {
        printf("\nRunning waitpidwaitop.sweb\n");
        execv("/usr/waitpidwaitop.sweb",NULL);
    }
    pid = waitpid(child, NULL, 0);

    child = fork();
    if(child == 0)
    {
        printf("\nRunning waitpidzero.sweb\n");
        execv("/usr/waitpidzero.sweb",NULL);
    }
    pid = waitpid(child, NULL, 0);

    (void)pid;
    return 0;
}
