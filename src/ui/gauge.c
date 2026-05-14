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

    /* Needle */
    float needle_angle = g->start_angle + g->sweep_angle * fill_pct;
    float nx = cosf(needle_angle);
    float ny = sinf(needle_angle);

    if (t->needle_style == 1) {
        /* Filled triangle needle with layered hub */
        float cos_p  = -ny, sin_p = nx;
        float tip_x  = (float)g->cx + nx * (float)(g->radius - 4);
        float tip_y  = (float)g->cy + ny * (float)(g->radius - 4);
        float base_d = (float)g->radius * 0.18f;
        float hw     = 5.0f;
        renderer_draw_filled_tri(
            tip_x, tip_y,
            (float)g->cx + nx * base_d + cos_p * hw,
            (float)g->cy + ny * base_d + sin_p * hw,
            (float)g->cx + nx * base_d - cos_p * hw,
            (float)g->cy + ny * base_d - sin_p * hw,
            t->needle
        );
        renderer_draw_rect(g->cx - 5, g->cy - 5, 10, 10, t->gauge_track);
        renderer_draw_rect(g->cx - 3, g->cy - 3, 6,  6,  t->accent);
        renderer_draw_rect(g->cx - 1, g->cy - 1, 3,  3,  t->needle);
    } else {
        int inner = g->radius / 4;
        renderer_draw_line(
            g->cx + (int)(nx * (float)inner),
            g->cy + (int)(ny * (float)inner),
            g->cx + (int)(nx * (float)(g->radius - 4)),
            g->cy + (int)(ny * (float)(g->radius - 4)),
            t->needle
        );
        renderer_draw_rect(g->cx - 3, g->cy - 3, 6, 6, t->needle);
    }
}

void gauge_draw_analog_jdm(GaugeDef *g, float value,
                            float major_step, float minor_step,
                            float label_scale) {
    const Theme *t = theme_current();
    float lerp = t->use_animations ? 0.12f : 1.0f;
    g->anim_val += (value - g->anim_val) * lerp;
    float v = clampf(g->anim_val, g->min_val, g->max_val);

    int   cx    = g->cx, cy = g->cy, r = g->radius;
    float range = g->max_val - g->min_val;
    float fill_pct   = (v - g->min_val) / range;
    float angle_end  = g->start_angle + g->sweep_angle;
    float fill_angle = g->start_angle + g->sweep_angle * fill_pct;
    int   segs_full  = 72;

    /* Outer rim (2px thick arc) */
    renderer_draw_arc(cx, cy, r,     g->start_angle, angle_end, t->gauge_track, segs_full);
    renderer_draw_arc(cx, cy, r - 1, g->start_angle, angle_end, t->gauge_track, segs_full);

    /* Redline zone on outer rim */
    if (g->redline < g->max_val) {
        float rl_pct = (g->redline - g->min_val) / range;
        float rl_a   = g->start_angle + g->sweep_angle * rl_pct;
        int   rl_seg = (int)((1.0f - rl_pct) * (float)segs_full) + 1;
        renderer_draw_arc(cx, cy, r,     rl_a, angle_end, t->redline, rl_seg);
        renderer_draw_arc(cx, cy, r - 1, rl_a, angle_end, t->redline, rl_seg);
    }

    /* Fill arc (3px thick, inner ring) */
    if (fill_pct > 0.001f) {
        int   fill_seg = (int)(fill_pct * (float)segs_full) + 1;
        uint32_t fc = (value >= g->redline) ? t->redline : t->gauge_fill;
        renderer_draw_arc(cx, cy, r - 3, g->start_angle, fill_angle, fc, fill_seg);
        renderer_draw_arc(cx, cy, r - 4, g->start_angle, fill_angle, fc, fill_seg);
        renderer_draw_arc(cx, cy, r - 5, g->start_angle, fill_angle, fc, fill_seg);
    }

    /* Minor tick marks */
    if (minor_step > 0.0f && major_step > 0.0f) {
        for (float tv = g->min_val; tv <= g->max_val + 0.001f; tv += minor_step) {
            float rem = fmodf(tv - g->min_val, major_step);
            if (rem < minor_step * 0.1f)
                continue;
            float ta  = g->start_angle + g->sweep_angle * ((tv - g->min_val) / range);
            float ca  = cosf(ta), sa = sinf(ta);
            renderer_draw_line(cx + (int)(ca * (float)(r - 7)),  cy + (int)(sa * (float)(r - 7)),
                               cx + (int)(ca * (float)(r - 13)), cy + (int)(sa * (float)(r - 13)),
                               t->text_secondary);
        }
    }

    /* Major tick marks + optional labels */
    if (major_step > 0.0f) {
        for (float tv = g->min_val; tv <= g->max_val + 0.001f; tv += major_step) {
            float ta = g->start_angle + g->sweep_angle * ((tv - g->min_val) / range);
            float ca = cosf(ta), sa = sinf(ta);
            renderer_draw_line(cx + (int)(ca * (float)(r - 7)),  cy + (int)(sa * (float)(r - 7)),
                               cx + (int)(ca * (float)(r - 20)), cy + (int)(sa * (float)(r - 20)),
                               t->text_primary);
            if (label_scale > 0.0f) {
                char lbuf[6];
                snprintf(lbuf, sizeof(lbuf), "%.0f", tv / label_scale);
                int lx = cx + (int)(ca * (float)(r - 30)) - (int)(font_str_width(lbuf, 1) / 2);
                int ly = cy + (int)(sa * (float)(r - 30)) - 4;
                font_draw_str(lx, ly, lbuf, t->text_secondary, 1);
            }
        }
    }

    /* Filled triangle needle */
    float cos_n = cosf(fill_angle), sin_n = sinf(fill_angle);
    float cos_p = -sin_n, sin_p = cos_n;
    float tip_x = (float)cx + cos_n * (float)(r - 10);
    float tip_y = (float)cy + sin_n * (float)(r - 10);
    float base_d = (float)r * 0.18f;
    float half_w = 5.0f;
    /* Shadow: slightly wider in a dim color */
    renderer_draw_filled_tri(
        tip_x, tip_y,
        (float)cx + cos_n * base_d + cos_p * (half_w + 2.0f),
        (float)cy + sin_n * base_d + sin_p * (half_w + 2.0f),
        (float)cx + cos_n * base_d - cos_p * (half_w + 2.0f),
        (float)cy + sin_n * base_d - sin_p * (half_w + 2.0f),
        RGBA(60, 60, 60, 200)
    );
    renderer_draw_filled_tri(
        tip_x, tip_y,
        (float)cx + cos_n * base_d + cos_p * half_w,
        (float)cy + sin_n * base_d + sin_p * half_w,
        (float)cx + cos_n * base_d - cos_p * half_w,
        (float)cy + sin_n * base_d - sin_p * half_w,
        t->needle
    );

    /* Center hub: three layered rects */
    renderer_draw_rect(cx - 7, cy - 7, 14, 14, t->gauge_track);
    renderer_draw_rect(cx - 5, cy - 5, 10, 10, t->accent);
    renderer_draw_rect(cx - 3, cy - 3, 6,  6,  t->needle);
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
