#include "files.h"

#include <string.h>
#include <stdio.h>
#include <unistd.h>

fd_info fd_source = { 0 };
fd_info fd_dest = { 0 };

static int cp_src_to_dst(void);

#define SAFE_CLOSE(x) if (x) { fclose(x); x = NULL; }
#define READ_BUFFER 512
#define _DEBUG 1

int file_set_xxxx(fd_info *fd, const char *path, const char *mode)
{
    SAFE_CLOSE(fd->fp);
    strncpy(fd->path, path, _MAX_LFN);
    fd->fp = fopen(fd->path, mode);
    return fd->fp != NULL;
}

void file_clear(void)
{
    SAFE_CLOSE(fd_source.fp);
    SAFE_CLOSE(fd_dest.fp);

    memset(&fd_source, 0, sizeof(fd_source));
    memset(&fd_dest, 0, sizeof(fd_dest));
}

int file_action_delete(void)
{
    int ret;

    if (!file_is_src())
    {
        return 0;
    }

    SAFE_CLOSE(fd_source.fp);
    ret = !unlink(fd_source.path);

    printf("UNLINK_RESULT: %d\n", ret);

    file_clear();
    return ret;
}

int file_action_move(void)
{
    int ret;

    if (!file_is_src() || !file_is_dst())
    {
        return 0;
    }

    ret = cp_src_to_dst();
    if (ret)
    {
        SAFE_CLOSE(fd_source.fp);
        ret = !unlink(fd_source.path);
#ifdef _DEBUG
        printf("UNLINK_RESULT: %d\n", ret);
#endif
    }

    file_clear();
    return ret;
}

int file_action_copy(void)
{
    int ret;

    if (!file_is_src() || !file_is_dst())
    {
        return 0;
    }

    ret = cp_src_to_dst();

    file_clear();
    return ret;
}

int cp_src_to_dst(void)
{
    char buffer[READ_BUFFER];
    size_t len, process = 0, processed = 0, written = 0;
    int ret = 0;

    fseek(fd_source.fp, 0, SEEK_END);
    len = ftell(fd_source.fp);
    fseek(fd_source.fp, 0, SEEK_SET);

    while (processed < len)
    {
        if ((len - processed) < READ_BUFFER)
        {
            written = len - processed;
        }
        else
        {
            written = READ_BUFFER;
        }

        process = fread(buffer, 1, written, fd_source.fp);
#ifdef _DEBUG
        printf("read %s %zu -> %zu\n", fd_source.path, written, process);
#endif
        if (process < 1)
        {
            break;
        }

        written = fwrite(buffer, 1, process, fd_dest.fp);
#ifdef _DEBUG
        printf("write %s %zu -> %zu\n", fd_dest.path, process, written);
#endif
        if (process != written)
        {
            break; // an error
        }

        processed += written;
    }

    if (processed == len)
    {
        ret = 1;
    }

#ifdef _DEBUG
    printf("result %d (%zu/%zu)\n", ret, processed, len);
#endif

    return ret;
}
