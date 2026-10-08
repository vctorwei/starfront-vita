#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int sf_sigaction(int signum, const void *act, void *oldact);

static unsigned log_calls;
static char last_log[160];

void port_log(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(last_log, sizeof(last_log), fmt, args);
    va_end(args);
    ++log_calls;
    /* Logging must not replace the error that the caller observes. */
    errno = EIO;
}

int main(void) {
    unsigned char oldact[32];
    unsigned char saved[sizeof(oldact)];
    memset(oldact, 0xa5, sizeof(oldact));
    memcpy(saved, oldact, sizeof(oldact));

    /* Deliberately unreadable input proves no Android structure is read. */
    const void *unreadable = (const void *)(uintptr_t)1;
    errno = 0;
    assert(sf_sigaction(13, unreadable, oldact + 8) == -1);
    assert(errno == ENOSYS);
    assert(memcmp(oldact, saved, sizeof(oldact)) == 0);
    assert(log_calls == 1 && strstr(last_log, "sigaction(13) unsupported"));

    /* Even a query fails explicitly and leaves the output buffer intact. */
    assert(sf_sigaction(13, NULL, oldact + 8) == -1 && errno == ENOSYS);
    assert(memcmp(oldact, saved, sizeof(oldact)) == 0);

    /* An unreadable output address also must not be accessed on failure. */
    assert(sf_sigaction(13, unreadable, (void *)(uintptr_t)1) == -1);
    assert(errno == ENOSYS);
    assert(sf_sigaction(13, NULL, NULL) == -1 && errno == ENOSYS);
    assert(log_calls == 4);
    puts("Unsupported sigaction failure semantics tests passed");
    return 0;
}
