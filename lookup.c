#include <stdio.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>


int main(int argc, char *argv[])
{
    char *path = "/proc/3951014/fd";
    DIR *fd_dir = opendir(path);

    if (!fd_dir) {
        perror("opendir");
        return 0;
    }

    struct dirent *cdir;
    size_t fd_path_len = strlen(path) + 10;
    char *fd_path = malloc(sizeof(char) * fd_path_len);
    char *buf = malloc(sizeof(char) * PATH_MAX + 1);

    while ((cdir = readdir(fd_dir))) {
        if (strcmp(cdir->d_name, ".") == 0 ||
            strcmp(cdir->d_name, "..") == 0)
            continue;
        snprintf(fd_path, fd_path_len - 1, "%s/%s", path, cdir->d_name);
        printf("Checking: %s\n", fd_path);
        ssize_t link_size = readlink(fd_path, buf, PATH_MAX);
        if (link_size < 0)
            perror("readlink");
        else {
            buf[link_size] = '\0';
            printf("%s\n", buf);
        }
        memset(fd_path, '0', fd_path_len);
    }

    closedir(fd_dir);
    free(fd_path);
    free(buf);

    return 0;
}