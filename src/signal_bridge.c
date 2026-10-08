#include "port.h"
#include <errno.h>

/* Vita's newlib has no POSIX sigaction backend.  Its signal() table is
 * per-thread software state, so using it here would promise capabilities
 * that it cannot provide.  The game's socket startup ignores this failure.
 * Do not inspect act: the original callers initialize only its handler. */
int sf_sigaction(int signum, const void *act, void *oldact) {
    (void)act;
    (void)oldact;
    port_log("sigaction(%d) unsupported: no POSIX signal action backend", signum);
    errno = ENOSYS;
    return -1;
}
