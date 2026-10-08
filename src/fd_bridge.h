#pragma once

/* Android ARM constants, not the host's O_* values. */
enum {
    SF_O_ACCMODE = 3, SF_O_CREAT = 0x40, SF_O_EXCL = 0x80,
    SF_O_TRUNC = 0x200, SF_O_APPEND = 0x400, SF_O_NONBLOCK = 0x800,
    SF_O_LARGEFILE = 0x20000,
    SF_F_GETFL = 3, SF_F_SETFL = 4
};

void port_fd_init(void);
int port_fd_track(int fd, int flags, int regular);
void port_fd_forget(int fd);
int sf_open(const char *path, int flags, ...);
int sf_close(int fd);
int sf_fcntl(int fd, int command, ...);
