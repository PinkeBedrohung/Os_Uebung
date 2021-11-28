#include "pthread.h"
#include "stdio.h"


void* func()
{
  for(int i = 0; i < 99999999;i++)
  {
  }

  pthread_cancel(7);
  printf("CANCEL MAIN\n");
  return 0;
}


int main()
{
  pthread_t tid;

  pthread_create(&tid, 0x0, func, 0x0);
  int i = 0;

  while(1)
  {
    if(i < 5)
    {
      printf("CAN'T TOUCH THIS\n");
      pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    }
    
    printf("MAIN\n");
    //i++;

  }
  return 0;
}