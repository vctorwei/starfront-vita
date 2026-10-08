#include "fd_bridge.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void port_log(const char *fmt, ...) { (void)fmt; }

int main(void) {
    port_fd_init();
    /* No third argument: the exact call shape used by the game's STL. */
    assert(sf_fcntl(0, SF_F_GETFL) == 0);
    assert(sf_fcntl(1, SF_F_GETFL) == 1);
    assert(sf_fcntl(2, SF_F_GETFL) == 1);
    assert(sf_fcntl(-1, SF_F_GETFL) == -1 && errno == EBADF);
    assert(sf_fcntl(256, SF_F_GETFL) == -1 && errno == EBADF);
    assert(sf_fcntl(1, 999) == -1 && errno == EINVAL);
    assert(sf_fcntl(1, SF_F_SETFL, 1 | SF_O_NONBLOCK) == -1 && errno == ENOTSUP);
    assert(sf_fcntl(1, SF_F_GETFL) == 1);

    char path[] = "/tmp/starfront-fcntl-XXXXXX";
    int seed = mkstemp(path);
    assert(seed >= 3);
    assert(write(seed, "abc", 3) == 3);
    assert(close(seed) == 0);

    int fd = sf_open(path, 2 | SF_O_APPEND | SF_O_LARGEFILE);
    assert(fd >= 3);
    assert(sf_fcntl(fd, SF_F_GETFL) == (2 | SF_O_APPEND | SF_O_LARGEFILE));
    assert((fcntl(fd, F_GETFL) & (O_ACCMODE | O_APPEND)) == (O_RDWR | O_APPEND));
    assert(lseek(fd, 0, SEEK_SET) == 0);
    assert(write(fd, "d", 1) == 1);
    assert(lseek(fd, 0, SEEK_SET) == 0);
    char data[5] = {0};
    assert(read(fd, data, 4) == 4 && !strcmp(data, "abcd"));

    /* Refuse a status change the Vita backend cannot actually perform. */
    assert(sf_fcntl(fd, SF_F_SETFL, 2) == -1 && errno == ENOTSUP);
    assert(sf_fcntl(fd, SF_F_GETFL) == (2 | SF_O_APPEND | SF_O_LARGEFILE));
    assert(sf_fcntl(fd, SF_F_SETFL, SF_O_APPEND | SF_O_NONBLOCK) == 0);
    assert(sf_fcntl(fd, SF_F_GETFL) == (2 | SF_O_APPEND | SF_O_NONBLOCK | SF_O_LARGEFILE));
    assert(sf_fcntl(fd, SF_F_SETFL, SF_O_APPEND) == 0);
    assert(sf_fcntl(fd, SF_F_GETFL) == (2 | SF_O_APPEND | SF_O_LARGEFILE));
    assert(sf_close(fd) == 0);
    assert(sf_fcntl(fd, SF_F_GETFL) == -1 && errno == EBADF);

    /* Reusing the same native descriptor must not retain append or R/W. */
    int reused = sf_open(path, 0);
    assert(reused == fd);
    assert(sf_fcntl(reused, SF_F_GETFL) == 0);
    assert(sf_close(reused) == 0);
    assert(sf_open(path, SF_O_CREAT | SF_O_EXCL | 1, 0600) == -1 && errno == EEXIST);
    assert(sf_fcntl(fd, SF_F_GETFL) == -1 && errno == EBADF);

    fd = sf_open(path, SF_O_CREAT | SF_O_TRUNC | 1, 0600);
    assert(fd >= 3);
    assert(sf_fcntl(fd, SF_F_GETFL) == 1); /* No creation-only flags. */
    assert(lseek(fd, 0, SEEK_END) == 0);
    assert(sf_fcntl(fd, SF_F_SETFL, 0x8000) == -1 && errno == EINVAL);
    assert(sf_close(fd) == 0);
    assert(sf_open(path, 0x8000) == -1 && errno == EINVAL); /* ARM O_NOFOLLOW. */
    assert(sf_open(path, 3) == -1 && errno == EINVAL);
    assert(unlink(path) == 0);
    assert(sf_open(path, 0) == -1 && errno == ENOENT);
    puts("Android fcntl / file-status tests passed");
}
