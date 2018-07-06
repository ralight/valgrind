#include <stdlib.h>

int main(int argc, char* argv[])
{
   int* p;

   /* Fails due to "random" chance with fixed seed of 2. */
   p = malloc(10);
   if (p) {
      p[0] = 42;
      free(p);
   }
   /* Should succeed. */
   p = malloc(10);
   if (p) {
      p[0] = 42;
      free(p);
   }
   /* Fails due to "random" chance with fixed seed of 2. */
   p = malloc(10);
   if (p) {
      p[0] = 42;
      free(p);
   }

   return 0;
}
