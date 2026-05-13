#include <stdio.h>
#include <math.h>
#include "ui/dashboard.h"
#include "ui/renderer.h"
#include "ui/gauge.h"
#include "ui/themes.h"
#include "utils/font.h"
#include "telemetry/units.h"

#define PI_F 3.14159265f

/* --- Shared layout helpers --- */

static void draw_divider(int x, int y, int w) {
    renderer_draw_rect(x, y, w, 1, theme_current()->text_secondary);
}

static void draw_warn_if_stale(int x, int y, int stale) {
    if (stale)
        font_draw_str(x, y, "---", theme_current()->text_secondary, 1);
}

/* Bidirectional bar for fuel trim: centered at 0, fills left (negative) or right (positive).
   scale_pct sets what value = full half-bar (e.g. 25 means ±25% = full bar).
   Colors: green <5%, warn 5-10%, danger >10%. */
static void draw_fuel_trim_row(int x, int y, const char *label,
                               float val, float scale_pct, const Theme *t) {
    font_draw_str(x, y + 2, label, t->text_secondary, 1);

    int bar_x  = x + 46;
    int bar_w  = 320;
    int bar_h  = 14;
    int center = bar_x + bar_w / 2;

    renderer_draw_rect(bar_x, y, bar_w, bar_h, t->gauge_track);
    /* Center tick */
    renderer_draw_rect(center - 1, y - 2, 2, bar_h + 4, t->text_secondary);

    float pct = val / scale_pct;
    if (pct >  1.0f) pct =  1.0f;
    if (pct < -1.0f) pct = -1.0f;
    int fill = (int)(pct * (bar_w / 2));

    uint32_t fc = (val > 10.0f || val < -10.0f) ? t->danger :
                  (val >  5.0f || val <  -5.0f) ? t->warn : t->gauge_fill;

    if (fill > 0)
        renderer_draw_rect(center,        y + 1, fill,  bar_h - 2, fc);
    else if (fill < 0)
        renderer_draw_rect(center + fill, y + 1, -fill, bar_h - 2, fc);

    char buf[10];
    snprintf(buf, sizeof(buf), "%+.1f%%", val);
    font_draw_str(bar_x + bar_w + 8, y + 2, buf, fc, 1);
}

/* Runtime seconds → "Hh MMm SSs" string */
static void fmt_runtime(float s, char *buf, int sz) {
    int total = (int)s;
    int h  = total / 3600;
    int m  = (total % 3600) / 60;
    int ss = total % 60;
    if (h > 0)
        snprintf(buf, sz, "%dh %02dm %02ds", h, m, ss);
    else
        snprintf(buf, sz, "%dm %02ds", m, ss);
}

/* --- Digital dashboard --- */

void dashboard_render_digital(const VehicleState *vs) {
    const Theme *t = theme_current();

    gauge_draw_label(10, 14, "RPM", t->text_secondary, 1);
    gauge_draw_bar(10, 24, 220, 16,
                   vs->rpm, 0.0f, 8000.0f,
                   t->gauge_fill, t->gauge_track);
    gauge_draw_numeric(10, 44, vs->rpm, "%.0f", "rpm", t->text_primary, 2);

    gauge_draw_label(260, 14, "SPEED", t->text_secondary, 1);
    gauge_draw_numeric(260, 30, vs->speed_kmh, "%.0f", "km/h", t->text_primary, 3);

    draw_divider(0, 100, SCREEN_W);

    gauge_draw_label(10, 110, "COOLANT", t->text_secondary, 1);
    uint32_t ct_color = (vs->coolant_temp_c > 100.0f) ? t->danger :
                        (vs->coolant_temp_c > 90.0f)  ? t->warn : t->text_primary;
    gauge_draw_numeric(10, 124, vs->coolant_temp_c, "%.0f", "C", ct_color, 2);
    gauge_draw_bar(10, 150, 130, 8,
                   vs->coolant_temp_c, -40.0f, 120.0f,
                   ct_color, t->gauge_track);

    gauge_draw_label(170, 110, "THROTTLE", t->text_secondary, 1);
    gauge_draw_numeric(170, 124, vs->throttle_pct, "%.1f", "%", t->text_primary, 2);
    gauge_draw_bar(170, 150, 130, 8,
                   vs->throttle_pct, 0.0f, 100.0f,
                   t->gauge_fill, t->gauge_track);

    gauge_draw_label(330, 110, "LOAD", t->text_secondary, 1);
    gauge_draw_numeric(330, 124, vs->engine_load_pct, "%.1f", "%", t->text_primary, 2);
    gauge_draw_bar(330, 150, 130, 8,
                   vs->engine_load_pct, 0.0f, 100.0f,
                   t->gauge_fill, t->gauge_track);

    draw_divider(0, 170, SCREEN_W);

    gauge_draw_label(10, 180, "IAT", t->text_secondary, 1);
    gauge_draw_numeric(10, 194, vs->iat_c, "%.0f", "C", t->text_primary, 1);

    gauge_draw_label(120, 180, "VOLTAGE", t->text_secondary, 1);
    uint32_t v_color = (vs->voltage_v < 11.5f) ? t->danger :
                       (vs->voltage_v < 12.0f) ? t->warn : t->text_primary;
    gauge_draw_numeric(120, 194, vs->voltage_v, "%.2f", "V", v_color, 1);

    (void)draw_warn_if_stale;
}

/* --- Analog gauge dashboard --- */

void dashboard_render_analog(const VehicleState *vs) {
    const Theme *t = theme_current();

    static GaugeDef tach = {
        .cx = 136, .cy = 136, .radius = 110,
        .min_val = 0.0f, .max_val = 8000.0f, .redline = 6500.0f,
        .start_angle = PI_F * 0.75f,
        .sweep_angle  = PI_F * 1.5f,
        .anim_val = 0.0f,
    };
    gauge_draw_analog(&tach, vs->rpm);
    gauge_draw_numeric(tach.cx - 30, tach.cy + tach.radius - 30,
                       vs->rpm, "%.0f", "rpm", t->text_primary, 1);
    gauge_draw_label(tach.cx - 16, tach.cy - 10, "RPM", t->text_secondary, 1);

    static GaugeDef spd = {
        .cx = 344, .cy = 136, .radius = 110,
        .min_val = 0.0f, .max_val = 240.0f, .redline = 999.0f,
        .start_angle = PI_F * 0.75f,
        .sweep_angle  = PI_F * 1.5f,
        .anim_val = 0.0f,
    };
    gauge_draw_analog(&spd, vs->speed_kmh);
    gauge_draw_numeric(spd.cx - 30, spd.cy + spd.radius - 30,
                       vs->speed_kmh, "%.0f", "km/h", t->text_primary, 1);
    gauge_draw_label(spd.cx - 20, spd.cy - 10, "SPEED", t->text_secondary, 1);

    draw_divider(0, 248, SCREEN_W);
    gauge_draw_label(10,  254, "COOL", t->text_secondary, 1);
    gauge_draw_numeric(44, 254, vs->coolant_temp_c, "%.0f", "C", t->text_primary, 1);

    gauge_draw_label(120, 254, "LOAD", t->text_secondary, 1);
    gauge_draw_numeric(154, 254, vs->engine_load_pct, "%.0f", "%", t->text_primary, 1);

    gauge_draw_label(220, 254, "VOLT", t->text_secondary, 1);
    gauge_draw_numeric(254, 254, vs->voltage_v, "%.1f", "V", t->text_primary, 1);
}

/* --- Diagnostics dashboard --- */

void dashboard_render_diagnostics(const VehicleState *vs, const DtcList *dtc) {
    const Theme *t = theme_current();

    gauge_draw_label(10, 10, "DIAGNOSTIC TROUBLE CODES", t->accent, 1);
    draw_divider(0, 22, SCREEN_W);

    if (!dtc->read_ok) {
        font_draw_str(10, 40, "Reading DTCs...", t->text_secondary, 1);
    } else if (dtc->count == 0) {
        font_draw_str(10, 40, "No fault codes. System OK.", t->text_primary, 1);
        renderer_draw_rect(10, 60, 12, 12, COLOR_GREEN);
    } else {
        char line[32];
        for (int i = 0; i < dtc->count && i < 10; i++) {
            snprintf(line, sizeof(line), "P%d  %s", i + 1, dtc->codes[i]);
            uint32_t row_col = (i % 2 == 0) ? t->danger : t->warn;
            font_draw_str(10, 40 + i * 18, line, row_col, 1);
        }
    }

    draw_divider(0, 220, SCREEN_W);
    gauge_draw_label(10, 228, "LIVE: RPM", t->text_secondary, 1);
    gauge_draw_numeric(80, 228, vs->rpm, "%.0f", "", t->text_primary, 1);
    gauge_draw_label(180, 228, "SPD", t->text_secondary, 1);
    gauge_draw_numeric(220, 228, vs->speed_kmh, "%.0f", "", t->text_primary, 1);
}

/* --- Performance dashboard --- */

void dashboard_render_performance(const VehicleState *vs, PerfState *perf) {
    const Theme *t = theme_current();

    if (!perf->timing_active && vs->speed_kmh < 5.0f && vs->throttle_pct > 80.0f) {
        perf->timing_active     = 1;
        perf->accel_start_ms    = 0;
        perf->accel_start_speed = vs->speed_kmh;
    }
    if (perf->timing_active && vs->speed_kmh >= 100.0f) {
        perf->timing_active   = 0;
        perf->accel_end_speed = vs->speed_kmh;
    }
    if (vs->rpm > perf->peak_rpm)
        perf->peak_rpm = vs->rpm;

    gauge_draw_label(10, 10, "PERFORMANCE", t->accent, 1);
    draw_divider(0, 22, SCREEN_W);

    gauge_draw_label(10, 35, "0-100 km/h", t->text_secondary, 1);
    if (perf->best_0_100 > 0.0f) {
        char buf[24];
        snprintf(buf, sizeof(buf), "%.2f s", perf->best_0_100);
        font_draw_str(10, 52, buf, t->accent, 2);
    } else {
        font_draw_str(10, 52, "--- s", t->text_secondary, 2);
    }
    if (perf->timing_active) {
        font_draw_str(300, 52, "TIMING", t->warn, 1);
        renderer_draw_rect(296, 50, 4, 12, t->warn);
    }

    draw_divider(0, 100, SCREEN_W);

    gauge_draw_label(10, 110, "PEAK RPM", t->text_secondary, 1);
    gauge_draw_numeric(10, 124, perf->peak_rpm, "%.0f", "rpm", t->text_primary, 2);

    gauge_draw_label(10, 170, "CURRENT SPEED", t->text_secondary, 1);
    gauge_draw_bar(10, 182, 460, 20,
                   vs->speed_kmh, 0.0f, 200.0f,
                   t->gauge_fill, t->gauge_track);
    gauge_draw_numeric(10, 210, vs->speed_kmh, "%.0f", "km/h", t->text_primary, 1);

    gauge_draw_label(10, 234, "THROTTLE", t->text_secondary, 1);
    gauge_draw_bar(10, 244, 460, 10,
                   vs->throttle_pct, 0.0f, 100.0f,
                   t->accent, t->gauge_track);
}

/* --- Engine internals dashboard ---
   Focus: fuel trims, timing advance - data you can't see on a stock cluster. */

void dashboard_render_engine(const VehicleState *vs) {
    const Theme *t = theme_current();

    gauge_draw_label(10, 6, "ENGINE INTERNALS", t->accent, 1);
    draw_divider(0, 18, SCREEN_W);

    /* Fuel trim section */
    gauge_draw_label(10, 22, "FUEL TRIMS  (< 5% normal,  > 10% fault)", t->text_secondary, 1);
    draw_fuel_trim_row(10, 34, "STFT", vs->stft_pct, 25.0f, t);
    draw_fuel_trim_row(10, 58, "LTFT", vs->ltft_pct, 25.0f, t);

    draw_divider(0, 80, SCREEN_W);

    /* Timing advance (left) + RPM & load context (right) */
    int mid = 240;
    gauge_draw_label(10, 86, "TIMING ADVANCE", t->text_secondary, 1);

    char tbuf[12];
    snprintf(tbuf, sizeof(tbuf), "%+.1f", vs->timing_adv_deg);
    font_draw_str(10, 98, tbuf, t->text_primary, 3);       /* scale=3 → 24px tall */
    font_draw_str(10, 126, "deg BTDC", t->text_secondary, 1);

    renderer_draw_rect(mid, 82, 1, 56, t->text_secondary); /* vertical divider */

    gauge_draw_label(mid + 10, 86, "RPM", t->text_secondary, 1);
    gauge_draw_numeric(mid + 10, 98, vs->rpm, "%.0f", "", t->text_primary, 2);

    gauge_draw_label(mid + 10, 120, "LOAD", t->text_secondary, 1);
    gauge_draw_numeric(mid + 64, 120, vs->engine_load_pct, "%.0f", "%", t->text_primary, 1);

    gauge_draw_label(mid + 130, 120, "THROTTLE", t->text_secondary, 1);
    gauge_draw_numeric(mid + 210, 120, vs->throttle_pct, "%.0f", "%", t->text_primary, 1);

    draw_divider(0, 140, SCREEN_W);

    /* Small values row */
    gauge_draw_label(10, 146, "IAT", t->text_secondary, 1);
    gauge_draw_numeric(42, 146, vs->iat_c, "%.0f", "C", t->text_primary, 1);

    gauge_draw_label(110, 146, "COOLANT", t->text_secondary, 1);
    uint32_t ct_col = (vs->coolant_temp_c > 100.0f) ? t->danger :
                      (vs->coolant_temp_c > 90.0f)  ? t->warn : t->text_primary;
    gauge_draw_numeric(174, 146, vs->coolant_temp_c, "%.0f", "C", ct_col, 1);

    gauge_draw_label(240, 146, "VOLTAGE", t->text_secondary, 1);
    uint32_t v_col = (vs->voltage_v < 11.5f) ? t->danger :
                     (vs->voltage_v < 12.0f) ? t->warn : t->text_primary;
    gauge_draw_numeric(306, 146, vs->voltage_v, "%.2f", "V", v_col, 1);

    draw_divider(0, 162, SCREEN_W);

    /* Fuel level bar */
    gauge_draw_label(10, 168, "FUEL", t->text_secondary, 1);
    uint32_t fuel_col = (vs->fuel_level_pct < 15.0f) ? t->danger :
                        (vs->fuel_level_pct < 25.0f) ? t->warn : t->gauge_fill;
    gauge_draw_bar(52, 168, 350, 12,
                   vs->fuel_level_pct, 0.0f, 100.0f, fuel_col, t->gauge_track);
    char fbuf[8];
    snprintf(fbuf, sizeof(fbuf), "%.0f%%", vs->fuel_level_pct);
    font_draw_str(410, 168, fbuf, fuel_col, 1);

    /* Ambient temp */
    gauge_draw_label(10, 188, "AMBIENT", t->text_secondary, 1);
    gauge_draw_numeric(74, 188, vs->ambient_temp_c, "%.1f", "C", t->text_primary, 1);

    gauge_draw_label(200, 188, "ENGINE RUNTIME", t->text_secondary, 1);
    char rtbuf[24];
    fmt_runtime(vs->runtime_s, rtbuf, sizeof(rtbuf));
    font_draw_str(344, 188, rtbuf, t->text_primary, 1);
}

/* --- Trip & status dashboard ---
   Focus: runtime, fuel, ambient - the "how long have I been driving" view. */

static int g_trip_reset = 0;
void dashboard_trip_reset_session(void) { g_trip_reset = 1; }

void dashboard_render_trip(const VehicleState *vs) {
    const Theme *t = theme_current();

    static float sess_max_speed = 0.0f;
    static float sess_max_rpm   = 0.0f;
    if (g_trip_reset) {
        sess_max_speed = 0.0f;
        sess_max_rpm   = 0.0f;
        g_trip_reset   = 0;
    }
    if (vs->speed_kmh > sess_max_speed) sess_max_speed = vs->speed_kmh;
    if (vs->rpm       > sess_max_rpm)   sess_max_rpm   = vs->rpm;

    gauge_draw_label(10, 6, "TRIP & STATUS", t->accent, 1);
    draw_divider(0, 18, SCREEN_W);

    /* Engine runtime - prominent */
    gauge_draw_label(10, 24, "ENGINE RUNTIME", t->text_secondary, 1);
    char rtbuf[24];
    fmt_runtime(vs->runtime_s, rtbuf, sizeof(rtbuf));
    font_draw_str(10, 36, rtbuf, t->text_primary, 2);

    draw_divider(0, 68, SCREEN_W);

    /* Fuel level - full width bar + large % */
    gauge_draw_label(10, 74, "FUEL LEVEL", t->text_secondary, 1);
    uint32_t fuel_col = (vs->fuel_level_pct < 15.0f) ? t->danger :
                        (vs->fuel_level_pct < 25.0f) ? t->warn : t->gauge_fill;

    char fbuf[8];
    snprintf(fbuf, sizeof(fbuf), "%.0f%%", vs->fuel_level_pct);
    font_draw_str(10, 86, fbuf, fuel_col, 3);             /* large % number */
    gauge_draw_bar(10, 116, 460, 16,
                   vs->fuel_level_pct, 0.0f, 100.0f, fuel_col, t->gauge_track);

    draw_divider(0, 140, SCREEN_W);

    /* Temperature block: ambient vs IAT vs coolant */
    gauge_draw_label(10, 146, "AMBIENT", t->text_secondary, 1);
    gauge_draw_numeric(74, 146, vs->ambient_temp_c, "%.1f", "C", t->text_primary, 1);

    gauge_draw_label(170, 146, "IAT", t->text_secondary, 1);
    gauge_draw_numeric(202, 146, vs->iat_c, "%.0f", "C", t->text_primary, 1);

    gauge_draw_label(270, 146, "COOLANT", t->text_secondary, 1);
    uint32_t ct_col = (vs->coolant_temp_c > 100.0f) ? t->danger :
                      (vs->coolant_temp_c > 90.0f)  ? t->warn : t->text_primary;
    gauge_draw_numeric(334, 146, vs->coolant_temp_c, "%.0f", "C", ct_col, 1);

    gauge_draw_label(10, 164, "VOLTAGE", t->text_secondary, 1);
    uint32_t v_col = (vs->voltage_v < 11.5f) ? t->danger :
                     (vs->voltage_v < 12.0f) ? t->warn : t->text_primary;
    gauge_draw_numeric(76, 164, vs->voltage_v, "%.2f", "V", v_col, 1);

    draw_divider(0, 180, SCREEN_W);

    /* Session max values */
    gauge_draw_label(10, 186, "SESSION PEAK", t->accent, 1);

    gauge_draw_label(10, 200, "SPEED", t->text_secondary, 1);
    gauge_draw_numeric(54, 200, sess_max_speed, "%.0f", "km/h", t->text_primary, 1);

    gauge_draw_label(160, 200, "RPM", t->text_secondary, 1);
    gauge_draw_numeric(196, 200, sess_max_rpm, "%.0f", "", t->text_primary, 1);

    /* Reset button hint */
    font_draw_str(320, 200, "Triangle: reset session", t->text_secondary, 1);
}

/* --- Status bar (bottom) --- */

void dashboard_render_status_bar(const VehicleState *vs, DashMode mode, int connected) {
    const Theme *t = theme_current();
    static const char *mode_names[] = {
        "DIGITAL", "ANALOG", "DIAG", "PERF", "ENGINE", "TRIP"
    };

    renderer_draw_rect(0, 260, SCREEN_W, 12, RGBA(8, 8, 8, 255));

    font_draw_str(4, 261, mode_names[mode], t->accent, 1);

    /* Connection status (2 = demo mode) */
    uint32_t conn_col = (connected == 2) ? t->warn :
                        (connected == 1) ? COLOR_GREEN : t->danger;
    const char *conn_str = (connected == 2) ? "DEMO" :
                           (connected == 1) ? "OBD OK" : "NO OBD";
    font_draw_str(80, 261, conn_str, conn_col, 1);

    font_draw_str(200, 261, theme_current()->name, t->text_secondary, 1);
    font_draw_str(310, 261, "L/R:MODE  X:THEME  SEL+STA:CFG", t->text_secondary, 1);

    (void)vs;
}

/* --- Top-level dispatcher --- */

void dashboard_init(void) {}

void dashboard_render(const VehicleState *vs, const DtcList *dtc, DashMode mode) {
    static PerfState perf = {0};

    switch (mode) {
        case DASH_MODE_DIGITAL:      dashboard_render_digital(vs);            break;
        case DASH_MODE_ANALOG:       dashboard_render_analog(vs);             break;
        case DASH_MODE_DIAGNOSTICS:  dashboard_render_diagnostics(vs, dtc);   break;
        case DASH_MODE_PERFORMANCE:  dashboard_render_performance(vs, &perf); break;
        case DASH_MODE_ENGINE:       dashboard_render_engine(vs);             break;
        case DASH_MODE_TRIP:         dashboard_render_trip(vs);               break;
        default: break;
    }
}
