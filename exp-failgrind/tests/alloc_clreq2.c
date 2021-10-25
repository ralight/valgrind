#include "stdio.h"
#include "stdlib.h"
#include "../failgrind.h"

int f_a(void)
{
   int* p;

   p = malloc(34);
   if (p) {
      p[4] = 34;
      free(p);
      return 1;
   }
   return 0;
}

int f_b(void)
{
   int* p;

   p = malloc(34);
   if (p) {
      p[4] = 34;
      free(p);
      return 1;
   }
   return 0;
}

int f_c(void)
{
   int* p;

   p = malloc(34);
   if (p) {
      p[4] = 34;
      free(p);
      return 1;
   }
   return 0;
}

int f_d(void)
{
   int* p;

   p = malloc(34);
   if (p) {
      p[4] = 34;
      free(p);
      return 1;
   }
   return 0;
}


void run_test1(void)
{
   f_a();
   f_b();
   f_c();
   f_d();
}

void run_test2(void)
{
   if (!f_a()) return;
   if (!f_b()) return;
   if (!f_c()) return;
   if (!f_d()) return;
}


int main(int argc, char* argv[])
{
   do {
      FAILGRIND_ZERO_COUNTS;
      FAILGRIND_ALLOC_FAIL_ON;
      run_test1();
      FAILGRIND_ALLOC_FAIL_OFF;
      printf("Test 1 success count: %d, new callstack count: %d\n",
            FAILGRIND_ALLOC_GET_SUCCESS_COUNT,
            FAILGRIND_ALLOC_GET_NEW_CALLSTACK_COUNT);
     } while(FAILGRIND_ALLOC_GET_NEW_CALLSTACK_COUNT > 0);

   FAILGRIND_CALLSTACK_CLEAR;

   do {
      FAILGRIND_ZERO_COUNTS;
      FAILGRIND_ALLOC_FAIL_ON;
      run_test2();
      FAILGRIND_ALLOC_FAIL_OFF;
      printf("Test 2 success count: %d, new callstack count: %d\n",
            FAILGRIND_ALLOC_GET_SUCCESS_COUNT,
            FAILGRIND_ALLOC_GET_NEW_CALLSTACK_COUNT);
   } while(FAILGRIND_ALLOC_GET_NEW_CALLSTACK_COUNT > 0);
   FAILGRIND_CALLSTACK_CLEAR;

   return 0;
}
