#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "fanplayer.h"

#ifdef WITH_LIBAVDEV
#include "libavdev/adev.h"
#include "libavdev/vdev.h"
#include "libavdev/idev.h"
#endif

#ifdef WIN32
#include <windows.h>
static int open_file_dialog(HWND hwnd, char *name, int len)
{
    OPENFILENAME ofn = {};
    ofn.lStructSize     = sizeof(ofn);
    ofn.hwndOwner       = hwnd;
    ofn.lpstrFilter     = "AVI Files (*.avi)\0*.avi\0FLV Files (*.flv)\0*.flv\0MP4 Files (*.mp4)\0*.mp4\0All Files (*.*)\0*.*\0\0";
    ofn.nFilterIndex    = 3;
    ofn.lpstrFile       = name;
    ofn.nMaxFile        = len;
    ofn.lpstrFileTitle  = NULL;
    ofn.lpstrInitialDir = NULL;
    ofn.lpstrTitle      = "Open File";
    ofn.Flags           = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_LONGNAMES | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt     = NULL;
    return GetOpenFileName(&ofn) ? 0 : -1;
}
#else
static int open_file_dialog(HWND hwnd, char *name, int len) { return -1; }
#endif

typedef struct {
    void *adev;
    void *vdev;
    void *idev;
    void *player;
    int   playing;
} MYAPP;

#ifdef WITH_LIBAVDEV
static int my_videv_cb(void *cbctx, int msg, uint32_t param1, uint32_t param2, uint32_t param3)
{
    MYAPP *app = cbctx;
    switch (msg) {
    case DEV_MSG_KEY_EVENT:
        if (param1 && param2 == ' ') {
            player_play(app->player, (app->playing = !app->playing));
        }
        break;
    }
    return 0;
}
#endif

static void bar(BMP *bmp, int x, int y, int w, int h, int c)
{
    uint32_t *p = (uint32_t*)bmp->pdata + y * bmp->width + x;
    int  i, j;
    for (i = 0; i < h; i++) {
        for (j = 0; j < w; j++) *p++ = c;
        p += bmp->width - w;
    }
}

static int my_player_cb(void *cbctx, int msg, void *buf, int len)
{
    MYAPP *app = cbctx;
    switch (msg) {
    case PLAYER_OPEN_SUCCESS:
        player_play(app->player, (app->playing = 1));
        break;
    case PLAYER_ADEV_SAMPRATE:
        return 48000;
    case PLAYER_ADEV_CHANNELS:
        return 2;
#ifdef WITH_LIBAVDEV
    case PLAYER_ADEV_BUFFER:
        adev_play(app->adev, buf, len, 100);
        break;
    case PLAYER_VDEV_LOCK: {
            BMP     *bmp  = vdev_lock(app->vdev);
            SURFACE *surf = buf;
            surf->w       = bmp ? bmp->width : 0;
            surf->h       = bmp ? bmp->height: 0;
            surf->data    = bmp ? bmp->pdata : NULL;
            surf->stride  = bmp ? bmp->width * 4 : 0;
            surf->format  = SURFACE_FMT_RGB32;
            surf->cdepth  = 32;
            if (surf->h > 3) {
                surf->h -= 3;
                uint32_t duration = player_get(app->player, PARAM_MEDIA_DURATION, NULL);
                uint32_t position = player_get(app->player, PARAM_MEDIA_POSITION, NULL);
                uint32_t w = surf->w * position / duration;
                bar(bmp, 0, surf->h, w, 3, 0xFF8800);
            }
        }
        break;
    case PLAYER_VDEV_UNLOCK:
        vdev_unlock(app->vdev);
        break;
#endif
    }
    return 0;
}

int main(int argc, char *argv[])
{
    MYAPP myapp     = {};
    char  file[256] = "test.mp4";

    if (argc < 2) {
        if (open_file_dialog(NULL, file, sizeof(file)) != 0) return 0;
    } else {
        strncpy(file, argv[1], sizeof(file) - 1);
    }

#ifdef WITH_LIBAVDEV
    myapp.adev   = adev_init(48000, 2, 48000 / 20, 5);
    myapp.vdev   = vdev_init(640, 480, "resizable", my_videv_cb, &myapp);
    myapp.idev   = (void*)vdev_get(myapp.vdev, "idev", NULL);
    vdev_set(myapp.vdev, "title", "fanplayer");
    idev_set(myapp.idev, "cbctx"   , &myapp);
    idev_set(myapp.idev, "callback", my_videv_cb);
#endif

    myapp.player = player_init(file, NULL, my_player_cb, &myapp);

#ifdef WITH_LIBAVDEV
    while (strcmp((char*)vdev_get(myapp.vdev, "state", NULL), "running") == 0) { sleep(1); }
#endif

    player_exit(myapp.player);

#ifdef WITH_LIBAVDEV
    vdev_exit(myapp.vdev, 0);
    adev_exit(myapp.adev);
#endif
    return 0;
}
