#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>

void syscall_func(void)
{
   int fd;

   fd = open("syscall_toggle_1.vgtest", O_RDONLY);
   if (fd < 0) {
      printf("Failed to open file.\n");
   } else {
      close(fd);
   }
}

void middle(void)
{
    syscall_func();
}


void testing(void)
{
    middle();
}

int main(int argc, char* argv[])
{
   /* Should succeed, syscall failing is disabled */
   syscall_func();

   /* Should also succeed. */
   middle();

   /* Should fail, the testing() function should enable failing */
   testing();

   /* Should succeed, syscall failing is disabled */
   syscall_func();

   /* Should also succeed. */
   middle();

   return 0;
}
