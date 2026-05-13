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

/* --- Digital dashboard --- */

void dashboard_render_digital(const VehicleState *vs) {
    const Theme *t = theme_current();

    /* RPM bar + number (left half) */
    gauge_draw_label(10, 14, "RPM", t->text_secondary, 1);
    gauge_draw_bar(10, 24, 220, 16,
                   vs->rpm, 0.0f, 8000.0f,
                   t->gauge_fill, t->gauge_track);
    gauge_draw_numeric(10, 44, vs->rpm, "%.0f", "rpm", t->text_primary, 2);

    /* Speed (right half) */
    gauge_draw_label(260, 14, "SPEED", t->text_secondary, 1);
    gauge_draw_numeric(260, 30, vs->speed_kmh, "%.0f", "km/h", t->text_primary, 3);

    draw_divider(0, 100, SCREEN_W);

    /* Second row: coolant temp, throttle, engine load */
    /* Coolant */
    gauge_draw_label(10, 110, "COOLANT", t->text_secondary, 1);
    uint32_t ct_color = (vs->coolant_temp_c > 100.0f) ? t->danger :
                        (vs->coolant_temp_c > 90.0f)  ? t->warn : t->text_primary;
    gauge_draw_numeric(10, 124, vs->coolant_temp_c, "%.0f", "C", ct_color, 2);
    gauge_draw_bar(10, 150, 130, 8,
                   vs->coolant_temp_c, -40.0f, 120.0f,
                   ct_color, t->gauge_track);

    /* Throttle */
    gauge_draw_label(170, 110, "THROTTLE", t->text_secondary, 1);
    gauge_draw_numeric(170, 124, vs->throttle_pct, "%.1f", "%", t->text_primary, 2);
    gauge_draw_bar(170, 150, 130, 8,
                   vs->throttle_pct, 0.0f, 100.0f,
                   t->gauge_fill, t->gauge_track);

    /* Engine load */
    gauge_draw_label(330, 110, "LOAD", t->text_secondary, 1);
    gauge_draw_numeric(330, 124, vs->engine_load_pct, "%.1f", "%", t->text_primary, 2);
    gauge_draw_bar(330, 150, 130, 8,
                   vs->engine_load_pct, 0.0f, 100.0f,
                   t->gauge_fill, t->gauge_track);

    draw_divider(0, 170, SCREEN_W);

    /* Third row: IAT, voltage */
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

    /* Static so anim_val persists between frames for smooth animation */
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

    /* Bottom strip */
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

    /* 0-100 km/h timer state machine */
    if (!perf->timing_active && vs->speed_kmh < 5.0f && vs->throttle_pct > 80.0f) {
        perf->timing_active    = 1;
        perf->accel_start_ms   = 0; /* set by app with real timestamp */
        perf->accel_start_speed = vs->speed_kmh;
    }
    if (perf->timing_active && vs->speed_kmh >= 100.0f) {
        perf->timing_active  = 0;
        perf->accel_end_speed = vs->speed_kmh;
    }

    if (vs->rpm > perf->peak_rpm)
        perf->peak_rpm = vs->rpm;

    gauge_draw_label(10, 10, "PERFORMANCE", t->accent, 1);
    draw_divider(0, 22, SCREEN_W);

    /* 0-100 timer */
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

    /* Peak RPM */
    gauge_draw_label(10, 110, "PEAK RPM", t->text_secondary, 1);
    gauge_draw_numeric(10, 124, perf->peak_rpm, "%.0f", "rpm", t->text_primary, 2);

    /* Current speed bar */
    gauge_draw_label(10, 170, "CURRENT SPEED", t->text_secondary, 1);
    gauge_draw_bar(10, 182, 460, 20,
                   vs->speed_kmh, 0.0f, 200.0f,
                   t->gauge_fill, t->gauge_track);
    gauge_draw_numeric(10, 210, vs->speed_kmh, "%.0f", "km/h", t->text_primary, 1);

    /* Throttle bar */
    gauge_draw_label(10, 234, "THROTTLE", t->text_secondary, 1);
    gauge_draw_bar(10, 244, 460, 10,
                   vs->throttle_pct, 0.0f, 100.0f,
                   t->accent, t->gauge_track);
}

/* --- Status bar (bottom) --- */

void dashboard_render_status_bar(const VehicleState *vs, DashMode mode, int connected) {
    const Theme *t = theme_current();
    static const char *mode_names[] = {"DIGITAL", "ANALOG", "DIAG", "PERF"};

    renderer_draw_rect(0, 260, SCREEN_W, 12, RGBA(8, 8, 8, 255));

    /* Mode indicator */
    font_draw_str(4, 261, mode_names[mode], t->accent, 1);

    /* Connection status */
    uint32_t conn_col = connected ? COLOR_GREEN : t->danger;
    font_draw_str(80, 261, connected ? "OBD OK" : "NO OBD", conn_col, 1);

    /* Theme name */
    font_draw_str(200, 261, theme_current()->name, t->text_secondary, 1);

    /* Hotkey hints */
    font_draw_str(310, 261, "L/R:MODE  X:THEME  SEL+STA:CFG", t->text_secondary, 1);

    (void)vs;
}

/* --- Top-level dispatcher --- */

void dashboard_init(void) { /* nothing to init statically */ }

void dashboard_render(const VehicleState *vs, const DtcList *dtc, DashMode mode) {
    static PerfState perf = {0};

    switch (mode) {
        case DASH_MODE_DIGITAL:      dashboard_render_digital(vs);           break;
        case DASH_MODE_ANALOG:       dashboard_render_analog(vs);            break;
        case DASH_MODE_DIAGNOSTICS:  dashboard_render_diagnostics(vs, dtc);  break;
        case DASH_MODE_PERFORMANCE:  dashboard_render_performance(vs, &perf); break;
        default: break;
    }
}
