#include "pthread.h"
#include "stdio.h"

int i = 0;

void* func()
{
  printf("HERE WE CANCEL THINGS BABY!\n");
  while(1)
  {
    if(i == 420)
    {
      printf("CANCELING MAIN\n");
      pthread_cancel(7); //cancel main
      break;
    }
  }

  return 0;
}

int main()
{
  pthread_t tid;
  pthread_create(&tid, 0x0, func, 0x0);
  

  while(1)
  {
    printf("M A I N\n");
    i = 420;
  }
  return 0;
}