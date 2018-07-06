#include <stdlib.h>

void alloc_func(void)
{
   char* p;

   p = malloc(10);
   if (p) {
      p[5] = 42;
      free(p);
   }
}

void middle(void)
{
   alloc_func();
}


void testing(void)
{
   middle();
}

int main(int argc, char* argv[])
{
   /* Should succeed, alloc failing is disabled */
   alloc_func();

   /* Should also succeed. */
   middle();

   /* Should fail, the testing() function should enable failing */
   testing();

   /* Should succeed, alloc failing is disabled */
   alloc_func();

   /* Should also succeed. */
   middle();

   return 0;
}
