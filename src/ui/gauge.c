#include <math.h>
#include <stdio.h>
#include "ui/gauge.h"
#include "ui/renderer.h"
#include "ui/themes.h"
#include "utils/font.h"

/* Clamp value to [min, max] */
static float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

void gauge_draw_analog(GaugeDef *g, float value) {
    const Theme *t = theme_current();

    /* Lerp toward target; factor 1.0 = instant, 0.15 = smooth */
    float lerp = t->use_animations ? 0.15f : 1.0f;
    g->anim_val += (value - g->anim_val) * lerp;

    float v = clampf(g->anim_val, g->min_val, g->max_val);

    /* Outer ring */
    renderer_draw_arc(g->cx, g->cy, g->radius,
                      g->start_angle, g->start_angle + g->sweep_angle,
                      t->gauge_track, 48);

    /* Filled arc up to current value */
    float fill_pct = (v - g->min_val) / (g->max_val - g->min_val);
    float fill_end = g->start_angle + g->sweep_angle * fill_pct;

    uint32_t fill_color = (value >= g->redline) ? t->redline : t->gauge_fill;
    renderer_draw_arc(g->cx, g->cy, g->radius,
                      g->start_angle, fill_end,
                      fill_color, (int)(48.0f * fill_pct + 1.0f));

    /* Redline zone arc */
    if (g->redline < g->max_val) {
        float rl_pct = (g->redline - g->min_val) / (g->max_val - g->min_val);
        float rl_start = g->start_angle + g->sweep_angle * rl_pct;
        renderer_draw_arc(g->cx, g->cy, g->radius + 3,
                          rl_start, g->start_angle + g->sweep_angle,
                          t->redline, 12);
    }

    /* Needle: line from center toward rim */
    float needle_angle = g->start_angle + g->sweep_angle * fill_pct;
    float nx = cosf(needle_angle);
    float ny = sinf(needle_angle);
    int inner = g->radius / 4;

    renderer_draw_line(
        g->cx + (int)(nx * (float)inner),
        g->cy + (int)(ny * (float)inner),
        g->cx + (int)(nx * (float)(g->radius - 4)),
        g->cy + (int)(ny * (float)(g->radius - 4)),
        t->needle
    );

    /* Center dot */
    renderer_draw_rect(g->cx - 3, g->cy - 3, 6, 6, t->needle);
}

void gauge_draw_bar(int x, int y, int w, int h,
                    float value, float min_val, float max_val,
                    uint32_t color_fill, uint32_t color_track) {
    renderer_draw_rect(x, y, w, h, color_track);
    float pct = (value - min_val) / (max_val - min_val);
    pct = clampf(pct, 0.0f, 1.0f);
    int fill_w = (int)((float)w * pct);
    if (fill_w > 0)
        renderer_draw_rect(x, y, fill_w, h, color_fill);
}

void gauge_draw_numeric(int x, int y, float value,
                        const char *fmt, const char *unit,
                        uint32_t color, int font_scale) {
    char buf[32];
    snprintf(buf, sizeof(buf), fmt, value);
    font_draw_str(x, y, buf, color, font_scale);
    if (unit && unit[0]) {
        int uw = font_str_width(buf, font_scale) + 4;
        font_draw_str(x + uw, y + font_scale * 2, unit,
                      theme_current()->text_secondary, 1);
    }
}

void gauge_draw_label(int x, int y, const char *label,
                      uint32_t color, int font_scale) {
    font_draw_str(x, y, label, color, font_scale);
}
