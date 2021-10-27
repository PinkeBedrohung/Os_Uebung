#include "stdio.h"
#include "time.h"

int main()
{
    clock_t start_clock  = clock();
    
    for(int i = 0; i < 999999999; i++)
    {

    }
    clock_t end_clock = clock();
    printf("Start clock: %ld\n",(size_t)start_clock);
    printf("End clock: %ld\n", (size_t) end_clock);
    printf("Difference: %ld\n", (size_t) (end_clock-start_clock)/CLOCKS_PER_SEC);
    return 0;
}