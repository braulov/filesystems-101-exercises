#pragma once
#include <unistd.h>

#define MAX_ARGS 64
#define MAX_LENGTH_ARG 100
#define MAX_LENGTH_PATH 256
void find_path(pid_t pid, char* path);
void find_arg(pid_t pid, char** argv);
void find_env(pid_t pid, char** envp);
/**
   Implement this function to list processes. It must call report_process()
   for each running process. If an error occurs when accessing a file or
   a directory, it must call report_error().
*/
void ps(void);
/**
   ps() must call this function to report each running process.

   @exe is the absolute path to the executable file of the process
   @argc is a NULL-terminated array of command line arguments to the process
   @envp is a NULL-terminated array of environment variables of the process
*/
void report_process(pid_t pid, const char *exe, char **argv, char **envp);
/**
   ps() must call this function whenever it detects an error when accessing
   a file or a directory.
*/
void report_error(const char *path, int errno_code);
