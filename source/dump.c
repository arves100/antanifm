#include "dump.h"
#include "console.h"

#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>

// from minute/dump.c

int copy_file(const char* from, const char* to)
{
    int fd_to, fd_from;
    char buf[4096];
    ssize_t nread;
    int saved_errno;

    fd_from = open(from, O_RDONLY);
    if (fd_from < 0)
        return -1;

    fd_to = open(to, O_WRONLY | O_CREAT, 0666);
    if (fd_to < 0)
        goto out_error;

    while (nread = read(fd_from, buf, sizeof buf), nread > 0)
    {
        char *out_ptr = buf;
        ssize_t nwritten;

        do {
            nwritten = write(fd_to, out_ptr, nread);

            if (nwritten >= 0)
            {
                nread -= nwritten;
                out_ptr += nwritten;
            }
            else if (errno != EINTR)
            {
                goto out_error;
            }
        } while (nread > 0);
    }

    if (nread == 0)
    {
        if (close(fd_to) < 0)
        {
            fd_to = -1;
            goto out_error;
        }
        close(fd_from);

        /* Success! */
        return 0;
    }

  out_error:
    saved_errno = errno;

    close(fd_from);
    if (fd_to >= 0)
        close(fd_to);

    errno = saved_errno;
    return -1;
}

void copy_dir(const char* dir, const char* dest)
{
    printf("Copying %s\n", dir);

    int res = mkdir(dest, 777);
    if(res){
        if(errno != EEXIST) {
            printf("ERROR creating %s: %i\n", dest, errno);
            console_power_to_continue();
            return;
        }
        printf("Warning: '%s' directory already exists\n", dest);
        if (console_abort_confirmation_power_no_eject_yes()) 
            return;
    }
    DIR *dfd = opendir(dir);
    if(!dfd){
        printf("ERROR opening %s: %i\n", dir, errno);
        console_power_to_continue();
        return;
    }
    struct dirent *dp;
    while(dp = readdir(dfd)){
        char src_pathbuf[255];
        snprintf(src_pathbuf, 254, "%s/%s", dir, dp->d_name);
        char dst_pathbuf[255];
        snprintf(dst_pathbuf, 254, "%s/%s", dest, dp->d_name);

        printf("Copying %s\n", src_pathbuf);
        int res = copy_file(src_pathbuf, dst_pathbuf);
        if(!res){
            printf("Error copying %s\n", src_pathbuf);
        }
    } 

    closedir(dfd);
}

int delete_file(const char *file)
{
    return unlink(file);
}

void delete_dir(const char *dir)
{
    DIR *dfd = opendir(dir);
    if(!dfd){
        printf("ERROR opening %s: %i\n", dir, errno);
        console_power_to_continue();
        return;
    }
    struct dirent *dp;
    while(dp = readdir(dfd)){
        char src_pathbuf[255];
        snprintf(src_pathbuf, 254, "%s/%s", dir, dp->d_name);

        printf("Deleting %s\n", src_pathbuf);
        int res = delete_file(src_pathbuf);
        if(!res){
            printf("Error deleting %s\n", src_pathbuf);
        }
    } 

    closedir(dfd);
}

int exist_file(const char *file)
{
    return access(file, F_OK) == 0;
}

const char *get_file_name(const char *file)
{
    char *inner;
    char* realpath;
    size_t pos;
    
    inner = strstr(file, "//");

    if (!inner)
    {
        return NULL;
    }

    inner += 2;

    while (inner != NULL)
    {
        realpath = inner + 1;
        inner = strchr(realpath, '/');
    }

#if 0
    pos = realpath - file;

    memcpy(out, file, pos);
    out[pos] = '\0';

    return out;
#else
    return realpath - 1;
#endif
}
