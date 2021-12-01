#include "pthread.h"
#include <stdio.h>


void* func()
{

  while(1)
  {
    printf("NUNA NEJ\n");
  }

  return 0;
}


int main()
{
  pthread_t tid;

  pthread_create(&tid, 0x0, &func, 0x0);

  int retval = pthread_cancel(tid);


  for(int i = 0; i < 5000000; i++);
  
  printf("Retval: %d\n", retval);

  if(pthread_join(tid, NULL) == 0)
  {
    printf("Pthread joinededed\n");
  }
  else
  {
    printf("Pthread no joineded\n");
  }

  printf("Main out! Mic drop\n");

  return 0;
}