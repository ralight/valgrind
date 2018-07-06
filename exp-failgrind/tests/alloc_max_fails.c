#include <stdlib.h>

int main(int argc, char* argv[])
{
   int* p;

   /* Fails */
   p = malloc(10);
   if (p) {
      p[0] = 42;
      free(p);
   }
   /* Should succeed, max fails has been met. */
   p = malloc(10);
   if (p) {
      p[0] = 42;
      free(p);
   }
   /* Should succeed, max fails has been met. */
   p = malloc(10);
   if (p) {
      p[0] = 42;
      free(p);
   }

   return 0;
}
