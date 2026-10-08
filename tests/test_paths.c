#include "port.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
static void mapped(const char *from,const char *to) {
    char out[1024];assert(port_map_path(from,out,sizeof(out))==0);assert(!strcmp(out,to));
}
int main(void) {
    mapped("/sdcard/gameloft/games/GloftSFHP/menu.gla",GAME_DIR "/menu.gla");
    mapped("/mnt/sdcard/gameloft/games/GloftSFHP/entities.gla",GAME_DIR "/entities.gla");
    mapped("/data/data/com.gameloft.android.ANMP.GloftSFHP.ML/files/a.sav",SAVE_DIR "/files/a.sav");
    mapped("menu.gla",GAME_DIR "/menu.gla");
    mapped("ux0:data/starfront/save/a.sav",SAVE_DIR "/a.sav");
    mapped("/sdcard/gameloft/games/GloftSFHPother/x","/sdcard/gameloft/games/GloftSFHPother/x");
    char out[1024];assert(port_map_path("../escape",out,sizeof(out))==-1&&errno==EACCES);
    assert(port_map_path("/sdcard/gameloft/games/GloftSFHP/a/../../escape",out,sizeof(out))==-1);
    assert(port_map_path("menu.gla",out,3)==-1&&errno==ENAMETOOLONG);
    assert(port_map_path(NULL,out,sizeof(out))==-1&&errno==EINVAL);
    puts("Path routing tests passed");
}
