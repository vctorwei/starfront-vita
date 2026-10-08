#include "port.h"
#include "fd_bridge.h"
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* The pinned Vita newlib has 256 descriptor slots, but its fcntl always
   returns ENOSYS and its descriptor records do not retain open flags.
   Track only descriptors handed to the Android game; no private SDK ABI. */
#define FD_COUNT 256
#define STATUS_FLAGS (SF_O_ACCMODE | SF_O_APPEND | SF_O_NONBLOCK | SF_O_LARGEFILE)
typedef struct { int active, flags, regular; } FdStatus;
static FdStatus descriptors[FD_COUNT];
static pthread_mutex_t fd_lock = PTHREAD_MUTEX_INITIALIZER;

int port_fd_track(int fd, int flags, int regular) {
    if (fd < 0 || fd >= FD_COUNT) { errno = EMFILE; return -1; }
    pthread_mutex_lock(&fd_lock);
    descriptors[fd] = (FdStatus){1, flags & STATUS_FLAGS, regular};
    pthread_mutex_unlock(&fd_lock);
    return 0;
}

void port_fd_forget(int fd) {
    if (fd < 0 || fd >= FD_COUNT) return;
    pthread_mutex_lock(&fd_lock);
    memset(&descriptors[fd], 0, sizeof(descriptors[fd]));
    pthread_mutex_unlock(&fd_lock);
}

void port_fd_init(void) {
    /* Android C++ stream constructors query these before opening any files. */
    port_fd_track(0, 0, 0);
    port_fd_track(1, 1, 0);
    port_fd_track(2, 1, 0);
}

int sf_open(const char *name, int flags, ...) {
    char path[1024];
    if (port_map_path(name, path, sizeof(path))) return -1;
    if ((flags & SF_O_ACCMODE) == SF_O_ACCMODE ||
        (flags & ~(STATUS_FLAGS | SF_O_CREAT | SF_O_EXCL | SF_O_TRUNC))) {
        port_log("open: unsupported Android flags 0x%x", flags);
        errno = EINVAL;
        return -1;
    }
    int native = flags & SF_O_ACCMODE;
    if (flags & SF_O_CREAT) native |= O_CREAT;
    if (flags & SF_O_EXCL) native |= O_EXCL;
    if (flags & SF_O_TRUNC) native |= O_TRUNC;
    if (flags & SF_O_APPEND) native |= O_APPEND;
    if (flags & SF_O_NONBLOCK) native |= O_NONBLOCK;
    /* ARM O_LARGEFILE is 0x20000. It needs no host flag for this game's
       sub-2-GiB resources. 0x8000 is O_NOFOLLOW and must not be ignored. */
    int mode = 0666;
    if (flags & SF_O_CREAT) {
        va_list ap; va_start(ap, flags); mode = va_arg(ap, int); va_end(ap);
    }
    int fd = open(path, native, mode);
    if (fd < 0) return -1;
    struct stat st;
    int regular = fstat(fd, &st) == 0 && S_ISREG(st.st_mode);
    if (port_fd_track(fd, flags, regular) < 0) {
        int saved = errno; close(fd); errno = saved; return -1;
    }
    return fd;
}

int sf_close(int fd) {
    /* Keep close/removal together so another thread cannot open and register
       the reused number before this thread clears its old status. */
    pthread_mutex_lock(&fd_lock);
    int result = close(fd);
    if (result == 0 && fd >= 0 && fd < FD_COUNT)
        memset(&descriptors[fd], 0, sizeof(descriptors[fd]));
    pthread_mutex_unlock(&fd_lock);
    return result;
}

int sf_fcntl(int fd, int command, ...) {
    int result = -1, error = 0;
    pthread_mutex_lock(&fd_lock);
    if (fd < 0 || fd >= FD_COUNT || !descriptors[fd].active) {
        error = EBADF;
    } else if (command == SF_F_GETFL) {
        /* STL calls fcntl(fd, F_GETFL) with TWO arguments. */
        result = descriptors[fd].flags;
    } else if (command == SF_F_SETFL) {
        va_list ap; va_start(ap, command);
        int flags = va_arg(ap, int); va_end(ap);
        FdStatus *entry = &descriptors[fd];
        int changed = (flags ^ entry->flags) & (SF_O_APPEND | SF_O_NONBLOCK);
        if (flags & ~(STATUS_FLAGS | SF_O_CREAT | SF_O_EXCL | SF_O_TRUNC)) {
            error = EINVAL;
        } else if ((changed & SF_O_APPEND) ||
                   ((changed & SF_O_NONBLOCK) && !entry->regular)) {
            /* No native status setter exists. Never claim to have changed
               append behavior or made a terminal/socket nonblocking. */
            error = ENOTSUP;
        } else {
            /* Nonblocking has no I/O effect on regular files. Access mode
               and creation flags are not changeable through F_SETFL. */
            entry->flags = (entry->flags & ~SF_O_NONBLOCK) | (flags & SF_O_NONBLOCK);
            result = 0;
        }
    } else {
        error = EINVAL;
    }
    pthread_mutex_unlock(&fd_lock);
    port_log("fcntl fd=%d cmd=%d -> %d (errno=%d)", fd, command, result, error);
    if (error) errno = error;
    return result;
}
