#pragma once
#include <stdint.h>

#define SCREEN_W   480
#define SCREEN_H   272
#define FRAMEBUF_W 512

/* ABGR color helpers (PSP GU_PSM_8888 is ABGR in little-endian) */
#define RGBA(r, g, b, a) (((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(r))

#define COLOR_WHITE     RGBA(255, 255, 255, 255)
#define COLOR_BLACK     RGBA(0,   0,   0,   255)
#define COLOR_RED       RGBA(255, 0,   0,   255)
#define COLOR_GREEN     RGBA(0,   220, 0,   255)
#define COLOR_BLUE      RGBA(0,   120, 255, 255)
#define COLOR_YELLOW    RGBA(255, 220, 0,   255)
#define COLOR_ORANGE    RGBA(255, 140, 0,   255)
#define COLOR_GRAY      RGBA(140, 140, 140, 255)
#define COLOR_DARKGRAY  RGBA(50,  50,  50,  255)
#define COLOR_DARKBG    RGBA(18,  18,  18,  255)
#define COLOR_REDLINE   RGBA(220, 30,  30,  255)

int  renderer_init(void);
void renderer_begin_frame(void);
void renderer_end_frame(void);
void renderer_shutdown(void);

void renderer_clear(uint32_t color);
void renderer_draw_rect(int x, int y, int w, int h, uint32_t color);
void renderer_draw_line(int x1, int y1, int x2, int y2, uint32_t color);
void renderer_draw_arc(int cx, int cy, int r, float a_start, float a_end,
                       uint32_t color, int segments);
void renderer_draw_filled_tri(float x1, float y1, float x2, float y2,
                               float x3, float y3, uint32_t color);
