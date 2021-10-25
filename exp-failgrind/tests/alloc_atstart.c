#include <stdlib.h>

/* These should all succeed, atstart=no and we don't enable fails at any point. */
int main(int argc, char* argv[])
{
   int* p;

   p = malloc(10);
   if (p) {
      p[0] = 42;
      free(p);
   }
   p = malloc(10);
   if (p) {
      p[0] = 42;
      free(p);
   }
   p = malloc(10);
   if (p) {
      p[0] = 42;
      free(p);
   }

   return 0;
}
