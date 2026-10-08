"""Exercise real log.c with host screen stubs: quiet boot must keep fatal errors visible."""
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

SDK = '''
#include <stdint.h>
typedef struct { unsigned int buttons; } SceCtrlData;
#define SCE_CTRL_START 8
int sceIoMkdir(const char *, int);
int sceKernelDelayThread(unsigned int);
int sceCtrlPeekBufferPositive(int, SceCtrlData *, int);
void sceKernelExitProcess(int) __attribute__((noreturn));
'''
SCREEN = 'int psvDebugScreenInit(void);\nint psvDebugScreenPrintf(const char *, ...);\n'
HARNESS = r'''
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include "vitasdk.h"
#include "port.h"
static int initialized, exit_status;
static char output[4096];
static jmp_buf exited;
int sceIoMkdir(const char *p, int mode) { (void)p; (void)mode; return 0; }
int sceKernelDelayThread(unsigned int delay) { (void)delay; abort(); }
int sceCtrlPeekBufferPositive(int port, SceCtrlData *pad, int n) {
    (void)port; (void)n; pad->buttons = SCE_CTRL_START; return 1;
}
void sceKernelExitProcess(int status) { exit_status = status; longjmp(exited, 1); }
int psvDebugScreenInit(void) { initialized++; return 0; }
int psvDebugScreenPrintf(const char *fmt, ...) {
    assert(initialized > 0);
    va_list ap; va_start(ap, fmt);
    int n = vsnprintf(output + strlen(output), sizeof(output) - strlen(output), fmt, ap);
    va_end(ap); return n;
}
int main(int argc, char **argv) {
    (void)argv;
    port_init_log(); port_log("ordinary startup output");
#ifdef STARFRONT_AUTOSTART
    assert(initialized == 1 && strstr(output, "ordinary startup output"));
#else
    assert(initialized == 0 && output[0] == 0);
#endif
    port_screen(0);
    size_t before = strlen(output); port_log("hidden after renderer takes over");
    assert(strlen(output) == before);
    if (argc > 1) {
        if (!setjmp(exited)) fatal_error("Missing resource: %s", "test.gla");
        assert(exit_status == 1);
        assert(strstr(output, "Missing resource: test.gla"));
        assert(strstr(output, "File logging is disabled."));
        assert(strstr(output, "Press START to exit."));
    }
    return 0;
}
'''


class LogStartupTest(unittest.TestCase):
    def test_psv_and_emulator_startup_and_fatal_error(self):
        cc = shutil.which('cc')
        if not cc:
            self.skipTest('host C compiler unavailable')
        with tempfile.TemporaryDirectory() as folder:
            p = Path(folder)
            (p / 'vitasdk.h').write_text(SDK)
            (p / 'debugScreen.h').write_text(SCREEN)
            (p / 'harness.c').write_text(HARNESS)
            for emulator in (False, True):
                binary = p / ('emulator' if emulator else 'psv')
                command = [cc, '-std=c11', '-Wall', '-Wextra', '-Werror',
                           '-DSTARFRONT_FILE_LOG=0', '-I' + str(p),
                           '-I' + str(ROOT / 'src')]
                if emulator:
                    command.append('-DSTARFRONT_AUTOSTART=1')
                subprocess.run(command + [str(ROOT / 'src/log.c'), str(p / 'harness.c'),
                                          '-o', str(binary)], check=True)
                for error in (False, True):
                    with self.subTest(emulator=emulator, fatal_error=error):
                        subprocess.run([str(binary)] + (['fatal'] if error else []), check=True)


if __name__ == '__main__':
    unittest.main()
