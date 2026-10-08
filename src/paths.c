#include "port.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
int port_map_path(const char *input, char *output, size_t size) {
    static const struct { const char *android, *vita; } roots[] = {
        {"/sdcard/gameloft/games/GloftSFHP", GAME_DIR},
        {"/mnt/sdcard/gameloft/games/GloftSFHP", GAME_DIR},
        {"/data/data/com.gameloft.android.ANMP.GloftSFHP.ML", SAVE_DIR},
    };
    if (!input || !output || !size) { errno=EINVAL; return -1; }
    const char *root = "", *suffix = input;
    for (unsigned i=0; i<sizeof(roots)/sizeof(roots[0]); ++i) {
        size_t len = strlen(roots[i].android);
        if (!strncmp(input, roots[i].android, len) && (!input[len] || input[len]=='/')) {
            root=roots[i].vita; suffix=input+len; break;
        }
    }
    if (!*root && input[0]!='/' && !strchr(input, ':')) root=GAME_DIR "/";
    /* Do not let relative paths escape the game's data directory. */
    const char *p=suffix;
    while (*p) {
        while (*p=='/') ++p;
        const char *q=strchr(p,'/'); size_t n=q?(size_t)(q-p):strlen(p);
        if (n==2 && p[0]=='.' && p[1]=='.') { errno=EACCES; return -1; }
        p += n;
    }
    int n=snprintf(output,size,"%s%s",root,suffix);
    if (n<0 || (size_t)n>=size) { errno=ENAMETOOLONG; return -1; }
    return 0;
}
