#include <stdlib.h>

int main(int argc, char* argv[])
{
   int* p;

   /* Should succeed always (99 < 100L)*/
   p = malloc(99);
   if (p) {
      p[0] = 42;
      free(p);
   }
   /* Should fail the first time. !(100 < 100L) && !(100 > 100H) */
   p = malloc(100);
   if (p) {
      p[0] = 42;
      free(p);
   }
   /* Should succeed always (101 > 100H) */
   p = malloc(101);
   if (p) {
      p[0] = 42;
      free(p);
   }

   return 0;
}
