#include <stdlib.h>

int main(int argc, char* argv[])
{
   int* p;

   /* Should fail the first time. !(98 > 99L) */
   p = malloc(98);
   if (p) {
      p[0] = 42;
      free(p);
   }
   /* Should fail the first time. !(99 > 99L) */
   p = malloc(99);
   if (p) {
      p[0] = 42;
      free(p);
   }
   /* Should succeed always (100 > 99L && 100 < 101H */
   p = malloc(100);
   if (p) {
      p[0] = 42;
      free(p);
   }
   /* Should fail the first time. !(101 < 101H) */
   p = malloc(101);
   if (p) {
      p[0] = 42;
      free(p);
   }
   /* Should fail the first time. !(102 < 101H) */
   p = malloc(102);
   if (p) {
      p[0] = 42;
      free(p);
   }

   return 0;
}
