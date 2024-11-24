#include <solution.h>
#include <fuse.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
static int hw_getattr(const char *path, struct stat *stbuf, struct fuse_file_info* fi) {
    (void) fi;
    memset(stbuf, 0, sizeof(struct stat));

    stbuf->st_uid = getuid();
    stbuf->st_gid = getgid();


    if (strcmp(path, "/") == 0) {  // Проверка, если путь корневой
        stbuf->st_mode = S_IFDIR | 0775;
        stbuf->st_nlink = 2;
    } else if (strcmp(path, "../") == 0){
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
    } else if (strcmp(path, "/hello") == 0) {
        stbuf->st_mode = S_IFREG | 0400;
        stbuf->st_nlink = 1;
        stbuf->st_size = 20;
    }
      else {
        return -ENOENT;  // Файл не найден
    }

    return 0;
}
static int hw_readdir( const char *path, void *buffer, fuse_fill_dir_t filler, off_t, struct fuse_file_info *,enum fuse_readdir_flags) {

    filler( buffer, ".", NULL, 0,0);
    filler( buffer, "..", NULL, 0,0);
    if ( strcmp( path, "/" ) == 0 ) // If the user is trying to show the files/directories of the root directory show the following
    {
        filler( buffer, "hello", NULL, 0,0);
    }

    return 0;
}
static int hw_read( const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *) {
    char helloText[] = "hello";
    char* selectedText = NULL;
    if ( strcmp( path, "/hello" ) == 0 )
        selectedText = helloText;
    else
        return -1;
    memcpy( buffer, selectedText + offset, size );
    return strlen( selectedText ) - offset;
}
static const struct fuse_operations hellofs_ops = {
	.getattr = hw_getattr,
    .readdir = hw_readdir,
    //.open = hw_open,
    .read = hw_read,
};
int helloworld(const char *mntp)
{
	char *argv[] = {"./a.out", "-f", (char *)mntp, NULL};
	return fuse_main(3, argv, &hellofs_ops, NULL);
}
