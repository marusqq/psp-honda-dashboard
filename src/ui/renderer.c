#include <math.h>
#include <string.h>
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspgum.h>
#include "ui/renderer.h"
#include "utils/log.h"

/* Display list: 512KB, 64-byte aligned */
static unsigned int __attribute__((aligned(64))) gu_list[131072];

/* VRAM layout (GU_PSM_8888, 4 bytes/pixel, stride=512):
   fbp0 at offset 0              (512*272*4 = 557056 bytes)
   fbp1 at offset 557056         (557056 bytes)
   (no depth buffer - 2D only)   */
#define VRAM_FB_SIZE  (FRAMEBUF_W * SCREEN_H * 4)

typedef struct {
    unsigned int color;
    float        x, y, z;
} __attribute__((packed)) Vert2D;

int renderer_init(void) {
    sceGuInit();
    sceGuStart(GU_DIRECT, gu_list);

    sceGuDrawBuffer(GU_PSM_8888, (void *)0, FRAMEBUF_W);
    sceGuDispBuffer(SCREEN_W, SCREEN_H, (void *)VRAM_FB_SIZE, FRAMEBUF_W);
    sceGuDepthBuffer((void *)(VRAM_FB_SIZE * 2), FRAMEBUF_W);

    sceGuOffset(2048 - SCREEN_W / 2, 2048 - SCREEN_H / 2);
    sceGuViewport(2048, 2048, SCREEN_W, SCREEN_H);
    sceGuDepthRange(65535, 0);
    sceGuScissor(0, 0, SCREEN_W, SCREEN_H);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuFrontFace(GU_CW);
    sceGuShadeModel(GU_SMOOTH);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_TEXTURE_2D);

    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);

    LOG_I("Renderer initialized");
    return 0;
}

void renderer_begin_frame(void) {
    sceGuStart(GU_DIRECT, gu_list);
}

void renderer_end_frame(void) {
    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
}

void renderer_shutdown(void) {
    sceGuDisplay(GU_FALSE);
    sceGuTerm();
}

void renderer_clear(uint32_t color) {
    sceGuClearColor(color);
    sceGuClear(GU_COLOR_BUFFER_BIT);
}

void renderer_draw_rect(int x, int y, int w, int h, uint32_t color) {
    Vert2D *v = (Vert2D *)sceGuGetMemory(2 * sizeof(Vert2D));
    v[0].color = color; v[0].x = (float)x;     v[0].y = (float)y;     v[0].z = 0.0f;
    v[1].color = color; v[1].x = (float)(x+w); v[1].y = (float)(y+h); v[1].z = 0.0f;
    sceGuDrawArray(GU_SPRITES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D, 2, NULL, v);
}

void renderer_draw_line(int x1, int y1, int x2, int y2, uint32_t color) {
    Vert2D *v = (Vert2D *)sceGuGetMemory(2 * sizeof(Vert2D));
    v[0].color = color; v[0].x = (float)x1; v[0].y = (float)y1; v[0].z = 0.0f;
    v[1].color = color; v[1].x = (float)x2; v[1].y = (float)y2; v[1].z = 0.0f;
    sceGuDrawArray(GU_LINES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D, 2, NULL, v);
}

void renderer_draw_arc(int cx, int cy, int r, float a_start, float a_end,
                       uint32_t color, int segments) {
    float step = (a_end - a_start) / (float)segments;
    for (int i = 0; i < segments; i++) {
        float a1 = a_start + step * (float)i;
        float a2 = a_start + step * (float)(i + 1);
        renderer_draw_line(
            cx + (int)(cosf(a1) * (float)r),
            cy + (int)(sinf(a1) * (float)r),
            cx + (int)(cosf(a2) * (float)r),
            cy + (int)(sinf(a2) * (float)r),
            color
        );
    }
}

void renderer_draw_filled_tri(float x1, float y1, float x2, float y2,
                               float x3, float y3, uint32_t color) {
    Vert2D *v = (Vert2D *)sceGuGetMemory(3 * sizeof(Vert2D));
    v[0].color = color; v[0].x = x1; v[0].y = y1; v[0].z = 0.0f;
    v[1].color = color; v[1].x = x2; v[1].y = y2; v[1].z = 0.0f;
    v[2].color = color; v[2].x = x3; v[2].y = y3; v[2].z = 0.0f;
    sceGuDrawArray(GU_TRIANGLES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D, 3, NULL, v);
}
