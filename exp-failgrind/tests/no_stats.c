#include <stdlib.h>

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
