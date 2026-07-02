#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include "vfs.h"
#include "vfs_internal.h"

struct file_ops_file {
    struct file_ops ops;
    int fd;
};

static int
file_fsync(FHANDLE fd_)
{
    struct file_ops_file *fd = (struct file_ops_file *)fd_;

    if (!fd) {
        return -1;
    }

#ifdef _WIN32
    return _commit(fd->fd);
#else
    return fsync(fd->fd);
#endif
}

static int
file_close(FHANDLE fd_)
{
    struct file_ops_file *fd = (struct file_ops_file *)fd_;

    if (!fd) {
        return -1;
    }

#ifdef _WIN32
    _close(fd->fd);
#else
    close(fd->fd);
#endif

    free(fd);
    return 0;
}

static ssize_t
file_read(FHANDLE fd_, void *buf, size_t count)
{
    struct file_ops_file *fd = (struct file_ops_file *)fd_;

    if (!fd) {
        return -1;
    }

#ifdef _WIN32
    return (ssize_t)_read(fd->fd, buf, (unsigned int)count);
#else
    return read(fd->fd, buf, count);
#endif
}

static ssize_t
file_write(FHANDLE fd_, const void *buf, size_t count)
{
    struct file_ops_file *fd = (struct file_ops_file *)fd_;

    if (!fd) {
        return -1;
    }

#ifdef _WIN32
    return (ssize_t)_write(fd->fd, buf, (unsigned int)count);
#else
    return write(fd->fd, buf, count);
#endif
}

static off_t
file_lseek(FHANDLE fd_, off_t offset, int whence)
{
    struct file_ops_file *fd = (struct file_ops_file *)fd_;

    if (!fd) {
        return -1;
    }

#ifdef _WIN32
    return (off_t)_lseeki64(fd->fd, (__int64)offset, whence);
#else
    return lseek(fd->fd, offset, whence);
#endif
}

static int
file_ioctl(FHANDLE fd_, unsigned long req, ...)
{
    struct file_ops_file *fd = (struct file_ops_file *)fd_;

    if (!fd) {
        return -1;
    }

    (void)req;
    return -1;
}

static int
file_ftruncate(FHANDLE fd_, off_t length)
{
    struct file_ops_file *fd = (struct file_ops_file *)fd_;

    if (!fd) {
        return -1;
    }

#ifdef _WIN32
    return _chsize_s(fd->fd, (__int64)length) == 0 ? 0 : -1;
#else
    return ftruncate(fd->fd, length);
#endif
}

static ssize_t
file_length(FHANDLE fd_)
{
    struct file_ops_file *fd = (struct file_ops_file *)fd_;

    if (!fd) {
        return -1;
    }

#ifdef _WIN32
    struct _stati64 st;

    if (_fstati64(fd->fd, &st) != 0) {
        return -1;
    }

    return (ssize_t)st.st_size;
#else
    struct stat st;

    if (fstat(fd->fd, &st) != 0) {
        return -1;
    }

    return (ssize_t)st.st_size;
#endif
}

FHANDLE
file_open(const char *pathname, int flags, ...)
{
    mode_t mode = 0;
    struct file_ops_file *ops;

    ops = malloc(sizeof(*ops));
    if (!ops) {
        return NULL;
    }

    if (flags & O_CREAT) {
        va_list ap;

        va_start(ap, flags);
        mode = (mode_t)va_arg(ap, int);
        va_end(ap);
    }

#ifdef _WIN32
    ops->fd = _open(pathname, flags | _O_BINARY, mode);
#else
    ops->fd = open(pathname, flags, mode);
#endif

    if (ops->fd < 0) {
        fprintf(
            stderr,
            "[e] cannot open '%s': %s (errno=%d)\n",
            pathname,
            strerror(errno),
            errno
        );

        free(ops);
        return NULL;
    }

    ops->ops.flags = flags & O_ACCMODE;
    ops->ops.read = file_read;
    ops->ops.write = file_write;
    ops->ops.lseek = file_lseek;
    ops->ops.ioctl = file_ioctl;
    ops->ops.ftruncate = file_ftruncate;
    ops->ops.fsync = file_fsync;
    ops->ops.close = file_close;
    ops->ops.length = file_length;

    return (FHANDLE)ops;
}
