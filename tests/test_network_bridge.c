#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct hostent;
struct hostent *sf_gethostbyname(const char *name);

static unsigned log_calls;
static char last_log[160];

void port_log(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(last_log, sizeof(last_log), fmt, args);
    va_end(args);
    ++log_calls;
    errno = EIO;
}

int main(void) {
    char hostname[] = "example.invalid";
    char saved[sizeof(hostname)];
    memcpy(saved, hostname, sizeof(hostname));

    errno = 0;
    assert(sf_gethostbyname(hostname) == NULL);
    assert(errno == ENETUNREACH);
    assert(memcmp(hostname, saved, sizeof(hostname)) == 0);
    assert(log_calls == 1 && strstr(last_log, "network disabled"));

    /* Offline policy never returns a fabricated loopback/numeric hostent. */
    assert(sf_gethostbyname("localhost") == NULL && errno == ENETUNREACH);
    assert(sf_gethostbyname("127.0.0.1") == NULL && errno == ENETUNREACH);

    /* Resolving is disabled before any access to caller-owned name bytes. */
    assert(sf_gethostbyname(NULL) == NULL && errno == ENETUNREACH);
    assert(sf_gethostbyname((const char *)(uintptr_t)1) == NULL);
    assert(errno == ENETUNREACH);
    assert(log_calls == 5);
    puts("Offline resolver failure semantics tests passed");
    return 0;
}
