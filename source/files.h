#pragma once

#include <stdio.h>
#include <ffconf.h>

typedef struct fd_info
{
    FILE* fp;
    char path[_MAX_LFN + 1];
} fd_info;

extern fd_info fd_source;
extern fd_info fd_dest;

int file_set_xxxx(fd_info *fd, const char *path, const char *mode);
void file_clear(void);

int file_action_delete(void);
int file_action_move(void);
int file_action_copy(void);

inline int file_is_xxxx(fd_info *fd)
{
    return fd->fp != NULL;
}

#define file_set_src(path) file_set_xxxx(&fd_source, path, "rb")
#define file_is_src() file_is_xxxx(&fd_source)
#define file_get_src() (fd_source.path)

#define file_set_dst(path) file_set_xxxx(&fd_dest, path, "wb")
#define file_is_dst() file_is_xxxx(&fd_dest)
#define file_get_dst() (fd_dest.path)
