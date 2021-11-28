#include <stdio.h>
#include "pthread.h"
int main()
{
  size_t p = pthread_create(0x0,0x0,0x0,0x0);
  printf("%ld", p);
  return 0;
}