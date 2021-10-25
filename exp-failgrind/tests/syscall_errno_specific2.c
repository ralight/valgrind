#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>

int main(int argc, char* argv[])
{
   int fd;
   int pid;

   /* This should fail with EACCES (specific failure for open()) */
   fd = open("syscall_errno_specific2.vgtest", O_RDONLY);
   if (fd < 0) {
      printf("Failed to open file: ");
      if (errno == EACCES) {
         printf("Access denied.\n");
      } else {
         printf("Incorrect error.\n");
      }
   } else {
      close(fd);
   }

   /* This should succeed - specific failures only */
   pid = getpid();
   if (pid < 0) {
      printf("Failed to get pid: ");
      if (errno == EINVAL) {
         printf("Invalid input.\n");
      } else {
         printf("Incorrect error.\n");
      }
   }

   return 0;
}
