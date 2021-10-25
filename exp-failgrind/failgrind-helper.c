//--------------------------------------------------------------------*/
//--- Failgrind: a memory allocation failure testing tool          ---*/
//--------------------------------------------------------------------*/

/*
   This file is part of Failgrind, a Valgrind tool for testing program
   behaviour when heap allocations or syscalls fail.

   Copyright (C) 2021 Roger Light.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307, USA.

   The GNU General Public License is contained in the file COPYING.
*/

/* Contributed by Roger Light <roger@atchoo.org> */

#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define RC_SUCCESS 0
#define RC_SEGV 1
#define RC_STATUS 2
#define RC_FAILS 3
#define RC_TIMEOUT 4

static int timed_out = 0;

static long callstack_file_len(void)
{
	FILE *fptr;
	long len;

	fptr = fopen("failgrind.callstacks", "rt");
	if(!fptr) return 0;
	fseek(fptr, 0, SEEK_END);
	len = ftell(fptr);
	fclose(fptr);
	return len;
}


static int run_exe(unsigned int timeout, int expected_return, char *cmd)
{
	int status;
	pid_t pid;
	int rc = 0;
	long pre_len, post_len;
	char *shell;
	char *args[4];

	shell = getenv("SHELL");
	if(!shell){
		shell = "/bin/sh";
	}

	args[0] = shell;
	args[1] = "-c";
	args[2] = cmd;
	args[3] = NULL;

	pre_len = callstack_file_len();

	pid = fork();
	if(pid == -1){
		return 1;
	}else if(pid == 0){
		status = execv(shell, args);
	}else{
		timed_out = 0;
		alarm(timeout);
		waitpid(pid, &status, 0);
		alarm(0);

		if(timed_out){
			rc = RC_TIMEOUT;
			kill(pid, SIGTERM);
		}else if(WIFEXITED(status)){
			if(expected_return != INT_MAX && WEXITSTATUS(status) != expected_return){
				rc = RC_STATUS;
			}else{
				rc = RC_SUCCESS;
			}
		}else if(WIFSIGNALED(status)){
			rc = RC_SEGV;
		}
	}

	post_len = callstack_file_len();

	if(rc == RC_SUCCESS && post_len > pre_len){
		rc = RC_FAILS;
	}

	return rc;
}


static void print_usage(void)
{
	printf("failgrind-helper is a utility to run a program with the valgrind tool\n");
	printf("failgrind. It repeatedly runs the program until the program segfaults, there\n");
	printf("are no memory allocation/system call failures during the run, or the program\n");
	printf("exits with an expected exit status.\n\n");
	printf("If failgrind-helper exits because the program has segfaulted or timed out, you\n");
	printf("should inspect the final callstack in the failgrind.callstacks file to find the\n");
	printf("cause of the problem.\n\n");
	printf("Usage: failgrind-helper [-e exit-status] [-t timeout] [failgrind-options] <program> [options]\n");
	printf("       failgrind-helper --help\n\n");
	printf("-e : set the expected exit status for the program. The value must be 0-255. If\n");
	printf("     set then the program will continue being run until it returns this status\n");
	printf("     and there are no memory/syscall failures.\n");
	printf("     If not set, then failgrind-helper will stop as soon as there are no allocation\n");
	printf("     or syscall failures.\n");
	printf("-t : set a timeout per run of the program. If the program does not finish within the\n");
	printf("     timeout, failgrind-helper will exit and print a message.\n");
	printf("failgrind-options : pass options to the failgrind tool. Note that failgrind-helper\n");
	printf("                    assumes the use of the failgrind.callstacks file, so do not \n");
	printf("                    override that option.\n");
	printf("program : the program to test\n");
	printf("options : options for the program to test.\n\n");
}


static void handle_alarm(int signal)
{
	timed_out = 1;
}


static char *create_command(int argc, char* argv[])
{
	char *cmd;
	size_t len = strlen("valgrind --tool=exp-failgrind ");

	for(int i=0; i<argc; i++){
		len += strlen(argv[i]) + 1;
	}
	cmd = calloc(1, len+1);
	if(cmd){
		snprintf(cmd, len, "valgrind --tool=exp-failgrind ");
	}
	for(int i=0; i<argc; i++){
		strcat(cmd, argv[i]);
		strcat(cmd, " ");
	}

	return cmd;
}

int main(int argc, char* argv[])
{
	int rc;
	unsigned int timeout = UINT_MAX;
	int expected_return = INT_MAX;
	struct sigaction sa;
	char *cmd;

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = handle_alarm;
	sigaction(SIGALRM, &sa, NULL);
	argc--;
	argv++;
	while(argc){
		if(!strcmp("-t", argv[0])){
			if(argc > 1){
				int v = atoi(argv[1]);
				if(v < 1){
					fprintf(stderr, "Error: Timeout must be > 0.\n");
					return 1;
				}
				timeout = (unsigned int)v;
				argc--;
				argv++;
			}else{
				fprintf(stderr, "Error: -t specified, but no timeout given.\n");
				return 1;
			}
		}else if(!strcmp("-e", argv[0])){
			if(argc > 1){
				int v = atoi(argv[1]);
				if(v < 0 || v > 255){
					fprintf(stderr, "Error: Expected return must be >= 0 and <= 255.\n");
					return 1;
				}
				expected_return = v;
				argc--;
				argv++;
			}else{
				fprintf(stderr, "Error: -e specified, but no expected return value given.\n");
				return 1;
			}
		}else if(!strcmp("--help", argv[0])){
			print_usage();
			return 0;
		}else{
			/* First unknown argument, this is failgrind options or the command to be tested */
			break;
		}
		argc--;
		argv++;
	}
	if(argc == 0){
		fprintf(stderr, "Error: No command specified. Try --help\n");
		return 1;
	}

	cmd = create_command(argc, argv);
	if(cmd == NULL){
		return 1;
	}
	do{
		rc = run_exe(timeout, expected_return, cmd);
		if(rc == RC_TIMEOUT){
			printf("Program timed out.\n");
			break;
		}else if(rc == RC_SEGV){
			printf("Program segfaulted.\n");
			break;
		}
	}while(rc != RC_SUCCESS);
	free(cmd);

	return 0;
}
