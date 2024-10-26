#include <solution.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>

#define MAX_ARGS 64
#define MAX_LENGTH_ARG 100
#define MAX_LENGTH_PATH 256
void split(char* path, char** args) {
    FILE *file = fopen(path,"rb");
    if (file == NULL) {
        report_error(path, errno);
        return;
    }
    char *buffer;

    long fileSize = MAX_LENGTH_ARG*MAX_ARGS;

    buffer = malloc(fileSize+1);
    size_t bytesRead = fread(buffer, 1, fileSize,file);
    buffer[bytesRead]='\0';
    fclose(file);

    char* currentArg = buffer;
    int id = 0;
    while(*currentArg != '\0' && id < MAX_ARGS) {
        args[id] = currentArg;
        id++;
        currentArg += strlen(currentArg) + 1;
    }
}
void find_path(pid_t pid, char* path) {
    char path_to_exe[256];
    snprintf(path_to_exe, sizeof(path_to_exe), "/proc/%d/exe", pid);
    if (readlink(path_to_exe,path,256)!=-1) return;
    else report_error(path_to_exe, errno);
}
void find_arg(pid_t pid, char** argv) {
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
    split(path, argv);
}
void find_env(pid_t pid, char** envp) {
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/environ", pid);
    split(path, envp);
}
void ps(void)
{
    const char* path = "/proc";

    DIR* dir = opendir(path);
    if (dir == NULL) {
        report_error(path,errno);
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR && strcmp(entry->d_name,".")!=0 && strcmp(entry->d_name,"..")!=0 ) {
            if (!isdigit(entry->d_name[0])) continue;

            pid_t pid = atoi(entry->d_name);

            char *true_path = (char*)malloc(MAX_LENGTH_PATH*sizeof(char));
            find_path(pid,true_path);

            char** argv = malloc(MAX_ARGS * sizeof(char*));
            find_arg(pid,argv);

            char** envp = malloc(MAX_ARGS * sizeof(char*));
            find_env(pid,envp);

            report_process(pid,true_path,argv,envp);
        }
    }

    // Закрываем директорию
    closedir(dir);
}
