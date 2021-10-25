#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>


int main(int argc, char* argv[])
{
   int fd;
   int sock;

   /* This should fail with EPIPE (specific failure for open() that wouldn't
    * normally be possible) */
   fd = open("syscall_errno_specific1.vgtest", O_RDONLY);
   if (fd < 0) {
      printf("Failed to open file: ");
      if (errno == EPIPE) {
         printf("EPIPE - correct.\n");
      } else {
         printf("Incorrect error (%s (%d)).\n", strerror(errno), errno);
      }
   } else {
      close(fd);
   }

   /* This should fail with EINVAL (general syscall failure) */
   sock = socket(AF_UNIX, SOCK_STREAM, 0);
   if (sock < 0) {
      printf("Failed to create socket: ");
      if (errno == EINVAL) {
         printf("Invalid input.\n");
      } else {
         printf("Incorrect error (%s (%d)).\n", strerror(errno), errno);
      }
   }

   return 0;
}
