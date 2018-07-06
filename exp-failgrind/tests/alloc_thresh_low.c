#include <stdlib.h>

int main(int argc, char* argv[])
{
   int* p;

   /* Should succeed always - 99 < 100*/
   p = malloc(99);
   if (p) {
      p[0] = 42;
      free(p);
   }
   /* Should fail the first time. !(100 < 100) */
   p = malloc(100);
   if (p) {
      p[0] = 42;
      free(p);
   }
   /* Should fail the first time. !(101 < 100) */
   p = malloc(101);
   if (p) {
      p[0] = 42;
      free(p);
   }

   return 0;
}
