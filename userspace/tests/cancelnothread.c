#include <stdio.h>
#include "pthread.h"
/*  Calling pthread_cancel with non existing tid
 *
 */

void* func(void* val)
{
  while(1)
  {
    printf("UBICU SE\n");
  }
  return 0;
}


int main(void)
{
  pthread_t tid;
  pthread_create(&tid, NULL, &func, NULL);
  printf("THIS THREAD WITH THREAD ID UPM: %ld\n", tid);

  int retval = pthread_cancel(435);

  if(retval == -1)
  {
    printf("WRONG TID - CANT CANCEL THAT \n");
    return 0;
  }

  printf("WAITING FOR THREAD TO BE CANCELED\n");
  pthread_join(tid, 0);

  return 0;
}