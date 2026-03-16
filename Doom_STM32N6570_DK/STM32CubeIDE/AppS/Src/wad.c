/* Includes */
#include <sys/stat.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <stdbool.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <sys/times.h>
#include <fcntl.h>   // for O_RDWR, O_WRONLY, O_CREAT, etc.
#include <unistd.h>  // for STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO
#include <string.h>  // for memcpy

/* Variables */
extern int __io_putchar(int ch) __attribute__((weak));
extern int __io_getchar(void) __attribute__((weak));

#define RESERVED_FILE_HANDLES           4     /* reserve lower file handles for stdin, stdout, stderr */
#define MAX_MEM_FILES                   4

/* WAD file is embedded in the binary in flash memory */
/* Hardcoded for DOOM1 shareware epsisode */
/* Written to flash memory by flash_wad.bat */
static const uint8_t* _binary_wad_DOOM1_WAD_start = (uint8_t*)0x70300000;
static const uint32_t _binary_wad_DOOM1_WAD_size  = 4196020;

typedef struct memfile_handle_s
{
    int handle;
    int curpos;
    int open;
    uint8_t* ptr; // pointer to data
    int filesize; // number of bytes in the file
} memfile_handle_t;

static memfile_handle_t g_memfile_handles[MAX_MEM_FILES] = {0};

int _read(int file, char *ptr, int len)
{

    // Allow reading from stdin via __io_getchar (optional)
    if (file == STDIN_FILENO)
    {
        int i = 0;
        while (i < len)
        {
            if (__io_getchar)
                ptr[i++] = __io_getchar();
            else
                break;
        }
        return i;
    }

    int findex = -1;
    for (int i=0; i < MAX_MEM_FILES; i++)
    {
        if (g_memfile_handles[i].open &&
            g_memfile_handles[i].handle == file)
        {
            findex = i;
            break;
        }
    }
    if (findex < 0 || findex >= MAX_MEM_FILES)
    {
        errno = EBADF; // File not found
        return -1;
    }

    if (g_memfile_handles[findex].curpos >= g_memfile_handles[findex].filesize)
    {
        printf("_read: EOF\n"); // Debugging output
        return 0; // EOF
    }

    // Adjust length if it would read beyond end of file
    if (g_memfile_handles[findex].curpos + len > g_memfile_handles[findex].filesize)
    {
        len = g_memfile_handles[findex].filesize - g_memfile_handles[findex].curpos;
    }
    if (len <= 0) // Handle case where len becomes 0 or negative
    {
        return 0; // EOF
    }

    // Copy data from embedded WAD
    uint8_t* pos = &g_memfile_handles[findex].ptr[g_memfile_handles[findex].curpos];
    memcpy(ptr, pos, len);
    g_memfile_handles[findex].curpos += len;

    return len; // Return number of bytes actually read
}

int _close(int file)
{
    if (file < RESERVED_FILE_HANDLES)
    {
        return 0;
    }
    int findex = -1;
    for (int i=0; i < MAX_MEM_FILES; i++)
    {
        if (g_memfile_handles[i].open &&
            g_memfile_handles[i].handle == file)
        {
            findex = i;
            break;
        }
    }
    if (findex < 0 || findex >= MAX_MEM_FILES)
    {
        errno = EBADF; // File not found
        return -1;
    }
    g_memfile_handles[findex].open = 0;
    g_memfile_handles[findex].handle = 0;
    // Keep the last position and pointer
    // g_memfile_handles[findex].curpos = 0;
    // g_memfile_handles[findex].ptr = pfilecontent;
    // g_memfile_handles[findex].filesize = filesize;

    return 0;
}

int _lseek(int file, int offset, int directive)
{
    if (file < RESERVED_FILE_HANDLES) // Skip reserved file handles
    {
        errno = EBADF;
        return -1;
    }
    int findex = -1;
    for (int i=0; i < MAX_MEM_FILES; i++)
    {
        if (g_memfile_handles[i].open &&
            g_memfile_handles[i].handle == file)
        {
            findex = i;
            break;
        }
    }
    if (findex < 0 || findex >= MAX_MEM_FILES)
    {
        errno = EBADF; // File not found
        return -1;
    }

    uint32_t wad_size = g_memfile_handles[findex].filesize;
    switch (directive)
    {
        case SEEK_SET:
            g_memfile_handles[findex].curpos = offset;
            break;
        case SEEK_CUR:
            g_memfile_handles[findex].curpos += offset;
            break;
        case SEEK_END:
            g_memfile_handles[findex].curpos = wad_size + offset;
            break;
        default:
            errno = EINVAL;
            return -1;
    }

    // Allow seeking beyond end (standard behavior)
    // but ensure non-negative position
    if (g_memfile_handles[findex].curpos < 0)
    {
        g_memfile_handles[findex].curpos = 0;
        errno = EINVAL;
        return -1;
    }

    return g_memfile_handles[findex].curpos;
}

int _open(const char *path, int flags, ...)
{
    uint8_t* pfilecontent = NULL;
    int filesize = 0;

    /* dictionary of supported files */
    if (strcasecmp(path, "doom1.wad") == 0)
    {
        pfilecontent = (uint8_t*)_binary_wad_DOOM1_WAD_start;
        filesize = (int)_binary_wad_DOOM1_WAD_size;
    }

    if (pfilecontent == NULL)
    {
        errno = ENOENT; // No such file or directory
        return -1;
    }

    for (int i=0; i < MAX_MEM_FILES; i++)
    {
        if (g_memfile_handles[i].open == false)
        {
            g_memfile_handles[i].open = 1;
            g_memfile_handles[i].curpos = 0;
            g_memfile_handles[i].handle = RESERVED_FILE_HANDLES + i;
            g_memfile_handles[i].ptr = pfilecontent;
            g_memfile_handles[i].filesize = filesize;
            return g_memfile_handles[i].handle;
        }
    }

    errno = EMFILE;
    return -1;
}

