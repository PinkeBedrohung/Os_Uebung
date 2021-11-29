#include "unistd.h"
#include "stdio.h"

int main()
{
  int var = 1;

  for (size_t i = 0; i < 50; i++)
  {
    if (var != 0)
    {
      var = fork();
      if (var != 0)
      {
        printf("var: %d\n", var);
      }
      else
      {
        break;
      }
    }

  }




  return 0;
}
