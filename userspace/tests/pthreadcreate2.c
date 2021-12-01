#include "stdio.h"
#include "pthread.h"
void* func(void* arg)
{

  printf("No... es no verdad...\n");
  return 0;
}

int main()
{

  pthread_t t;
  int i = 0;
  while(1)
  {
    i++;
    printf("Luke, yo soy tu padre %d\n\n", i);
    pthread_create(&t,0x0,func,0x0);
  }
  return 0;
}