#include <stdio.h>
#include <math.h>
#include "ui/dashboard.h"
#include "ui/renderer.h"
#include "ui/gauge.h"
#include "ui/themes.h"
#include "utils/font.h"
#include "utils/time.h"
#include "telemetry/units.h"

#define VTEC_RPM_THRESHOLD 5800.0f

#define PI_F 3.14159265f

/* ------------------------------------------------------------------ */
/* Shared helpers                                                      */
/* ------------------------------------------------------------------ */

static void draw_divider(int x, int y, int w) {
    renderer_draw_rect(x, y, w, 1, theme_current()->text_secondary);
}

/* Value with N/A fallback based on supported status. */
static void draw_fa(int x, int y, float val, const char *fmt, const char *unit,
                    uint8_t status, uint32_t col, int scale, const Theme *t) {
    if (status == 2)
        font_draw_str(x, y, "N/A", RGBA(70, 70, 70, 255), scale);
    else if (status == 0)
        font_draw_str(x, y, "...", t->text_secondary, scale);
    else
        gauge_draw_numeric(x, y, val, fmt, unit, col, scale);
}

/* Bidirectional fuel trim bar centred at 0%; green <5%, amber 5-10%, red >10%. */
static void draw_fuel_trim_row(int x, int y, const char *label,
                               float val, uint8_t status, const Theme *t) {
    font_draw_str(x, y + 2, label, t->text_secondary, 1);

    int bar_x  = x + 46;
    int bar_w  = 320;
    int bar_h  = 14;
    int center = bar_x + bar_w / 2;

    if (status == 2) {
        font_draw_str(bar_x, y + 2, "N/A - PID not supported by this ECU",
                      RGBA(70, 70, 70, 255), 1);
        return;
    }
    if (status == 0) {
        font_draw_str(bar_x, y + 2, "...", t->text_secondary, 1);
        return;
    }

    renderer_draw_rect(bar_x, y, bar_w, bar_h, t->gauge_track);
    renderer_draw_rect(center - 1, y - 2, 2, bar_h + 4, t->text_secondary);

    float pct = val / 25.0f;
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

/* Bidirectional G-force bar centred at 0; ±2G full scale. */
static void draw_g_bar(int x, int y, int w, float g_val, const Theme *t) {
    int center = x + w / 2;
    int bar_h  = 12;
    renderer_draw_rect(x, y, w, bar_h, t->gauge_track);
    renderer_draw_rect(center - 1, y - 2, 2, bar_h + 4, t->text_secondary);

    float pct = g_val / 2.0f;
    if (pct >  1.0f) pct =  1.0f;
    if (pct < -1.0f) pct = -1.0f;
    int fill = (int)(pct * (w / 2));

    uint32_t col = (g_val >= 0.0f) ? t->accent : t->warn;
    if (fill > 0)
        renderer_draw_rect(center, y + 1, fill, bar_h - 2, col);
    else if (fill < 0)
        renderer_draw_rect(center + fill, y + 1, -fill, bar_h - 2, col);
}

/* Estimated gear indicator. */
static void draw_gear(int x, int y, int gear, int scale, const Theme *t) {
    char buf[4];
    uint32_t col;
    if (gear == 0) {
        buf[0] = 'N'; buf[1] = '\0';
        col = t->text_secondary;
    } else if (gear < 0) {
        buf[0] = '?'; buf[1] = '\0';
        col = RGBA(70, 70, 70, 255);
    } else {
        buf[0] = (char)('0' + gear);
        buf[1] = '\0';
        col = (gear >= 5) ? COLOR_GREEN : t->text_primary;
    }
    font_draw_str(x, y, buf, col, scale);
}

/* Format runtime seconds -> "Hh MMm SSs" */
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

/* Format minutes -> "Xh Ym" or "X min" */
static void fmt_minutes(float min_f, char *buf, int sz) {
    int m = (int)min_f;
    if (m >= 60)
        snprintf(buf, sz, "%dh %dm", m / 60, m % 60);
    else
        snprintf(buf, sz, "%d min", m);
}

/* ------------------------------------------------------------------ */
/* Digital dashboard                                                   */
/* ------------------------------------------------------------------ */

void dashboard_render_digital(const VehicleState *vs, const DerivedState *d) {
    const Theme *t = theme_current();
    int fs = t->font_scale;  /* 1=normal, 2=large */

    gauge_draw_label(10, 14, "RPM", t->text_secondary, 1);
    gauge_draw_bar(10, 24, 220, 16,
                   vs->rpm, 0.0f, 8000.0f, t->gauge_fill, t->gauge_track);
    gauge_draw_numeric(10, 44, vs->rpm, "%.0f", "rpm", t->text_primary, fs + 1);

    gauge_draw_label(260, 14, "SPEED", t->text_secondary, 1);
    gauge_draw_numeric(260, 30, vs->speed_kmh, "%.0f", "km/h", t->text_primary, fs + 2);

    /* Gear indicator top-right */
    gauge_draw_label(420, 14, "GEAR", t->text_secondary, 1);
    draw_gear(432, 30, d->gear, fs + 1, t);

    draw_divider(0, 100, SCREEN_W);

    gauge_draw_label(10, 110, "COOLANT", t->text_secondary, 1);
    uint32_t ct_col = (vs->coolant_temp_c > 100.0f) ? t->danger :
                      (vs->coolant_temp_c > 90.0f)  ? t->warn : t->text_primary;
    gauge_draw_numeric(10, 124, vs->coolant_temp_c, "%.0f", "C", ct_col, fs + 1);
    gauge_draw_bar(10, 150, 130, 8,
                   vs->coolant_temp_c, -40.0f, 120.0f, ct_col, t->gauge_track);

    gauge_draw_label(170, 110, "THROTTLE", t->text_secondary, 1);
    gauge_draw_numeric(170, 124, vs->throttle_pct, "%.1f", "%", t->text_primary, fs + 1);
    gauge_draw_bar(170, 150, 130, 8,
                   vs->throttle_pct, 0.0f, 100.0f, t->gauge_fill, t->gauge_track);

    gauge_draw_label(330, 110, "LOAD", t->text_secondary, 1);
    gauge_draw_numeric(330, 124, vs->engine_load_pct, "%.1f", "%", t->text_primary, fs + 1);
    gauge_draw_bar(330, 150, 130, 8,
                   vs->engine_load_pct, 0.0f, 100.0f, t->gauge_fill, t->gauge_track);

    draw_divider(0, 170, SCREEN_W);

    gauge_draw_label(10, 180, "IAT", t->text_secondary, 1);
    gauge_draw_numeric(10, 194, vs->iat_c, "%.0f", "C", t->text_primary, fs);

    gauge_draw_label(110, 180, "OIL", t->text_secondary, 1);
    draw_fa(142, 194, vs->oil_temp_c, "%.0f", "C",
            vs->supported[PID_OIL_TEMP], t->text_primary, fs, t);

    gauge_draw_label(220, 180, "VOLTAGE", t->text_secondary, 1);
    uint32_t v_col = (vs->voltage_v < 11.5f) ? t->danger :
                     (vs->voltage_v < 12.0f) ? t->warn : t->text_primary;
    gauge_draw_numeric(286, 194, vs->voltage_v, "%.2f", "V", v_col, 1);

    gauge_draw_label(370, 180, "FUEL", t->text_secondary, 1);
    uint32_t fl_col = (vs->fuel_level_pct < 15.0f) ? t->danger :
                      (vs->fuel_level_pct < 25.0f) ? t->warn : t->text_primary;
    draw_fa(400, 194, vs->fuel_level_pct, "%.0f", "%",
            vs->supported[PID_FUEL_LEVEL], fl_col, 1, t);

    /* Coasting / idle status */
    if (d->coasting)
        font_draw_str(10, 234, "COASTING", COLOR_GREEN, fs);
    else if (d->idle)
        font_draw_str(10, 234, "IDLE", t->text_secondary, fs);
}

/* ------------------------------------------------------------------ */
/* Analog gauge dashboard                                              */
/* ------------------------------------------------------------------ */

void dashboard_render_analog(const VehicleState *vs, const DerivedState *d) {
    const Theme *t = theme_current();
    int fs = t->font_scale;

    static GaugeDef tach = {
        .cx=136, .cy=136, .radius=110,
        .min_val=0.0f, .max_val=8000.0f, .redline=6500.0f,
        .start_angle=PI_F * 0.75f, .sweep_angle=PI_F * 1.5f, .anim_val=0.0f,
    };
    gauge_draw_analog(&tach, vs->rpm);
    gauge_draw_numeric(tach.cx - 30, tach.cy + tach.radius - 30,
                       vs->rpm, "%.0f", "rpm", t->text_primary, fs);
    gauge_draw_label(tach.cx - 16, tach.cy - 10, "RPM", t->text_secondary, 1);

    static GaugeDef spd = {
        .cx=344, .cy=136, .radius=110,
        .min_val=0.0f, .max_val=240.0f, .redline=999.0f,
        .start_angle=PI_F * 0.75f, .sweep_angle=PI_F * 1.5f, .anim_val=0.0f,
    };
    gauge_draw_analog(&spd, vs->speed_kmh);
    gauge_draw_numeric(spd.cx - 30, spd.cy + spd.radius - 30,
                       vs->speed_kmh, "%.0f", "km/h", t->text_primary, fs);
    gauge_draw_label(spd.cx - 20, spd.cy - 10, "SPEED", t->text_secondary, 1);

    /* Gear in centre between gauges */
    gauge_draw_label(224, 8, "GEAR", t->text_secondary, 1);
    draw_gear(228, 20, d->gear, 3, t);

    draw_divider(0, 244, SCREEN_W);
    gauge_draw_label(10,  249, "COOL", t->text_secondary, 1);
    gauge_draw_numeric(44, 249, vs->coolant_temp_c, "%.0f", "C", t->text_primary, 1);
    gauge_draw_label(120, 249, "LOAD", t->text_secondary, 1);
    gauge_draw_numeric(154,249, vs->engine_load_pct, "%.0f", "%", t->text_primary, 1);
    gauge_draw_label(220, 249, "VOLT", t->text_secondary, 1);
    gauge_draw_numeric(254,249, vs->voltage_v, "%.1f", "V", t->text_primary, 1);
    gauge_draw_label(320, 249, "OIL", t->text_secondary, 1);
    draw_fa(348, 249, vs->oil_temp_c, "%.0f", "C",
            vs->supported[PID_OIL_TEMP], t->text_primary, 1, t);
    gauge_draw_label(420, 249, "GEAR", t->text_secondary, 1);
    draw_gear(454, 249, d->gear, 1, t);
}

/* ------------------------------------------------------------------ */
/* Diagnostics dashboard                                               */
/* ------------------------------------------------------------------ */

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
        for (int i = 0; i < dtc->count && i < 8; i++) {
            snprintf(line, sizeof(line), "P%d  %s", i + 1, dtc->codes[i]);
            uint32_t rc = (i % 2 == 0) ? t->danger : t->warn;
            font_draw_str(10, 40 + i * 18, line, rc, 1);
        }
    }

    draw_divider(0, 192, SCREEN_W);

    gauge_draw_label(10, 198, "LIVE: RPM", t->text_secondary, 1);
    gauge_draw_numeric(80, 198, vs->rpm, "%.0f", "", t->text_primary, 1);
    gauge_draw_label(150, 198, "SPD", t->text_secondary, 1);
    gauge_draw_numeric(182, 198, vs->speed_kmh, "%.0f", "", t->text_primary, 1);

    draw_divider(0, 212, SCREEN_W);

    gauge_draw_label(10, 218, "MIL ON", t->text_secondary, 1);
    {
        char buf[24];
        if (vs->supported[PID_MIL_TIME] == 1) {
            fmt_minutes(vs->mil_time_min, buf, sizeof(buf));
            uint32_t mc = (vs->mil_time_min > 0) ? t->warn : t->text_primary;
            font_draw_str(64, 218, buf, mc, 1);
        } else {
            font_draw_str(64, 218, vs->supported[PID_MIL_TIME] == 2 ? "N/A" : "...",
                          RGBA(70,70,70,255), 1);
        }
    }

    gauge_draw_label(200, 218, "DIST MIL", t->text_secondary, 1);
    {
        char buf[16];
        if (vs->supported[PID_MIL_DIST] == 1) {
            snprintf(buf, sizeof(buf), "%.0f km", vs->mil_dist_km);
            uint32_t mc = (vs->mil_dist_km > 0) ? t->warn : t->text_primary;
            font_draw_str(272, 218, buf, mc, 1);
        } else {
            font_draw_str(272, 218, vs->supported[PID_MIL_DIST] == 2 ? "N/A" : "...",
                          RGBA(70,70,70,255), 1);
        }
    }

    gauge_draw_label(10, 234, "SINCE CLR", t->text_secondary, 1);
    {
        char buf[24];
        if (vs->supported[PID_CLR_TIME] == 1) {
            fmt_minutes(vs->clr_time_min, buf, sizeof(buf));
            font_draw_str(82, 234, buf, t->text_primary, 1);
        } else {
            font_draw_str(82, 234, vs->supported[PID_CLR_TIME] == 2 ? "N/A" : "...",
                          RGBA(70,70,70,255), 1);
        }
    }

    gauge_draw_label(200, 234, "DIST CLR", t->text_secondary, 1);
    {
        char buf[16];
        if (vs->supported[PID_CLR_DIST] == 1) {
            snprintf(buf, sizeof(buf), "%.0f km", vs->clr_dist_km);
            font_draw_str(272, 234, buf, t->text_primary, 1);
        } else {
            font_draw_str(272, 234, vs->supported[PID_CLR_DIST] == 2 ? "N/A" : "...",
                          RGBA(70,70,70,255), 1);
        }
    }
}

/* ------------------------------------------------------------------ */
/* Performance dashboard                                               */
/* ------------------------------------------------------------------ */

void dashboard_render_performance(const VehicleState *vs, PerfState *perf,
                                  const DerivedState *d) {
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
    if (vs->rpm > perf->peak_rpm) perf->peak_rpm = vs->rpm;

    gauge_draw_label(10, 10, "PERFORMANCE", t->accent, 1);
    draw_divider(0, 22, SCREEN_W);

    /* Gear top-left */
    gauge_draw_label(10, 28, "GEAR", t->text_secondary, 1);
    draw_gear(10, 40, d->gear, 3, t);

    /* 0-100 timer top-right */
    gauge_draw_label(200, 28, "0-100 km/h", t->text_secondary, 1);
    if (perf->best_0_100 > 0.0f) {
        char buf[24];
        snprintf(buf, sizeof(buf), "%.2f s", perf->best_0_100);
        font_draw_str(200, 40, buf, t->accent, 2);
    } else {
        font_draw_str(200, 40, "--- s", t->text_secondary, 2);
    }
    if (perf->timing_active)
        font_draw_str(380, 40, "TIMING", t->warn, 1);

    draw_divider(0, 78, SCREEN_W);

    /* G-force bar */
    gauge_draw_label(10, 84, "G-FORCE", t->text_secondary, 1);
    draw_g_bar(10, 96, 460, d->accel_g, t);
    font_draw_str(10,  112, "-2G", RGBA(70,70,70,255), 1);
    font_draw_str(234, 112, "0", RGBA(70,70,70,255), 1);
    font_draw_str(450, 112, "+2G", RGBA(70,70,70,255), 1);
    {
        char gbuf[12];
        uint32_t gcol = (d->accel_g > 0.05f)  ? t->accent :
                        (d->accel_g < -0.05f) ? t->warn : t->text_primary;
        snprintf(gbuf, sizeof(gbuf), "%+.2fG", d->accel_g);
        font_draw_str(192, 108, gbuf, gcol, 2);
    }

    draw_divider(0, 128, SCREEN_W);

    /* Peak RPM + relative power */
    gauge_draw_label(10, 134, "PEAK RPM", t->text_secondary, 1);
    gauge_draw_numeric(10, 146, perf->peak_rpm, "%.0f", "rpm", t->text_primary, 1);

    gauge_draw_label(200, 134, "POWER", t->text_secondary, 1);
    gauge_draw_bar(200, 146, 250, 10,
                   d->power_pct, 0.0f, 100.0f, t->accent, t->gauge_track);
    {
        char pbuf[8];
        snprintf(pbuf, sizeof(pbuf), "%.0f%%", d->power_pct);
        font_draw_str(458, 140, pbuf, t->accent, 1);
    }

    draw_divider(0, 164, SCREEN_W);

    /* Speed bar */
    gauge_draw_label(10, 170, "SPEED", t->text_secondary, 1);
    gauge_draw_bar(10, 182, 460, 16,
                   vs->speed_kmh, 0.0f, 200.0f, t->gauge_fill, t->gauge_track);
    gauge_draw_numeric(10, 202, vs->speed_kmh, "%.0f", "km/h", t->text_primary, 1);

    /* Throttle bar */
    gauge_draw_label(10, 220, "THROTTLE", t->text_secondary, 1);
    gauge_draw_bar(10, 230, 460, 10,
                   vs->throttle_pct, 0.0f, 100.0f, t->accent, t->gauge_track);
    gauge_draw_numeric(10, 244, vs->throttle_pct, "%.1f", "%", t->text_primary, 1);
}

/* ------------------------------------------------------------------ */
/* Engine internals dashboard                                          */
/* ------------------------------------------------------------------ */

void dashboard_render_engine(const VehicleState *vs) {
    const Theme *t = theme_current();

    gauge_draw_label(10, 6, "ENGINE INTERNALS", t->accent, 1);
    draw_divider(0, 18, SCREEN_W);

    gauge_draw_label(10, 22, "FUEL TRIMS  (< 5% normal,  > 10% fault)", t->text_secondary, 1);
    draw_fuel_trim_row(10, 34, "STFT", vs->stft_pct, vs->supported[PID_STFT], t);
    draw_fuel_trim_row(10, 58, "LTFT", vs->ltft_pct, vs->supported[PID_LTFT], t);

    draw_divider(0, 80, SCREEN_W);

    int mid = 240;
    gauge_draw_label(10, 86, "TIMING ADVANCE", t->text_secondary, 1);
    if (vs->supported[PID_TIMING_ADV] == 1) {
        char tbuf[12];
        snprintf(tbuf, sizeof(tbuf), "%+.1f", vs->timing_adv_deg);
        font_draw_str(10, 98, tbuf, t->text_primary, 3);
        font_draw_str(10, 126, "deg BTDC", t->text_secondary, 1);
    } else {
        font_draw_str(10, 98, vs->supported[PID_TIMING_ADV] == 2 ? "N/A" : "...",
                      RGBA(70,70,70,255), 2);
    }

    renderer_draw_rect(mid, 82, 1, 56, t->text_secondary);

    gauge_draw_label(mid + 10, 86, "RPM", t->text_secondary, 1);
    gauge_draw_numeric(mid + 10, 98, vs->rpm, "%.0f", "", t->text_primary, 2);
    gauge_draw_label(mid + 10, 120, "LOAD", t->text_secondary, 1);
    gauge_draw_numeric(mid + 64, 120, vs->engine_load_pct, "%.0f", "%", t->text_primary, 1);

    draw_divider(0, 140, SCREEN_W);

    gauge_draw_label(10, 146, "IAT", t->text_secondary, 1);
    gauge_draw_numeric(42, 146, vs->iat_c, "%.0f", "C", t->text_primary, 1);

    gauge_draw_label(100, 146, "COOLANT", t->text_secondary, 1);
    uint32_t ct_col = (vs->coolant_temp_c > 100.0f) ? t->danger :
                      (vs->coolant_temp_c > 90.0f)  ? t->warn : t->text_primary;
    gauge_draw_numeric(164, 146, vs->coolant_temp_c, "%.0f", "C", ct_col, 1);

    gauge_draw_label(220, 146, "OIL", t->text_secondary, 1);
    draw_fa(248, 146, vs->oil_temp_c, "%.0f", "C",
            vs->supported[PID_OIL_TEMP], t->text_primary, 1, t);

    gauge_draw_label(310, 146, "VOLTAGE", t->text_secondary, 1);
    uint32_t v_col = (vs->voltage_v < 11.5f) ? t->danger :
                     (vs->voltage_v < 12.0f) ? t->warn : t->text_primary;
    gauge_draw_numeric(376, 146, vs->voltage_v, "%.2f", "V", v_col, 1);

    draw_divider(0, 162, SCREEN_W);

    gauge_draw_label(10, 168, "MAP", t->text_secondary, 1);
    draw_fa(42, 168, vs->map_kpa, "%.0f", "kPa",
            vs->supported[PID_MAP], t->text_primary, 1, t);

    gauge_draw_label(130, 168, "MAF", t->text_secondary, 1);
    draw_fa(160, 168, vs->maf_gs, "%.1f", "g/s",
            vs->supported[PID_MAF], t->text_primary, 1, t);

    gauge_draw_label(260, 168, "BARO", t->text_secondary, 1);
    draw_fa(296, 168, vs->baro_kpa, "%.0f", "kPa",
            vs->supported[PID_BARO], t->text_primary, 1, t);

    draw_divider(0, 184, SCREEN_W);

    gauge_draw_label(10, 190, "FUEL", t->text_secondary, 1);
    uint32_t fl_col = (vs->fuel_level_pct < 15.0f) ? t->danger :
                      (vs->fuel_level_pct < 25.0f) ? t->warn : t->gauge_fill;
    gauge_draw_bar(52, 190, 280, 10,
                   vs->fuel_level_pct, 0.0f, 100.0f, fl_col, t->gauge_track);
    char fbuf[8];
    snprintf(fbuf, sizeof(fbuf), "%.0f%%", vs->fuel_level_pct);
    font_draw_str(340, 190, fbuf, fl_col, 1);

    gauge_draw_label(10, 206, "AMBIENT", t->text_secondary, 1);
    gauge_draw_numeric(74, 206, vs->ambient_temp_c, "%.1f", "C", t->text_primary, 1);

    gauge_draw_label(180, 206, "RUNTIME", t->text_secondary, 1);
    char rtbuf[20];
    fmt_runtime(vs->runtime_s, rtbuf, sizeof(rtbuf));
    font_draw_str(242, 206, rtbuf, t->text_primary, 1);
}

/* ------------------------------------------------------------------ */
/* Trip & status dashboard                                             */
/* ------------------------------------------------------------------ */

static int g_trip_reset = 0;
void dashboard_trip_reset_session(void) { g_trip_reset = 1; }

void dashboard_render_trip(const VehicleState *vs, const DerivedState *d) {
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

    /* Engine runtime */
    gauge_draw_label(10, 24, "ENGINE RUNTIME", t->text_secondary, 1);
    char rtbuf[24];
    fmt_runtime(vs->runtime_s, rtbuf, sizeof(rtbuf));
    font_draw_str(10, 36, rtbuf, t->text_primary, 2);

    draw_divider(0, 68, SCREEN_W);

    /* Fuel level */
    gauge_draw_label(10, 74, "FUEL LEVEL", t->text_secondary, 1);
    uint32_t fl_col = (vs->fuel_level_pct < 15.0f) ? t->danger :
                      (vs->fuel_level_pct < 25.0f) ? t->warn : t->gauge_fill;
    char fbuf[8];
    snprintf(fbuf, sizeof(fbuf), "%.0f%%", vs->fuel_level_pct);
    font_draw_str(10, 86, fbuf, fl_col, 2);
    gauge_draw_bar(10, 110, 240, 12,
                   vs->fuel_level_pct, 0.0f, 100.0f, fl_col, t->gauge_track);

    /* Fuel rate + derived fuel economy */
    gauge_draw_label(270, 74, "FUEL RATE", t->text_secondary, 1);
    draw_fa(270, 86, vs->fuel_rate_lh, "%.1f", "L/h",
            vs->supported[PID_FUEL_RATE], t->text_primary, 2, t);

    gauge_draw_label(380, 74, "AVG", t->text_secondary, 1);
    if (d->trip_l100km > 0.0f) {
        char abuf[12];
        snprintf(abuf, sizeof(abuf), "%.1f", d->trip_l100km);
        font_draw_str(380, 86, abuf, t->text_primary, 2);
        font_draw_str(380, 104, "L/100km", t->text_secondary, 1);
    } else {
        font_draw_str(380, 86, "---", t->text_secondary, 2);
    }

    draw_divider(0, 128, SCREEN_W);

    /* Temperature block */
    gauge_draw_label(10, 134, "AMBIENT", t->text_secondary, 1);
    gauge_draw_numeric(74, 134, vs->ambient_temp_c, "%.1f", "C", t->text_primary, 1);

    gauge_draw_label(160, 134, "IAT", t->text_secondary, 1);
    gauge_draw_numeric(192, 134, vs->iat_c, "%.0f", "C", t->text_primary, 1);

    gauge_draw_label(260, 134, "COOLANT", t->text_secondary, 1);
    uint32_t ct_col = (vs->coolant_temp_c > 100.0f) ? t->danger :
                      (vs->coolant_temp_c > 90.0f)  ? t->warn : t->text_primary;
    gauge_draw_numeric(324, 134, vs->coolant_temp_c, "%.0f", "C", ct_col, 1);

    gauge_draw_label(10, 150, "OIL", t->text_secondary, 1);
    draw_fa(38, 150, vs->oil_temp_c, "%.0f", "C",
            vs->supported[PID_OIL_TEMP], t->text_primary, 1, t);

    gauge_draw_label(110, 150, "VOLTAGE", t->text_secondary, 1);
    uint32_t v_col = (vs->voltage_v < 11.5f) ? t->danger :
                     (vs->voltage_v < 12.0f) ? t->warn : t->text_primary;
    gauge_draw_numeric(176, 150, vs->voltage_v, "%.2f", "V", v_col, 1);

    gauge_draw_label(260, 150, "ETHANOL", t->text_secondary, 1);
    draw_fa(324, 150, vs->ethanol_pct, "%.1f", "%",
            vs->supported[PID_ETHANOL], t->text_primary, 1, t);

    draw_divider(0, 164, SCREEN_W);

    /* Trip derived stats */
    gauge_draw_label(10, 170, "TRIP DIST", t->text_secondary, 1);
    {
        char buf[12];
        snprintf(buf, sizeof(buf), "%.1f km", d->trip_dist_km);
        font_draw_str(10, 182, buf, t->text_primary, 1);
    }

    gauge_draw_label(140, 170, "FUEL USED", t->text_secondary, 1);
    {
        char buf[12];
        if (d->trip_fuel_l > 0.0f) {
            snprintf(buf, sizeof(buf), "%.2f L", d->trip_fuel_l);
            font_draw_str(140, 182, buf, t->text_primary, 1);
        } else {
            font_draw_str(140, 182, "--- L", t->text_secondary, 1);
        }
    }

    gauge_draw_label(280, 170, "RANGE EST.", t->text_secondary, 1);
    {
        char buf[12];
        if (d->range_km > 0.0f) {
            snprintf(buf, sizeof(buf), "%.0f km", d->range_km);
            uint32_t rc = (d->range_km < 50.0f) ? t->danger :
                          (d->range_km < 100.0f) ? t->warn : t->text_primary;
            font_draw_str(280, 182, buf, rc, 1);
        } else {
            font_draw_str(280, 182, "--- km", t->text_secondary, 1);
        }
    }

    draw_divider(0, 196, SCREEN_W);

    /* Session peaks */
    gauge_draw_label(10, 202, "SESSION PEAK", t->accent, 1);
    gauge_draw_label(10, 214, "SPEED", t->text_secondary, 1);
    gauge_draw_numeric(54, 214, sess_max_speed, "%.0f", "km/h", t->text_primary, 1);
    gauge_draw_label(160, 214, "RPM", t->text_secondary, 1);
    gauge_draw_numeric(196, 214, sess_max_rpm, "%.0f", "", t->text_primary, 1);
    font_draw_str(320, 214, "Triangle: reset", t->text_secondary, 1);
}

/* ------------------------------------------------------------------ */
/* Sensors dashboard                                                   */
/* ------------------------------------------------------------------ */

void dashboard_render_sensors(const VehicleState *vs, const DerivedState *d) {
    const Theme *t = theme_current();

    gauge_draw_label(10, 6, "ENGINE SENSORS", t->accent, 1);
    draw_divider(0, 18, SCREEN_W);

    int col2 = 240;

    gauge_draw_label(10, 24, "MAP", t->text_secondary, 1);
    draw_fa(46, 24, vs->map_kpa, "%.0f", "kPa",
            vs->supported[PID_MAP], t->text_primary, 1, t);
    if (vs->supported[PID_MAP] == 1) {
        gauge_draw_bar(46, 34, 160, 8,
                       vs->map_kpa, 20.0f, 105.0f, t->gauge_fill, t->gauge_track);
    }

    gauge_draw_label(col2, 24, "MAF", t->text_secondary, 1);
    draw_fa(col2 + 36, 24, vs->maf_gs, "%.1f", "g/s",
            vs->supported[PID_MAF], t->text_primary, 1, t);

    gauge_draw_label(10, 48, "BARO", t->text_secondary, 1);
    draw_fa(46, 48, vs->baro_kpa, "%.0f", "kPa",
            vs->supported[PID_BARO], t->text_primary, 1, t);

    gauge_draw_label(col2, 48, "FUEL RATE", t->text_secondary, 1);
    draw_fa(col2 + 82, 48, vs->fuel_rate_lh, "%.2f", "L/h",
            vs->supported[PID_FUEL_RATE], t->text_primary, 1, t);

    draw_divider(0, 64, SCREEN_W);

    /* O2 upstream - with AFR interpretation */
    gauge_draw_label(10, 70, "O2 UPSTREAM  (B1S1)", t->text_secondary, 1);
    if (vs->supported[PID_O2_B1S1] == 1) {
        gauge_draw_numeric(200, 70, vs->o2_b1s1_v, "%.3f", "V", t->text_primary, 1);
        gauge_draw_bar(10, 82, 460, 12,
                       vs->o2_b1s1_v, 0.0f, 1.275f, t->gauge_fill, t->gauge_track);

        /* AFR state label on same line as interpretation note */
        if (d->afr_state >= 0) {
            static const char *afl[] = { "LEAN", "STOICH", "RICH" };
            font_draw_str(10, 98, "AFR:", t->text_secondary, 1);
            uint32_t afc = (d->afr_state == 0) ? t->danger :
                           (d->afr_state == 1) ? COLOR_GREEN : t->warn;
            font_draw_str(42, 98, afl[d->afr_state], afc, 1);
            font_draw_str(100, 98, "   cycling 0.1-0.9V = closed loop OK",
                          t->text_secondary, 1);
        } else {
            font_draw_str(10, 98, "0.1-0.9V switching = closed loop OK",
                          t->text_secondary, 1);
        }
    } else {
        font_draw_str(200, 70,
                      vs->supported[PID_O2_B1S1] == 2 ? "N/A" : "...",
                      RGBA(70,70,70,255), 1);
    }

    gauge_draw_label(10, 112, "O2 DOWNSTREAM (B1S2)", t->text_secondary, 1);
    if (vs->supported[PID_O2_B1S2] == 1) {
        gauge_draw_numeric(208, 112, vs->o2_b1s2_v, "%.3f", "V", t->text_primary, 1);
        uint32_t o2_col = (vs->o2_b1s2_v > 0.5f && vs->o2_b1s2_v < 0.8f)
                          ? COLOR_GREEN : t->warn;
        gauge_draw_bar(10, 124, 460, 12,
                       vs->o2_b1s2_v, 0.0f, 1.275f, o2_col, t->gauge_track);
        font_draw_str(10, 140, "0.5-0.8V steady = catalyst working",
                      t->text_secondary, 1);
    } else {
        font_draw_str(208, 112,
                      vs->supported[PID_O2_B1S2] == 2 ? "N/A" : "...",
                      RGBA(70,70,70,255), 1);
    }

    draw_divider(0, 156, SCREEN_W);

    gauge_draw_label(10, 162, "OIL TEMP", t->text_secondary, 1);
    draw_fa(82, 162, vs->oil_temp_c, "%.0f", "C",
            vs->supported[PID_OIL_TEMP], t->text_primary, 1, t);

    gauge_draw_label(col2, 162, "ETHANOL", t->text_secondary, 1);
    draw_fa(col2 + 70, 162, vs->ethanol_pct, "%.1f", "%",
            vs->supported[PID_ETHANOL], t->text_primary, 1, t);

    gauge_draw_label(10, 178, "ACCEL POS", t->text_secondary, 1);
    draw_fa(82, 178, vs->accel_pos_pct, "%.1f", "%",
            vs->supported[PID_ACCEL_POS], t->text_primary, 1, t);

    gauge_draw_label(col2, 178, "REL THROT", t->text_secondary, 1);
    draw_fa(col2 + 82, 178, vs->rel_throttle_pct, "%.1f", "%",
            vs->supported[PID_REL_THROTTLE], t->text_primary, 1, t);

    draw_divider(0, 194, SCREEN_W);

    gauge_draw_label(10, 200, "MIL ON", t->text_secondary, 1);
    {
        char buf[20];
        if (vs->supported[PID_MIL_TIME] == 1) {
            fmt_minutes(vs->mil_time_min, buf, sizeof(buf));
            uint32_t mc = (vs->mil_time_min > 0) ? t->warn : t->text_primary;
            font_draw_str(64, 200, buf, mc, 1);
        } else {
            font_draw_str(64, 200,
                          vs->supported[PID_MIL_TIME] == 2 ? "N/A" : "...",
                          RGBA(70,70,70,255), 1);
        }
    }

    gauge_draw_label(col2, 200, "SINCE CLR", t->text_secondary, 1);
    {
        char buf[20];
        if (vs->supported[PID_CLR_TIME] == 1) {
            fmt_minutes(vs->clr_time_min, buf, sizeof(buf));
            font_draw_str(col2 + 82, 200, buf, t->text_primary, 1);
        } else {
            font_draw_str(col2 + 82, 200,
                          vs->supported[PID_CLR_TIME] == 2 ? "N/A" : "...",
                          RGBA(70,70,70,255), 1);
        }
    }

    gauge_draw_label(10, 216, "DIST MIL", t->text_secondary, 1);
    {
        char buf[16];
        if (vs->supported[PID_MIL_DIST] == 1) {
            snprintf(buf, sizeof(buf), "%.0f km", vs->mil_dist_km);
            uint32_t mc = (vs->mil_dist_km > 0) ? t->warn : t->text_primary;
            font_draw_str(74, 216, buf, mc, 1);
        } else {
            font_draw_str(74, 216,
                          vs->supported[PID_MIL_DIST] == 2 ? "N/A" : "...",
                          RGBA(70,70,70,255), 1);
        }
    }

    gauge_draw_label(col2, 216, "DIST CLR", t->text_secondary, 1);
    {
        char buf[16];
        if (vs->supported[PID_CLR_DIST] == 1) {
            snprintf(buf, sizeof(buf), "%.0f km", vs->clr_dist_km);
            font_draw_str(col2 + 74, 216, buf, t->text_primary, 1);
        } else {
            font_draw_str(col2 + 74, 216,
                          vs->supported[PID_CLR_DIST] == 2 ? "N/A" : "...",
                          RGBA(70,70,70,255), 1);
        }
    }

    draw_divider(0, 232, SCREEN_W);
    font_draw_str(10, 237, "N/A = PID not supported by this ECU",
                  RGBA(70, 70, 70, 255), 1);
}

/* ------------------------------------------------------------------ */
/* Economy dashboard                                                   */
/* ------------------------------------------------------------------ */

void dashboard_render_economy(const VehicleState *vs, const DerivedState *d) {
    const Theme *t = theme_current();

    gauge_draw_label(10, 6, "ECONOMY & EFFICIENCY", t->accent, 1);
    draw_divider(0, 18, SCREEN_W);

    /* --- Top: Gear (left) + Instant consumption (right) --- */
    gauge_draw_label(10, 26, "GEAR", t->text_secondary, 1);
    draw_gear(10, 38, d->gear, 5, t);   /* scale 5 = 40px digit */

    gauge_draw_label(250, 26, "INSTANT", t->text_secondary, 1);
    if (vs->supported[PID_FUEL_RATE] != 1) {
        font_draw_str(250, 38, "N/A", RGBA(70,70,70,255), 2);
        font_draw_str(250, 58, "(needs PID 015E)", RGBA(70,70,70,255), 1);
    } else if (d->coasting) {
        font_draw_str(250, 38, "COASTING", COLOR_GREEN, 2);
        font_draw_str(250, 58, "fuel cut", t->text_secondary, 1);
    } else if (d->idle) {
        gauge_draw_numeric(250, 38, vs->fuel_rate_lh, "%.2f", "L/h",
                           t->text_primary, 2);
        font_draw_str(250, 58, "at idle", t->text_secondary, 1);
    } else if (d->instant_l100km > 0.0f) {
        gauge_draw_numeric(250, 38, d->instant_l100km, "%.1f", "L/100km",
                           t->text_primary, 2);
    } else {
        font_draw_str(250, 38, "---", t->text_secondary, 2);
    }

    draw_divider(0, 88, SCREEN_W);

    /* --- Middle: Trip stats --- */
    gauge_draw_label(10,  94, "TRIP AVG", t->text_secondary, 1);
    gauge_draw_label(180, 94, "FUEL USED", t->text_secondary, 1);
    gauge_draw_label(350, 94, "RANGE", t->text_secondary, 1);

    /* Trip avg L/100km */
    if (d->trip_l100km > 0.0f) {
        char buf[12];
        snprintf(buf, sizeof(buf), "%.1f", d->trip_l100km);
        font_draw_str(10, 106, buf, t->text_primary, 2);
        font_draw_str(10, 124, "L/100km", t->text_secondary, 1);
    } else {
        font_draw_str(10, 106, "---", t->text_secondary, 2);
    }

    /* Trip fuel used */
    if (d->trip_fuel_l > 0.0f) {
        char buf[12];
        snprintf(buf, sizeof(buf), "%.2f L", d->trip_fuel_l);
        font_draw_str(180, 106, buf, t->text_primary, 1);
    } else {
        font_draw_str(180, 106, "---", t->text_secondary, 1);
    }

    /* Estimated range */
    if (d->range_km > 0.0f) {
        uint32_t rc = (d->range_km < 50.0f)  ? t->danger :
                      (d->range_km < 100.0f) ? t->warn : t->text_primary;
        char buf[12];
        snprintf(buf, sizeof(buf), "%.0f km", d->range_km);
        font_draw_str(350, 106, buf, rc, 1);
    } else {
        font_draw_str(350, 106, "---", t->text_secondary, 1);
    }

    draw_divider(0, 136, SCREEN_W);

    /* --- AFR status + relative power --- */
    gauge_draw_label(10, 142, "AFR STATUS", t->text_secondary, 1);
    if (d->afr_state < 0) {
        font_draw_str(10, 154, "unknown", RGBA(70,70,70,255), 1);
        font_draw_str(10, 164, "(needs O2 sensor PID 0114)", RGBA(70,70,70,255), 1);
    } else {
        static const char *afl[] = { "LEAN", "STOICH", "RICH" };
        static const char *afd[] = {
            "too little fuel - check trims",
            "ideal air-fuel ratio",
            "too much fuel - check trims"
        };
        uint32_t afc = (d->afr_state == 0) ? t->danger :
                       (d->afr_state == 1) ? COLOR_GREEN : t->warn;
        font_draw_str(10, 154, afl[d->afr_state], afc, 2);
        font_draw_str(10, 172, afd[d->afr_state], t->text_secondary, 1);
    }

    gauge_draw_label(270, 142, "RELATIVE POWER", t->text_secondary, 1);
    gauge_draw_bar(270, 154, 190, 14,
                   d->power_pct, 0.0f, 100.0f, t->accent, t->gauge_track);
    {
        char pbuf[8];
        snprintf(pbuf, sizeof(pbuf), "%.0f%%", d->power_pct);
        font_draw_str(270, 172, pbuf, t->accent, 1);
    }

    draw_divider(0, 186, SCREEN_W);

    /* --- G-force bar --- */
    gauge_draw_label(10, 192, "G-FORCE (LONGITUDINAL)", t->text_secondary, 1);
    draw_g_bar(10, 204, 460, d->accel_g, t);
    font_draw_str(10,  220, "-2G", RGBA(70,70,70,255), 1);
    font_draw_str(230, 220, "0G", RGBA(70,70,70,255), 1);
    font_draw_str(452, 220, "+2G", RGBA(70,70,70,255), 1);
    {
        char gbuf[12];
        uint32_t gcol = (d->accel_g > 0.05f)  ? t->accent :
                        (d->accel_g < -0.05f) ? t->warn : t->text_primary;
        snprintf(gbuf, sizeof(gbuf), "%+.2fG", d->accel_g);
        font_draw_str(192, 220, gbuf, gcol, 1);
    }

    draw_divider(0, 234, SCREEN_W);

    /* Context strip */
    gauge_draw_label(10, 240, "SPEED", t->text_secondary, 1);
    gauge_draw_numeric(54, 240, vs->speed_kmh, "%.0f", "km/h", t->text_primary, 1);
    gauge_draw_label(160, 240, "RPM", t->text_secondary, 1);
    gauge_draw_numeric(196, 240, vs->rpm, "%.0f", "", t->text_primary, 1);
    gauge_draw_label(290, 240, "LOAD", t->text_secondary, 1);
    gauge_draw_numeric(326, 240, vs->engine_load_pct, "%.0f", "%", t->text_primary, 1);
    if (d->coasting)
        font_draw_str(410, 240, "COAST", COLOR_GREEN, 1);
    else if (d->idle)
        font_draw_str(410, 240, "IDLE", t->text_secondary, 1);
}

/* ------------------------------------------------------------------ */
/* JDM / NFS dashboard                                                 */
/* ------------------------------------------------------------------ */

void dashboard_render_jdm(const VehicleState *vs, const DerivedState *d) {
    const Theme *t = theme_current();
    int fs = t->font_scale;

    /* === Top RPM power band strip (y=2, h=8, full width) === */
    renderer_draw_rect(0, 2, SCREEN_W, 8, t->gauge_track);
    /* Three colored zone backgrounds */
    renderer_draw_rect(0,   2, 240, 8, RGBA(0, 35, 0,   255));  /* 0-4000: dark green */
    renderer_draw_rect(240, 2, 150, 8, RGBA(35, 25, 0,  255));  /* 4000-6500: dark amber */
    renderer_draw_rect(390, 2, 90,  8, RGBA(40, 0,  0,  255));  /* 6500-8000: dark red */

    float rpm_pct = vs->rpm / 8000.0f;
    if (rpm_pct > 1.0f) rpm_pct = 1.0f;
    int fill_w = (int)(rpm_pct * SCREEN_W);
    if (fill_w > 0) {
        int z1 = 240, z2 = 150, z12 = z1 + z2;
        if (fill_w <= z1) {
            renderer_draw_rect(0, 2, fill_w, 8, t->gauge_fill);
        } else if (fill_w <= z12) {
            renderer_draw_rect(0,  2, z1,           8, t->gauge_fill);
            renderer_draw_rect(z1, 2, fill_w - z1,  8, t->warn);
        } else {
            renderer_draw_rect(0,   2, z1,            8, t->gauge_fill);
            renderer_draw_rect(z1,  2, z2,            8, t->warn);
            renderer_draw_rect(z12, 2, fill_w - z12,  8, t->redline);
        }
    }

    /* === Tachometer: cx=118, cy=127, r=100 === */
    static GaugeDef tach_jdm = {
        .cx=118, .cy=127, .radius=100,
        .min_val=0.0f, .max_val=8000.0f, .redline=6500.0f,
        .start_angle=PI_F * 0.75f, .sweep_angle=PI_F * 1.5f, .anim_val=0.0f,
    };
    gauge_draw_analog_jdm(&tach_jdm, vs->rpm, 1000.0f, 500.0f, 1000.0f);

    /* RPM value centered in lower tach */
    {
        char buf[8];
        int rscale = fs + 1;
        snprintf(buf, sizeof(buf), "%.0f", vs->rpm);
        int tw = font_str_width(buf, rscale);
        uint32_t rc = (vs->rpm >= 6500.0f) ? t->redline : t->text_primary;
        font_draw_str(tach_jdm.cx - tw / 2, 170, buf, rc, rscale);
        font_draw_str(tach_jdm.cx - 8, 170 + rscale * 8 + 2, "rpm", t->text_secondary, 1);
    }

    /* === Speedometer: cx=362, cy=127, r=100 === */
    static GaugeDef spd_jdm = {
        .cx=362, .cy=127, .radius=100,
        .min_val=0.0f, .max_val=240.0f, .redline=999.0f,
        .start_angle=PI_F * 0.75f, .sweep_angle=PI_F * 1.5f, .anim_val=0.0f,
    };
    gauge_draw_analog_jdm(&spd_jdm, vs->speed_kmh, 20.0f, 10.0f, 0.0f);

    /* Speed value centered in lower speedo */
    {
        char buf[8];
        int sscale = fs + 2;
        snprintf(buf, sizeof(buf), "%.0f", vs->speed_kmh);
        int sw = font_str_width(buf, sscale);
        font_draw_str(spd_jdm.cx - sw / 2, 162, buf, t->text_primary, sscale);
        font_draw_str(spd_jdm.cx - 16, 162 + sscale * 8 + 4, "km/h", t->text_secondary, 1);
    }

    /* === Center column: gear indicator === */
    gauge_draw_label(224, 100, "GEAR", t->text_secondary, 1);
    draw_gear(228, 112, d->gear, 3, t);

    /* Coasting flag in center if active */
    if (d->coasting)
        font_draw_str(218, 148, "COAST", COLOR_GREEN, 1);
    else if (d->idle)
        font_draw_str(224, 148, "IDLE", t->text_secondary, 1);

    /* === Bottom data strip === */
    renderer_draw_rect(0, 233, SCREEN_W, 1, t->text_secondary);

    /* Throttle */
    font_draw_str(6, 238, "THR", t->text_secondary, 1);
    uint32_t thr_col = (vs->throttle_pct > 80.0f) ? t->redline :
                       (vs->throttle_pct > 50.0f) ? t->warn : t->gauge_fill;
    gauge_draw_bar(32, 240, 120, 7, vs->throttle_pct, 0.0f, 100.0f, thr_col, t->gauge_track);
    {
        char buf[8];
        snprintf(buf, sizeof(buf), "%.0f%%", vs->throttle_pct);
        font_draw_str(158, 238, buf, thr_col, fs);
    }

    /* Coolant temp */
    uint32_t ct_col = (vs->coolant_temp_c > 100.0f) ? t->danger :
                      (vs->coolant_temp_c > 90.0f)  ? t->warn : t->text_secondary;
    font_draw_str(200, 238, "COOL", t->text_secondary, 1);
    {
        char buf[8];
        snprintf(buf, sizeof(buf), "%.0fC", vs->coolant_temp_c);
        font_draw_str(234, 238, buf, ct_col, fs);
    }

    /* Voltage */
    uint32_t v_col = (vs->voltage_v < 11.5f) ? t->danger :
                     (vs->voltage_v < 12.5f) ? t->warn : t->text_secondary;
    font_draw_str(292, 238, "VOLT", t->text_secondary, 1);
    {
        char buf[10];
        snprintf(buf, sizeof(buf), "%.1fV", vs->voltage_v);
        font_draw_str(326, 238, buf, v_col, fs);
    }

    /* G-force */
    font_draw_str(378, 238, "G", t->text_secondary, 1);
    {
        char buf[10];
        uint32_t gc = (d->accel_g > 0.1f)  ? t->accent :
                      (d->accel_g < -0.1f) ? t->warn : t->text_primary;
        snprintf(buf, sizeof(buf), "%+.1fG", d->accel_g);
        font_draw_str(390, 238, buf, gc, fs);
    }
}

/* ------------------------------------------------------------------ */
/* Touge -- driving-focused, peripheral-vision layout                  */
/* Star: huge gear + RPM + speed. AFR, G-force, shift lights.         */
/* ------------------------------------------------------------------ */

void dashboard_render_touge(const VehicleState *vs, const DerivedState *d) {
    const Theme *t  = theme_current();
    uint32_t    now = time_now_ms();

    /* === RPM power band strip (y=0, h=10) === */
    renderer_draw_rect(0,   0, 480, 10, t->gauge_track);
    renderer_draw_rect(0,   0, 270, 10, RGBA(0,  35,  0, 255));
    renderer_draw_rect(270, 0,  78, 10, RGBA(35, 25,  0, 255));
    renderer_draw_rect(348, 0, 132, 10, RGBA(40,  0,  0, 255));
    {
        float pct = vs->rpm / 8000.0f;
        if (pct > 1.0f) pct = 1.0f;
        int fw = (int)(pct * 480);
        if (fw > 0) {
            if (fw <= 270) {
                renderer_draw_rect(0, 0, fw, 10, t->gauge_fill);
            } else if (fw <= 348) {
                renderer_draw_rect(0,   0, 270,      10, t->gauge_fill);
                renderer_draw_rect(270, 0, fw - 270, 10, t->warn);
            } else {
                renderer_draw_rect(0,   0, 270,      10, t->gauge_fill);
                renderer_draw_rect(270, 0,  78,      10, t->warn);
                renderer_draw_rect(348, 0, fw - 348, 10, t->redline);
            }
        }
    }

    /* === Main row: Gear | RPM | Speed (y=12-66) === */
    gauge_draw_label(12,  12, "GEAR",  t->text_secondary, 1);
    gauge_draw_label(140, 12, "RPM",   t->text_secondary, 1);
    gauge_draw_label(315, 12, "SPEED", t->text_secondary, 1);

    draw_gear(12, 22, d->gear, 5, t);  /* scale 5 = 40px tall, ends y=62 */

    {
        char buf[8];
        snprintf(buf, sizeof(buf), "%.0f", vs->rpm);
        uint32_t rc = (vs->rpm >= 6500.0f)        ? t->redline  :
                      (vs->rpm >= VTEC_RPM_THRESHOLD) ? COLOR_GREEN :
                      t->text_primary;
        font_draw_str(140, 22, buf, rc, 2);
    }
    gauge_draw_bar(140, 44, 150, 7, vs->rpm, 0.0f, 8000.0f, t->gauge_fill, t->gauge_track);
    font_draw_str(140, 53, "rpm", t->text_secondary, 1);

    {
        char buf[8];
        snprintf(buf, sizeof(buf), "%.0f", vs->speed_kmh);
        font_draw_str(315, 20, buf, t->text_primary, 3);
    }
    font_draw_str(315, 46, "km/h", t->text_secondary, 1);

    /* VTEC indicator -- appears top-right only when engaged, flashes */
    if (vs->rpm >= VTEC_RPM_THRESHOLD) {
        uint32_t vc = ((now / 200) % 2) ? COLOR_GREEN : RGBA(0, 140, 0, 255);
        font_draw_str(408, 12, "VTEC", vc, 1);
        font_draw_str(412, 22, "ON",   vc, 1);
    }

    draw_divider(0, 70, SCREEN_W);

    /* === AFR + G-force + coast row (y=74) === */
    gauge_draw_label(8, 74, "AFR", t->text_secondary, 1);
    if (d->afr_state < 0) {
        font_draw_str(38, 74, "N/A", RGBA(70, 70, 70, 255), 1);
    } else {
        static const char *afl[] = { "LEAN", "STOICH", "RICH" };
        uint32_t afc = (d->afr_state == 0) ? t->danger :
                       (d->afr_state == 1) ? COLOR_GREEN : t->warn;
        font_draw_str(38, 74, afl[d->afr_state], afc, 1);
        if (vs->supported[PID_O2_B1S1] == 1) {
            char buf[10];
            snprintf(buf, sizeof(buf), "%.2fV", vs->o2_b1s1_v);
            font_draw_str(100, 74, buf, t->text_secondary, 1);
        }
    }

    {
        char gbuf[12];
        uint32_t gc = (d->accel_g >  0.1f) ? t->accent :
                      (d->accel_g < -0.1f) ? t->warn : t->text_secondary;
        snprintf(gbuf, sizeof(gbuf), "%+.2fG", d->accel_g);
        font_draw_str(230, 74, gbuf, gc, 1);
    }

    if (d->coasting)
        font_draw_str(340, 74, "COASTING", COLOR_GREEN, 1);
    else if (d->idle)
        font_draw_str(374, 74, "IDLE", t->text_secondary, 1);

    draw_divider(0, 92, SCREEN_W);

    /* === Data strip: Timing | Load | IAT | Throttle (y=96-130) === */
    gauge_draw_label(8,   96, "TIMING",   t->text_secondary, 1);
    draw_fa(8, 106, vs->timing_adv_deg, "%+.1f", "deg",
            vs->supported[PID_TIMING_ADV], t->text_primary, 1, t);

    gauge_draw_label(120, 96, "LOAD", t->text_secondary, 1);
    gauge_draw_numeric(120, 106, vs->engine_load_pct, "%.0f", "%", t->text_primary, 1);
    gauge_draw_bar(120, 118, 90, 6, vs->engine_load_pct, 0.0f, 100.0f, t->gauge_fill, t->gauge_track);

    gauge_draw_label(228, 96, "IAT", t->text_secondary, 1);
    gauge_draw_numeric(228, 106, vs->iat_c, "%.0f", "C", t->text_primary, 1);

    gauge_draw_label(318, 96, "THROTTLE", t->text_secondary, 1);
    {
        uint32_t tc = (vs->throttle_pct > 80.0f) ? t->redline :
                      (vs->throttle_pct > 50.0f) ? t->warn : t->gauge_fill;
        gauge_draw_bar(318, 106, 154, 8, vs->throttle_pct, 0.0f, 100.0f, tc, t->gauge_track);
        char buf[8];
        snprintf(buf, sizeof(buf), "%.0f%%", vs->throttle_pct);
        font_draw_str(318, 118, buf, tc, 1);
    }

    draw_divider(0, 132, SCREEN_W);

    /* === G-force bar (y=136-148) === */
    gauge_draw_label(8, 136, "G-FORCE", t->text_secondary, 1);
    draw_g_bar(8, 148, 464, d->accel_g, t);
    font_draw_str(8,   160, "-2G", RGBA(70, 70, 70, 255), 1);
    font_draw_str(232, 160, "0G",  RGBA(70, 70, 70, 255), 1);
    font_draw_str(452, 160, "+2G", RGBA(70, 70, 70, 255), 1);

    draw_divider(0, 174, SCREEN_W);

    /* === Bottom info: fuel economy + coolant (y=178) === */
    gauge_draw_label(8, 178, "INSTANT", t->text_secondary, 1);
    if (d->instant_l100km > 0.0f) {
        gauge_draw_numeric(62, 178, d->instant_l100km, "%.1f", "L/100", t->accent, 1);
    } else {
        font_draw_str(62, 178, "--.-", t->text_secondary, 1);
    }

    gauge_draw_label(190, 178, "TRIP AVG", t->text_secondary, 1);
    if (d->trip_l100km > 0.5f) {
        gauge_draw_numeric(254, 178, d->trip_l100km, "%.1f", "L/100", t->text_primary, 1);
    } else {
        font_draw_str(254, 178, "--.-", t->text_secondary, 1);
    }

    gauge_draw_label(370, 178, "COOL", t->text_secondary, 1);
    {
        uint32_t cc = (vs->coolant_temp_c > 100.0f) ? t->danger :
                      (vs->coolant_temp_c > 90.0f)  ? t->warn : t->text_primary;
        char buf[8];
        snprintf(buf, sizeof(buf), "%.0fC", vs->coolant_temp_c);
        font_draw_str(404, 178, buf, cc, 1);
    }

    /* === Shift lights: 8 blocks, fill 5500-6500 RPM (y=196-210) === */
    if (vs->rpm >= 5500.0f) {
        int lit = (int)((vs->rpm - 5500.0f) / 125.0f);
        if (lit > 8) lit = 8;
        int bw = 48, gap = 7;  /* 8*48 + 7*7 = 433px, start x=24 */
        for (int i = 0; i < 8; i++) {
            uint32_t bc = (i < lit)
                ? ((i < 4) ? t->warn : t->redline)
                : t->gauge_track;
            renderer_draw_rect(24 + i * (bw + gap), 196, bw, 14, bc);
        }
    }
}

/* ------------------------------------------------------------------ */
/* VTEC -- Honda engine performance screen                             */
/* RPM + VTEC engage badge + throttle/load bars + trims + G-force.    */
/* ------------------------------------------------------------------ */

void dashboard_render_vtec(const VehicleState *vs, const DerivedState *d) {
    const Theme *t   = theme_current();
    uint32_t    now  = time_now_ms();
    int         vtec = (vs->rpm >= VTEC_RPM_THRESHOLD);

    /* === RPM strip (y=0, h=12) with yellow VTEC threshold marker === */
    renderer_draw_rect(0,   0, 480, 12, t->gauge_track);
    renderer_draw_rect(0,   0, 270, 12, RGBA(0,  35,  0, 255));
    renderer_draw_rect(270, 0,  78, 12, RGBA(35, 25,  0, 255));
    renderer_draw_rect(348, 0, 132, 12, RGBA(40,  0,  0, 255));
    {
        float pct = vs->rpm / 8000.0f;
        if (pct > 1.0f) pct = 1.0f;
        int fw = (int)(pct * 480);
        if (fw > 0) {
            if (fw <= 270) {
                renderer_draw_rect(0, 0, fw, 12, t->gauge_fill);
            } else if (fw <= 348) {
                renderer_draw_rect(0,   0, 270,      12, t->gauge_fill);
                renderer_draw_rect(270, 0, fw - 270, 12, t->warn);
            } else {
                renderer_draw_rect(0,   0, 270,      12, t->gauge_fill);
                renderer_draw_rect(270, 0,  78,      12, t->warn);
                renderer_draw_rect(348, 0, fw - 348, 12, t->redline);
            }
        }
        /* VTEC threshold marker */
        renderer_draw_rect(348, 0, 2, 12, RGBA(255, 255, 0, 220));
    }

    /* === VTEC badge (center) + RPM (left) + Speed (right) (y=15-54) === */
    {
        int flash = !vtec || ((now / 180) % 2);
        const char *vtec_str = vtec ? "VTEC ON" : "VTEC OFF";
        uint32_t badge_bg  = vtec && flash ? RGBA(0, 50, 0, 255) : RGBA(18, 18, 18, 255);
        uint32_t badge_bdr = vtec && flash ? COLOR_GREEN : RGBA(45, 45, 45, 255);
        uint32_t vtec_col  = vtec && flash ? COLOR_GREEN :
                             vtec           ? RGBA(0, 160, 0, 255) :
                             RGBA(50, 50, 50, 255);

        /* badge box: x=145, y=15, w=190, h=36 */
        renderer_draw_rect(145, 15, 190, 36, badge_bg);
        renderer_draw_rect(145, 15, 190,  1, badge_bdr);
        renderer_draw_rect(145, 50,  190,  1, badge_bdr);
        renderer_draw_rect(145, 15,   1, 36, badge_bdr);
        renderer_draw_rect(334, 15,   1, 36, badge_bdr);

        int tw = font_str_width(vtec_str, 2);
        font_draw_str(240 - tw / 2, 24, vtec_str, vtec_col, 2);
    }

    /* RPM value left of badge */
    {
        char buf[8];
        snprintf(buf, sizeof(buf), "%.0f", vs->rpm);
        uint32_t rc = (vs->rpm >= 6500.0f) ? t->redline :
                      vtec                  ? COLOR_GREEN : t->text_primary;
        font_draw_str(8, 18, buf, rc, 3);
        font_draw_str(8, 44, "rpm", t->text_secondary, 1);
    }

    /* Speed right of badge */
    {
        char buf[8];
        snprintf(buf, sizeof(buf), "%.0f", vs->speed_kmh);
        font_draw_str(352, 20, buf, t->text_primary, 2);
        font_draw_str(352, 38, "km/h", t->text_secondary, 1);
    }

    draw_divider(0, 56, SCREEN_W);

    /* === Full-width RPM bar with VTEC marker (y=60-76) === */
    gauge_draw_label(8, 60, "RPM", t->text_secondary, 1);
    {
        uint32_t bc = (vs->rpm >= 6500.0f) ? t->redline :
                      vtec                  ? COLOR_GREEN : t->gauge_fill;
        gauge_draw_bar(8, 70, 464, 10, vs->rpm, 0.0f, 8000.0f, bc, t->gauge_track);
        int mx = 8 + (int)(VTEC_RPM_THRESHOLD / 8000.0f * 464);
        renderer_draw_rect(mx, 68, 2, 14, RGBA(255, 255, 0, 200));
    }

    /* === Throttle bar (y=86-100) === */
    gauge_draw_label(8, 86, "THROTTLE", t->text_secondary, 1);
    {
        uint32_t tc = (vs->throttle_pct > 80.0f) ? t->redline :
                      (vs->throttle_pct > 50.0f) ? t->warn : t->gauge_fill;
        gauge_draw_bar(8, 96, 432, 10, vs->throttle_pct, 0.0f, 100.0f, tc, t->gauge_track);
        char buf[8];
        snprintf(buf, sizeof(buf), "%.0f%%", vs->throttle_pct);
        font_draw_str(448, 96, buf, tc, 1);
    }

    /* === Engine load bar (y=110-124) === */
    gauge_draw_label(8, 110, "ENGINE LOAD", t->text_secondary, 1);
    {
        uint32_t lc = (vs->engine_load_pct > 80.0f) ? t->redline :
                      (vs->engine_load_pct > 60.0f) ? t->warn : t->gauge_fill;
        gauge_draw_bar(8, 120, 432, 10, vs->engine_load_pct, 0.0f, 100.0f, lc, t->gauge_track);
        char buf[8];
        snprintf(buf, sizeof(buf), "%.0f%%", vs->engine_load_pct);
        font_draw_str(448, 120, buf, lc, 1);
    }

    draw_divider(0, 134, SCREEN_W);

    /* === Compact trim + timing + AFR row (y=138) === */
    gauge_draw_label(8, 138, "STFT", t->text_secondary, 1);
    {
        uint32_t sc = (vs->stft_pct > 10.0f || vs->stft_pct < -10.0f) ? t->danger :
                      (vs->stft_pct >  5.0f || vs->stft_pct <  -5.0f) ? t->warn : t->gauge_fill;
        draw_fa(8, 148, vs->stft_pct, "%+.1f", "%", vs->supported[PID_STFT], sc, 1, t);
    }

    gauge_draw_label(100, 138, "LTFT", t->text_secondary, 1);
    {
        uint32_t lc = (vs->ltft_pct > 10.0f || vs->ltft_pct < -10.0f) ? t->danger :
                      (vs->ltft_pct >  5.0f || vs->ltft_pct <  -5.0f) ? t->warn : t->gauge_fill;
        draw_fa(100, 148, vs->ltft_pct, "%+.1f", "%", vs->supported[PID_LTFT], lc, 1, t);
    }

    gauge_draw_label(200, 138, "TIMING", t->text_secondary, 1);
    draw_fa(200, 148, vs->timing_adv_deg, "%+.1f", "deg",
            vs->supported[PID_TIMING_ADV], t->text_primary, 1, t);

    gauge_draw_label(320, 138, "AFR", t->text_secondary, 1);
    if (d->afr_state < 0) {
        font_draw_str(352, 138, "N/A", RGBA(70, 70, 70, 255), 1);
    } else {
        static const char *afl[] = { "LEAN", "STOICH", "RICH" };
        uint32_t afc = (d->afr_state == 0) ? t->danger :
                       (d->afr_state == 1) ? COLOR_GREEN : t->warn;
        font_draw_str(352, 138, afl[d->afr_state], afc, 1);
    }

    draw_divider(0, 162, SCREEN_W);

    /* === Context strip: Coolant | IAT | Gear | G-force (y=166) === */
    gauge_draw_label(8, 166, "COOL", t->text_secondary, 1);
    {
        uint32_t cc = (vs->coolant_temp_c > 100.0f) ? t->danger :
                      (vs->coolant_temp_c > 90.0f)  ? t->warn : t->text_primary;
        char buf[8];
        snprintf(buf, sizeof(buf), "%.0fC", vs->coolant_temp_c);
        font_draw_str(42, 166, buf, cc, 1);
    }

    gauge_draw_label(110, 166, "IAT", t->text_secondary, 1);
    gauge_draw_numeric(138, 166, vs->iat_c, "%.0f", "C", t->text_primary, 1);

    gauge_draw_label(200, 166, "GEAR", t->text_secondary, 1);
    draw_gear(234, 162, d->gear, 2, t);

    {
        char gbuf[12];
        uint32_t gc = (d->accel_g >  0.1f) ? t->accent :
                      (d->accel_g < -0.1f) ? t->warn : t->text_secondary;
        snprintf(gbuf, sizeof(gbuf), "%+.2fG", d->accel_g);
        gauge_draw_label(300, 166, "G", t->text_secondary, 1);
        font_draw_str(316, 166, gbuf, gc, 1);
    }

    if (d->coasting)
        font_draw_str(418, 166, "COAST", COLOR_GREEN, 1);
    else if (d->idle)
        font_draw_str(426, 166, "IDLE", t->text_secondary, 1);

    draw_divider(0, 180, SCREEN_W);

    /* === G-force bar (y=184-198) === */
    draw_g_bar(8, 184, 464, d->accel_g, t);
    font_draw_str(8,   198, "-2G", RGBA(70, 70, 70, 255), 1);
    font_draw_str(232, 198, "0G",  RGBA(70, 70, 70, 255), 1);
    font_draw_str(452, 198, "+2G", RGBA(70, 70, 70, 255), 1);
}

/* ------------------------------------------------------------------ */
/* Status bar (bottom)                                                 */
/* ------------------------------------------------------------------ */

void dashboard_render_status_bar(const VehicleState *vs, DashMode mode, int connected) {
    const Theme *t = theme_current();
    static const char *mode_names[] = {
        "DIGITAL", "ANALOG", "DIAG", "PERF", "ENGINE", "TRIP", "SENSORS", "ECONOMY", "JDM",
        "TOUGE", "VTEC"
    };

    renderer_draw_rect(0, 260, SCREEN_W, 12, RGBA(8, 8, 8, 255));
    font_draw_str(4, 261, mode_names[mode], t->accent, 1);

    uint32_t conn_col = (connected == 2) ? t->warn :
                        (connected == 1) ? COLOR_GREEN : t->danger;
    const char *conn_str = (connected == 2) ? "DEMO" :
                           (connected == 1) ? "OBD OK" : "NO OBD";
    font_draw_str(80, 261, conn_str, conn_col, 1);

    font_draw_str(200, 261, theme_current()->name, t->text_secondary, 1);
    font_draw_str(310, 261, "SEL:HELP  STA:MENU", t->text_secondary, 1);
    (void)vs;
}

/* ------------------------------------------------------------------ */
/* Top-level dispatcher                                                */
/* ------------------------------------------------------------------ */

void dashboard_init(void) {}

void dashboard_render(const VehicleState *vs, const DtcList *dtc,
                      const DerivedState *d, DashMode mode) {
    static PerfState perf = {0};
    switch (mode) {
        case DASH_MODE_DIGITAL:     dashboard_render_digital(vs, d);             break;
        case DASH_MODE_ANALOG:      dashboard_render_analog(vs, d);              break;
        case DASH_MODE_DIAGNOSTICS: dashboard_render_diagnostics(vs, dtc);       break;
        case DASH_MODE_PERFORMANCE: dashboard_render_performance(vs, &perf, d);  break;
        case DASH_MODE_ENGINE:      dashboard_render_engine(vs);                 break;
        case DASH_MODE_TRIP:        dashboard_render_trip(vs, d);                break;
        case DASH_MODE_SENSORS:     dashboard_render_sensors(vs, d);             break;
        case DASH_MODE_ECONOMY:     dashboard_render_economy(vs, d);             break;
        case DASH_MODE_JDM:         dashboard_render_jdm(vs, d);                 break;
        case DASH_MODE_TOUGE:       dashboard_render_touge(vs, d);               break;
        case DASH_MODE_VTEC:        dashboard_render_vtec(vs, d);                break;
        default: break;
    }
}
