#include "port.h"
#include <errno.h>

struct hostent;

/* This test build has no network transport: sf_socket fails ENETUNREACH.
 * Every native resolver caller checks NULL before reading a hostent.  Do not
 * return an address that suggests a usable network or inspect the hostname.
 * Android h_errno uses __get_h_errno(), which this library does not import;
 * the Vita backend has no matching accessor either.  Native callers inspect
 * only this pointer result, so no unrelated host h_errno storage is used. */
struct hostent *sf_gethostbyname(const char *name) {
    (void)name;
    port_log("gethostbyname unavailable: network disabled in test build");
    errno = ENETUNREACH;
    return NULL;
}
