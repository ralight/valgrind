#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>

int main(int argc, char* argv[])
{
   int fd;

   fd = open("syscall_errno.vgtest", O_RDONLY);
   if (fd < 0) {
      printf("Failed to open file: ");
      if (errno == EINVAL) {
         printf("Invalid input.\n");
      } else {
         printf("Incorrect error.\n");
      }
   } else {
      close(fd);
   }

   return 0;
}
