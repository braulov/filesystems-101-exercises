#include <solution.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>

#define MAX_ARGS 1100
#define MAX_LENGTH_ARG 1900
#define MAX_LENGTH_PATH 256
void split(char* path, char** args) {
    FILE *file = fopen(path,"rb");
    if (file == NULL) {
        free(args[0]);
        args[0] = NULL;
        report_error(path, errno);
        return;
    }
    long fileSize = (MAX_LENGTH_ARG+1)*(MAX_ARGS+1);
    char buffer[fileSize];
    size_t bytesRead = fread(buffer, 1, fileSize,file);
    buffer[bytesRead]='\0';
    int id = 0;
    char* current = buffer;
    while(*current != '\0') {
        memcpy(args[id],current, strlen(current));
        current += strlen(current)+1;
        id++;
    }

    free(args[id]);
    args[id] = NULL;
    fclose(file);

}
void find_path(pid_t pid, char* path) {
    char path_to_exe[MAX_LENGTH_PATH+1];
    memset(path_to_exe,0,(MAX_LENGTH_PATH+1) );
    snprintf(path_to_exe, (MAX_LENGTH_PATH+1), "/proc/%d/exe", pid);
    if (readlink(path_to_exe,path,256)!=-1) {
        return;
    }
    else {
        path = "";
        report_error(path_to_exe, errno);
    }
}
void find_arg(pid_t pid, char** argv) {
    char path[MAX_LENGTH_PATH+1];
    memset(path,0,(MAX_LENGTH_PATH+1) );
    snprintf(path, (MAX_LENGTH_PATH+1), "/proc/%d/cmdline", pid);
    split(path,argv);
}
void find_env(pid_t pid, char** envp) {
    char path[MAX_LENGTH_PATH+1];
    memset(path,0,(MAX_LENGTH_PATH+1) );
    snprintf(path, (MAX_LENGTH_PATH+1), "/proc/%d/environ", pid);
    split(path,envp);
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
    char true_path[MAX_LENGTH_PATH+1];
    char** argv = malloc((MAX_ARGS+1) * sizeof(char*));
    char** envp = malloc((MAX_ARGS+1) * sizeof(char*));

    for(int i = 0; i < MAX_ARGS; i++) {
        argv[i] = malloc((MAX_LENGTH_ARG+1) );
        envp[i] = malloc((MAX_LENGTH_ARG+1) );
    }
    //free(argv[MAX_ARGS]);
    //free(envp[MAX_ARGS]);
    argv[MAX_ARGS] = NULL;
    envp[MAX_ARGS] = NULL;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR && strcmp(entry->d_name,".")!=0 && strcmp(entry->d_name,"..")!=0 ) {
            if (!isdigit(entry->d_name[0])) continue;
            memset(true_path,0,MAX_LENGTH_PATH+1);
            for(int i = 0;i<MAX_ARGS;i++) {
                if (argv[i] == NULL) argv[i] = malloc((MAX_LENGTH_ARG+1));
                if (envp[i] == NULL) envp[i] = malloc((MAX_LENGTH_ARG+1));
                memset(argv[i],0,(MAX_LENGTH_ARG+1));
                memset(envp[i],0,(MAX_LENGTH_ARG+1));
            }
            for(int i = 0;i<MAX_ARGS;i++) {
                memset(argv[i],0,(MAX_LENGTH_ARG+1));
                memset(envp[i],0,(MAX_LENGTH_ARG+1));
            }
            pid_t pid = atoi(entry->d_name);

            find_path(pid,true_path);
            find_arg(pid,argv);
            find_env(pid,envp);

            report_process(pid,(const char *)true_path,argv,envp);
        }
    }
    for (int i = 0; i < MAX_ARGS; i++) {
        free(argv[i]);
        free(envp[i]);
    }
    free(argv);
    free(envp);

    closedir(dir);
}