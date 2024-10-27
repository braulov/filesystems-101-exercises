#include <solution.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>

#define MAX_ARGS 1100
#define MAX_LENGTH_ARG 1900
#define MAX_LENGTH_PATH 256
static void find_files(pid_t pid, char* abs_path) {
    static char path_to_dir[MAX_LENGTH_PATH+1];
    static char path_to_file[MAX_LENGTH_PATH+1];

    memset(path_to_dir,0,MAX_LENGTH_PATH+1);
    memset(path_to_file,0,MAX_LENGTH_PATH+1);
    memset(abs_path,0,MAX_LENGTH_PATH+1);

    snprintf(path_to_dir, (MAX_LENGTH_PATH+1), "/proc/%d/fd", pid);
    DIR* dir = opendir(path_to_dir);
    if (dir == NULL) {
        report_error(path_to_dir,errno);
        return;
    }
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        memset(path_to_file,0,MAX_LENGTH_PATH+1);
        memset(abs_path,0,MAX_LENGTH_PATH+1);
        pid_t file_id = atoi(entry->d_name);
        snprintf(path_to_file, (MAX_LENGTH_PATH+1), "/proc/%d/fd/%d", pid, file_id);

        if (readlink(path_to_file,abs_path,MAX_LENGTH_PATH+1)==-1) {
            report_error(path_to_file, errno);
        }
        else report_file((const char *)abs_path);
    }
    closedir(dir);
}
void lsof(void)
{
    const char* path = "/proc";


    DIR* dir = opendir(path);
    if (dir==NULL){
        report_error(path,errno);
        return;
    }
    struct dirent* entry;
    char* abs_path = malloc(MAX_LENGTH_PATH+1);
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR && strcmp(entry->d_name,".")!=0 && strcmp(entry->d_name,"..")!=0 ) {
            if (!isdigit(entry->d_name[0])) continue;

            pid_t pid = atoi(entry->d_name);
            find_files(pid, abs_path);
        }
    }
    free(abs_path);
    closedir(dir);
}