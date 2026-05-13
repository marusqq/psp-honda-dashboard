#pragma once
#include <stdint.h>

typedef struct {
    int    cx, cy, radius;
    float  min_val, max_val, redline;
    float  start_angle, sweep_angle;
    float  anim_val;   /* current animated value, lerped toward target */
} GaugeDef;

void gauge_draw_analog(GaugeDef *g, float value);
void gauge_draw_bar(int x, int y, int w, int h,
                    float value, float min_val, float max_val,
                    uint32_t color_fill, uint32_t color_track);
void gauge_draw_numeric(int x, int y, float value,
                        const char *fmt, const char *unit,
                        uint32_t color, int font_scale);
void gauge_draw_label(int x, int y, const char *label,
                      uint32_t color, int font_scale);
