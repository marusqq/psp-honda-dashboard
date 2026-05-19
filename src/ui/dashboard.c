#include <stdio.h>
#include <math.h>
#include "ui/dashboard.h"
#include "ui/renderer.h"
#include "ui/gauge.h"
#include "ui/themes.h"
#include "utils/font.h"
#include "utils/time.h"
#include "telemetry/units.h"
#include <psprtc.h>

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


/* ------------------------------------------------------------------ */
/* Diagnostics dashboard                                               */
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
/* Combined Economy + Trip dashboard                                   */
/* ------------------------------------------------------------------ */

static int g_trip_reset = 0;
void dashboard_trip_reset_session(void) { g_trip_reset = 1; }

void dashboard_render_ecotrip(const VehicleState *vs, const DerivedState *d) {
    const Theme *t = theme_current();

    static float s_peak_speed = 0.0f;
    static float s_peak_rpm   = 0.0f;
    if (g_trip_reset) {
        s_peak_speed = 0.0f;
        s_peak_rpm   = 0.0f;
        g_trip_reset = 0;
    }
    if (vs->speed_kmh > s_peak_speed) s_peak_speed = vs->speed_kmh;
    if (vs->rpm       > s_peak_rpm)   s_peak_rpm   = vs->rpm;

    gauge_draw_label(10, 6, "ECO & TRIP", t->accent, 1);
    draw_divider(0, 18, SCREEN_W);

    /* --- Gear + Instant consumption --- */
    gauge_draw_label(10, 24, "GEAR", t->text_secondary, 1);
    draw_gear(10, 36, d->gear, 5, t);

    gauge_draw_label(250, 24, "INSTANT", t->text_secondary, 1);
    if (d->instant_lh == DERIVED_NO_DATA && !d->coasting && !d->idle) {
        font_draw_str(250, 36, "---", t->text_secondary, 2);
        font_draw_str(250, 56, "no fuel data", RGBA(70,70,70,255), 1);
    } else if (d->coasting) {
        font_draw_str(250, 36, "COAST", COLOR_GREEN, 2);
        font_draw_str(250, 56, "fuel cut", t->text_secondary, 1);
    } else if (d->idle && d->instant_lh > 0.0f) {
        gauge_draw_numeric(250, 36, d->instant_lh, "%.2f", "L/h", t->text_primary, 2);
        font_draw_str(250, 56, "at idle", t->text_secondary, 1);
    } else if (d->instant_l100km > 0.0f) {
        gauge_draw_numeric(250, 36, d->instant_l100km, "%.1f", "L/100", t->text_primary, 2);
    } else {
        font_draw_str(250, 36, "---", t->text_secondary, 2);
    }

    draw_divider(0, 90, SCREEN_W);

    /* --- Trip stats + fuel bar --- */
    gauge_draw_label(10,  96, "TRIP AVG",  t->text_secondary, 1);
    gauge_draw_label(180, 96, "USED",      t->text_secondary, 1);
    gauge_draw_label(300, 96, "RANGE",     t->text_secondary, 1);

    if (d->trip_l100km > 0.0f) {
        char buf[12];
        snprintf(buf, sizeof(buf), "%.1f L/100", d->trip_l100km);
        font_draw_str(10, 108, buf, t->text_primary, 1);
    } else {
        font_draw_str(10, 108, "---", t->text_secondary, 1);
    }

    {
        char buf[12];
        if (d->trip_fuel_l > 0.0f)
            snprintf(buf, sizeof(buf), "%.2f L", d->trip_fuel_l);
        else
            snprintf(buf, sizeof(buf), "---");
        font_draw_str(180, 108, buf, d->trip_fuel_l > 0.0f ? t->text_primary : t->text_secondary, 1);
    }

    if (d->range_km > 0.0f) {
        uint32_t rc = (d->range_km < 50.0f)  ? t->danger :
                      (d->range_km < 100.0f) ? t->warn : t->text_primary;
        char buf[12];
        snprintf(buf, sizeof(buf), "%.0f km", d->range_km);
        font_draw_str(300, 108, buf, rc, 1);
    } else {
        font_draw_str(300, 108, "---", t->text_secondary, 1);
    }

    /* Fuel level bar */
    {
        uint32_t fl_col = (vs->fuel_level_pct < 15.0f) ? t->danger :
                          (vs->fuel_level_pct < 25.0f) ? t->warn : t->gauge_fill;
        gauge_draw_bar(10, 122, 350, 10,
                       vs->fuel_level_pct, 0.0f, 100.0f, fl_col, t->gauge_track);
        char fbuf[12];
        snprintf(fbuf, sizeof(fbuf), "%.0f%% fuel", vs->fuel_level_pct);
        font_draw_str(370, 122, fbuf, fl_col, 1);
    }

    draw_divider(0, 140, SCREEN_W);

    /* --- AFR + relative power --- */
    gauge_draw_label(10, 146, "AFR STATUS", t->text_secondary, 1);
    if (d->afr_state < 0) {
        font_draw_str(10, 158, "---", RGBA(70,70,70,255), 1);
    } else {
        static const char *afl[] = {"LEAN", "STOICH", "RICH"};
        static const char *afd[] = {"too little fuel", "ideal AFR", "too much fuel"};
        uint32_t afc = (d->afr_state == 0) ? t->danger :
                       (d->afr_state == 1) ? COLOR_GREEN : t->warn;
        font_draw_str(10, 158, afl[d->afr_state], afc, 2);
        font_draw_str(10, 176, afd[d->afr_state], t->text_secondary, 1);
    }

    gauge_draw_label(270, 146, "POWER", t->text_secondary, 1);
    gauge_draw_bar(270, 158, 190, 12,
                   d->power_pct, 0.0f, 100.0f, t->accent, t->gauge_track);
    {
        char pbuf[8];
        snprintf(pbuf, sizeof(pbuf), "%.0f%%", d->power_pct);
        font_draw_str(270, 174, pbuf, t->accent, 1);
    }

    draw_divider(0, 188, SCREEN_W);

    /* --- G-force bar --- */
    gauge_draw_label(10, 194, "G-FORCE", t->text_secondary, 1);
    draw_g_bar(10, 206, 460, d->accel_g, t);
    {
        char gbuf[12];
        uint32_t gcol = (d->accel_g >  0.05f) ? t->accent :
                        (d->accel_g < -0.05f) ? t->warn : t->text_primary;
        snprintf(gbuf, sizeof(gbuf), "%+.2fG", d->accel_g);
        font_draw_str(220, 218, gbuf, gcol, 1);
    }

    draw_divider(0, 228, SCREEN_W);

    /* --- Bottom: temps + session peaks + reset hint --- */
    {
        uint32_t ct_col = (vs->coolant_temp_c > 100.0f) ? t->danger :
                          (vs->coolant_temp_c >  90.0f) ? t->warn : t->text_primary;
        uint32_t vt_col = (vs->voltage_v < 11.5f) ? t->danger :
                          (vs->voltage_v < 12.0f) ? t->warn : t->text_primary;
        char buf[16];
        snprintf(buf, sizeof(buf), "CLT:%.0fC", vs->coolant_temp_c);
        font_draw_str(10, 234, buf, ct_col, 1);
        snprintf(buf, sizeof(buf), "IAT:%.0fC", vs->iat_c);
        font_draw_str(110, 234, buf, t->text_primary, 1);
        snprintf(buf, sizeof(buf), "AMB:%.0fC", vs->ambient_temp_c);
        font_draw_str(210, 234, buf, t->text_primary, 1);
        snprintf(buf, sizeof(buf), "VLT:%.1fV", vs->voltage_v);
        font_draw_str(310, 234, buf, vt_col, 1);
    }
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "PEAK: %.0fkm/h  %.0frpm",
                 s_peak_speed, s_peak_rpm);
        font_draw_str(10, 248, buf, t->text_secondary, 1);
        font_draw_str(340, 248, "Tri: reset trip", t->text_secondary, 1);
    }
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
/* Arcade / pixel-car dashboard                                        */
/* ------------------------------------------------------------------ */

#define AC_BG     RGBA(  8,   8,  12, 255)
#define AC_PANEL  RGBA( 14,  14,  22, 255)
#define AC_BORDER RGBA( 40, 100, 180, 255)
#define AC_CYAN   RGBA(  0, 220, 255, 255)
#define AC_DIM    RGBA( 60,  60,  80, 255)
#define AC_GREEN  RGBA(  0, 200,  80, 255)
#define AC_ROAD   RGBA( 18,  18,  28, 255)
#define AC_RDASH  RGBA( 70,  70,  95, 255)

static void ac_box(int x, int y, int w, int h) {
    renderer_draw_rect(x+1,   y+1,   w-2, h-2, AC_PANEL);
    renderer_draw_rect(x,     y,     w,   1,   AC_BORDER);
    renderer_draw_rect(x,     y+h-1, w,   1,   AC_BORDER);
    renderer_draw_rect(x,     y,     1,   h,   AC_BORDER);
    renderer_draw_rect(x+w-1, y,     1,   h,   AC_BORDER);
}

/* Pixel sedan, front faces right, 60x29px.
   state: 0=normal silver, 1=high-rpm green, 2=overtemp red.
   y+29 = wheel bottom; caller places car so y+29 aligns with road top. */
static void ac_pixel_car(int x, int y, uint32_t tick, int state) {
    uint32_t body   = (state==1) ? RGBA( 30,140, 60,255) :
                      (state==2) ? RGBA(170, 40, 40,255) : RGBA(192,192,186,255);
    uint32_t roof   = (state==1) ? RGBA( 20,110, 45,255) :
                      (state==2) ? RGBA(140, 30, 30,255) : RGBA(176,176,170,255);
    uint32_t pillar = (state==1) ? RGBA( 15, 90, 35,255) :
                      (state==2) ? RGBA(120, 25, 25,255) : RGBA(155,155,149,255);
    uint32_t seam   = (state==1) ? RGBA( 10, 70, 25,255) :
                      (state==2) ? RGBA(100, 15, 15,255) : RGBA(138,138,132,255);
    uint32_t win    = RGBA( 15,  32,  65, 255);
    uint32_t whl    = RGBA( 25,  25,  38, 255);
    uint32_t hub    = RGBA(110, 110, 135, 255);
    uint32_t light  = RGBA(255, 240, 120, 255);
    uint32_t tail   = RGBA(210,  40,  40, 255);
    uint32_t under  = RGBA( 15,  15,  24, 255);

    /* cabin -- low roofline sedan profile */
    renderer_draw_rect(x+13, y,    34, 10, roof);
    renderer_draw_rect(x+15, y+1,  11,  7, win);
    renderer_draw_rect(x+27, y+1,   3,  7, pillar);
    renderer_draw_rect(x+31, y+1,  13,  7, win);
    /* body */
    renderer_draw_rect(x,    y+10, 60, 16, body);
    renderer_draw_rect(x+2,  y+15, 56,  1, RGBA(228,228,228,255));  /* chrome waist strip */
    renderer_draw_rect(x+4,  y+22, 52,  4, under);
    renderer_draw_rect(x+12, y+10,  1, 10, seam);
    renderer_draw_rect(x+40, y+10,  1, 10, seam);
    /* front face (right side) */
    renderer_draw_rect(x+56, y+11,  4,  6, pillar);
    renderer_draw_rect(x+57, y+12,  2,  3, light);
    /* rear face (left side) */
    renderer_draw_rect(x,    y+11,  4,  6, pillar);
    renderer_draw_rect(x+1,  y+12,  2,  3, tail);
    /* wheel arches */
    renderer_draw_rect(x+5,  y+21, 12,  5, under);
    renderer_draw_rect(x+43, y+21, 12,  5, under);
    /* wheels */
    renderer_draw_rect(x+6,  y+21, 10,  8, whl);
    renderer_draw_rect(x+44, y+21, 10,  8, whl);
    renderer_draw_rect(x+7,  y+22,  8,  2, RGBA(42,42,55,255));
    renderer_draw_rect(x+45, y+22,  8,  2, RGBA(42,42,55,255));
    /* spinning hub: 4-frame clockwise dot orbit */
    static const int8_t hdx[4] = {3, 4, 3, 2};
    static const int8_t hdy[4] = {2, 3, 4, 3};
    int fr = (int)(tick / 70) % 4;
    renderer_draw_rect(x+6  + hdx[fr], y+21 + hdy[fr], 2, 2, hub);
    renderer_draw_rect(x+44 + hdx[fr], y+21 + hdy[fr], 2, 2, hub);
}

/* Exhaust puffs drifting left from car rear (rx, ry). */
static void ac_exhaust(int rx, int ry, uint32_t tick) {
    for (int i = 0; i < 4; i++) {
        int d  = (int)((tick / 55 + (uint32_t)(i * 150)) % 20);
        int px = rx - d * 2 - 2;
        int py = ry - d / 6;
        if (px < 0) continue;
        int v = 120 - d * 5;
        if (v < 15) continue;
        int sz = (d < 4) ? 3 : (d < 10) ? 2 : 1;
        renderer_draw_rect(px, py, sz, sz, RGBA((uint8_t)(v+30),(uint8_t)(v+30),(uint8_t)(v+40),255));
    }
}

/* City skyline silhouette drawn at bottom of sky, above road. */
static void ac_skyline(int x0, int y_road) {
    static const struct { int8_t x; uint8_t w, h; } bld[] = {
        {  2, 22, 42}, { 28, 16, 58}, { 50, 28, 28},
        { 82, 20, 62}, {106, 14, 44}, {122, 30, 36},
        {155, 18, 52}, {177, 20, 40},
    };
    uint32_t bc = RGBA(18, 22, 34, 255);
    uint32_t wc = RGBA(50, 54, 78, 255);
    int nb = (int)(sizeof(bld) / sizeof(bld[0]));
    for (int i = 0; i < nb; i++) {
        int bx = x0 + bld[i].x;
        int by = y_road - (int)bld[i].h;
        renderer_draw_rect(bx, by, (int)bld[i].w, (int)bld[i].h, bc);
        for (int wy = by + 3; wy <= y_road - 8; wy += 8)
            for (int wx = bx + 3; wx <= bx + (int)bld[i].w - 5; wx += 6)
                renderer_draw_rect(wx, wy, 2, 2, wc);
    }
}

/* Wide smoke cloud billowing from rear wheels on launch. */
static void ac_tire_smoke(int car_x, int car_y, uint32_t tick) {
    for (int i = 0; i < 8; i++) {
        int d  = (int)((tick / 45 + (uint32_t)(i * 5)) % 18);
        int px = car_x + 4 - d * 3;
        int py = car_y + 22 - d / 3;
        if (px < 0) continue;
        int v  = 170 - d * 8;
        if (v < 20) continue;
        int sz = 3 + d / 3;
        renderer_draw_rect(px, py, sz, sz, RGBA((uint8_t)v,(uint8_t)v,(uint8_t)v,255));
    }
}

/* Scrolling road with center dashes; offset is caller-managed for speed-scaling. */
static void ac_road(int x0, int w, int y0, int h, int offset) {
    renderer_draw_rect(x0, y0,     w, h, AC_ROAD);
    renderer_draw_rect(x0, y0,     w, 2, RGBA(55,55,75,255));
    renderer_draw_rect(x0, y0+h-2, w, 2, RGBA(55,55,75,255));
    int period   = 32;
    int dash_len = 18;
    int off      = offset % period;
    if (off < 0) off += period;
    int cy       = y0 + h / 2 - 1;
    for (int dx = -(period - off); dx < w + period; dx += period) {
        int sx = x0 + dx, ex = sx + dash_len;
        if (ex <= x0 || sx >= x0 + w) continue;
        if (sx < x0)     sx = x0;
        if (ex > x0 + w) ex = x0 + w;
        renderer_draw_rect(sx, cy, ex - sx, 2, AC_RDASH);
    }
}

/* Scrolling street lampposts for night sky. offset = same road scroll accumulator. */
static void ac_lampposts(int x0, int y_road, int offset) {
    int spacing = 60;
    for (int i = -1; i <= 4; i++) {
        int px  = x0 + i * spacing - (offset % spacing);
        if (px > x0 + 200 || px + 22 < x0) continue;
        int top = y_road - 52;
        if (top < 15) top = 15;
        /* pole */
        if (px >= x0 && px + 2 <= x0 + 200)
            renderer_draw_rect(px, top, 2, y_road - top, RGBA(58, 58, 68, 255));
        /* horizontal arm extending right */
        {
            int ax = px, aw = 18;
            if (ax < x0)     { aw -= (x0 - ax); ax = x0; }
            if (ax + aw > x0 + 200) aw = x0 + 200 - ax;
            if (aw > 0)
                renderer_draw_rect(ax, top, aw, 2, RGBA(58, 58, 68, 255));
        }
        /* lantern */
        int lx = px + 16, ly = top - 2;
        if (lx >= x0 && lx + 4 <= x0 + 200)
            renderer_draw_rect(lx, ly, 4, 5, RGBA(255, 235, 140, 255));
        /* light cone downward */
        for (int d = 1; d <= 28; d++) {
            int cy = top + 2 + d;
            if (cy >= y_road) break;
            int half = d * 2 / 3;
            int cx = lx + 2 - half;
            int cw = half * 2 + 1;
            if (cx < x0)     { cw -= (x0 - cx); cx = x0; }
            if (cx + cw > x0 + 200) cw = x0 + 200 - cx;
            if (cw <= 0) continue;
            int v = 55 - d;
            if (v < 8) v = 8;
            renderer_draw_rect(cx, cy, cw, 1,
                               RGBA((uint8_t)v, (uint8_t)v, (uint8_t)(v / 3), 255));
        }
    }
}

void dashboard_render_arcade(const VehicleState *vs, const DerivedState *d) {
    uint32_t     tick = time_now_ms();
    const Theme *t    = theme_current();

    renderer_draw_rect(0, 0, SCREEN_W, 272, AC_BG);

    /* ---- TOP BAR  y=0..13 ---- */
    renderer_draw_rect(0, 0, SCREEN_W, 14, AC_PANEL);
    renderer_draw_rect(0, 13, SCREEN_W, 1, AC_BORDER);
    font_draw_str(6, 3, "DRIVE", AC_CYAN, 1);
    {
        char buf[12];
        uint32_t vc = (vs->voltage_v < 11.5f) ? t->danger :
                      (vs->voltage_v < 12.5f) ? t->warn   : AC_DIM;
        snprintf(buf, sizeof(buf), "%.1fV", vs->voltage_v);
        font_draw_str(274, 3, "VOLT", AC_DIM, 1);
        font_draw_str(314, 3, buf, vc, 1);
    }
    {
        char gbuf[4];
        uint32_t gc = d->idle ? AC_DIM : AC_CYAN;
        int cg = (d->gear > 0 && d->gear <= 9) ? d->gear : 0;
        if (cg == 0) { gbuf[0] = 'N'; gbuf[1] = '\0'; }
        else         { gbuf[0] = (char)('0' + cg); gbuf[1] = '\0'; }
        font_draw_str(380, 3, "GEAR", AC_DIM, 1);
        font_draw_str(420, 3, gbuf, gc, 1);
    }
    if (d->coasting)
        font_draw_str(446, 3, "CST", AC_GREEN, 1);
    else if (d->idle)
        font_draw_str(450, 3, "IDL", AC_DIM, 1);

    /* ---- MAIN AREA  y=15..148 ---- */
    renderer_draw_rect(200, 15, 1, 134, AC_BORDER);

    /* ---- car scene  x=0..199, y=15..148 ---- */
    {
        /* road offset scales with speed: fast speed = fast dashes */
        static float    s_road_px   = 0.0f;
        static uint32_t s_prev_tick = 0;
        uint32_t dt = tick - s_prev_tick;
        if (dt > 100) dt = 100;
        s_prev_tick = tick;
        s_road_px  += vs->speed_kmh * (float)dt * 0.004f;
        if (s_road_px >= 32000.0f) s_road_px -= 32000.0f;

        /* day / night sky based on PSP local clock */
        {
            ScePspDateTime rtc_t;
            int hour = 12;
            if (sceRtcGetCurrentClockLocalTime(&rtc_t) == 0)
                hour = (int)rtc_t.hour;
            int is_night = (hour < 6 || hour >= 20);

            if (is_night) {
                renderer_draw_rect(0, 15, 200, 103, RGBA(4, 4, 15, 255));
                static const struct { uint8_t x, y; } stars[] = {
                    {10,5},{35,18},{60,8},{85,25},{110,4},{135,15},{160,22},{185,9},
                    {22,30},{75,35},{140,28},{178,40},{50,45},{100,50},{155,48},
                };
                int ns = (int)(sizeof(stars) / sizeof(stars[0]));
                for (int si = 0; si < ns; si++)
                    renderer_draw_rect((int)stars[si].x, 15 + (int)stars[si].y,
                                       1, 1, RGBA(180, 180, 200, 255));
                ac_skyline(0, 118);
                ac_lampposts(0, 118, (int)s_road_px);
            } else {
                renderer_draw_rect(0,  15, 200, 40, RGBA( 22,  68, 162, 255));
                renderer_draw_rect(0,  55, 200, 40, RGBA( 52, 112, 200, 255));
                renderer_draw_rect(0,  95, 200, 23, RGBA( 98, 152, 220, 255));
                ac_skyline(0, 118);
            }
        }
        ac_road(0, 200, 118, 30, (int)s_road_px);

        /* bounce ±1px at RPM >= 4000 */
        int bounce    = (vs->rpm >= 4000.0f) ? ((int)(tick / 55) % 2 ? -1 : 0) : 0;
        int car_state = (vs->coolant_temp_c > 100.0f) ? 2 :
                        (vs->rpm >= 5500.0f)          ? 1 : 0;
        ac_pixel_car(70, 89 + bounce, tick, car_state);

        /* tire smoke on launch, else normal exhaust */
        if (vs->throttle_pct > 88.0f && vs->speed_kmh < 8.0f)
            ac_tire_smoke(70, 89 + bounce, tick);
        else
            ac_exhaust(68, 113 + bounce, tick);

        /* sky info text */
        if (d->instant_l100km > DERIVED_NO_DATA) {
            char buf[14];
            snprintf(buf, sizeof(buf), "%.1f L/100", d->instant_l100km);
            font_draw_str(4, 20, buf, AC_GREEN, 1);
        }
        if (d->trip_dist_km > 0.0f) {
            char buf[10];
            snprintf(buf, sizeof(buf), "%.1fkm", d->trip_dist_km);
            font_draw_str(4, 30, buf, AC_DIM, 1);
        }
    }

    /* right panel  x=203..476, y=15..148  (rw=274) */
    {
        int rx = 203, rw = 274;

        /* RPM */
        font_draw_str(rx, 17, "RPM", AC_DIM, 1);
        {
            char buf[8];
            snprintf(buf, sizeof(buf), "%.0f", vs->rpm);
            uint32_t rc = (vs->rpm >= 6500.0f) ? t->redline :
                          (vs->rpm >= 4000.0f) ? t->warn     : AC_CYAN;
            int tw = font_str_width(buf, 3);
            font_draw_str(rx + rw - tw - 2, 17, buf, rc, 3);
        }
        /* shift lights  y=42..47: 5 dots, thresholds 4000/5000/5800/6300/7000 */
        {
            static const float sl_thr[5] = {4000,5000,5800,6300,7000};
            static const uint32_t sl_on[5] = {
                RGBA(  0,200, 80,255), RGBA(  0,200, 80,255), RGBA(  0,220,255,255),
                RGBA(255,200,  0,255), RGBA(255, 50, 50,255)
            };
            int all5 = (vs->rpm >= sl_thr[4]);
            int flash = all5 && ((tick / 80) % 2);
            int lw = 40, gap = 10;
            int ltotal = 5*lw + 4*gap;
            int lx0 = rx + (rw - ltotal) / 2;
            for (int i = 0; i < 5; i++) {
                int lx = lx0 + i*(lw+gap);
                int lit = (vs->rpm >= sl_thr[i]);
                uint32_t c = (lit && !flash) ? sl_on[i] : RGBA(16,16,24,255);
                renderer_draw_rect(lx, 42, lw, 6, c);
            }
        }

        /* RPM bar  y=50..60 */
        renderer_draw_rect(rx, 50, rw, 11, RGBA(16,16,24,255));
        {
            float pct = vs->rpm / 8000.0f;
            if (pct > 1.0f) pct = 1.0f;
            int fill = (int)(pct * rw);
            int z1   = (int)(4000.0f / 8000.0f * rw);
            int z2   = (int)(6500.0f / 8000.0f * rw);
            if (fill > 0) {
                if (fill <= z1) {
                    renderer_draw_rect(rx,    50, fill,      11, AC_CYAN);
                } else if (fill <= z2) {
                    renderer_draw_rect(rx,    50, z1,        11, AC_CYAN);
                    renderer_draw_rect(rx+z1, 50, fill-z1,   11, t->warn);
                } else {
                    renderer_draw_rect(rx,    50, z1,        11, AC_CYAN);
                    renderer_draw_rect(rx+z1, 50, z2-z1,     11, t->warn);
                    renderer_draw_rect(rx+z2, 50, fill-z2,   11, t->redline);
                }
            }
            /* redline tick */
            renderer_draw_rect(rx + z2, 48, 1, 15, RGBA(255,255,60,200));
        }
        font_draw_str(rx,        63, "0",    AC_DIM, 1);
        font_draw_str(rx+rw-34,  63, "8000", AC_DIM, 1);

        renderer_draw_rect(rx, 73, rw, 1, AC_DIM);

        /* SPEED */
        font_draw_str(rx, 75, "km/h", AC_DIM, 1);
        {
            char buf[8];
            snprintf(buf, sizeof(buf), "%.0f", vs->speed_kmh);
            uint32_t sc = (vs->speed_kmh > 160.0f) ? t->danger :
                          (vs->speed_kmh > 100.0f) ? t->warn   :
                          RGBA(220,220,220,255);
            int tw = font_str_width(buf, 2);
            font_draw_str(rx + rw - tw - 2, 75, buf, sc, 2);
        }
        /* speed bar  y=94..103 */
        renderer_draw_rect(rx, 94, rw, 10, RGBA(16,16,24,255));
        {
            float pct = vs->speed_kmh / 240.0f;
            if (pct > 1.0f) pct = 1.0f;
            int fill = (int)(pct * rw);
            if (fill > 0)
                renderer_draw_rect(rx, 94, fill, 10, RGBA(200,200,200,255));
        }
        font_draw_str(rx,       106, "0",   AC_DIM, 1);
        font_draw_str(rx+rw-26, 106, "240", AC_DIM, 1);

        renderer_draw_rect(rx, 116, rw, 1, AC_DIM);

        /* THROTTLE + LOAD side-by-side  y=118..148 */
        {
            int hw = (rw - 4) / 2;
            /* THR */
            font_draw_str(rx, 118, "THR", AC_DIM, 1);
            {
                char buf[8];
                snprintf(buf, sizeof(buf), "%.0f%%", vs->throttle_pct);
                uint32_t tc = (vs->throttle_pct > 80.0f) ? t->redline :
                              (vs->throttle_pct > 50.0f) ? t->warn    : AC_GREEN;
                font_draw_str(rx+26, 118, buf, tc, 1);
                renderer_draw_rect(rx, 128, hw, 8, RGBA(16,16,24,255));
                int f = (int)(vs->throttle_pct / 100.0f * hw);
                if (f > 0) renderer_draw_rect(rx, 128, f, 8, tc);
            }
            /* LOAD */
            int lx = rx + hw + 4;
            font_draw_str(lx, 118, "LOAD", AC_DIM, 1);
            {
                char buf[8];
                snprintf(buf, sizeof(buf), "%.0f%%", vs->engine_load_pct);
                uint32_t lc = (vs->engine_load_pct > 80.0f) ? t->redline :
                              (vs->engine_load_pct > 60.0f) ? t->warn    : AC_GREEN;
                font_draw_str(lx+32, 118, buf, lc, 1);
                renderer_draw_rect(lx, 128, hw, 8, RGBA(16,16,24,255));
                int f = (int)(vs->engine_load_pct / 100.0f * hw);
                if (f > 0) renderer_draw_rect(lx, 128, f, 8, lc);
            }
        }
    }

    /* ---- DIVIDER ---- */
    renderer_draw_rect(0, 149, SCREEN_W, 1, AC_BORDER);

    /* ---- ROW 2: COOL | IAT | STFT | LTFT   y=150..178 ---- */
    {
        int bh = 29, y2 = 150;
        ac_box(  0, y2, 120, bh);
        ac_box(120, y2, 120, bh);
        ac_box(240, y2, 120, bh);
        ac_box(360, y2, 120, bh);
        font_draw_str(  4, y2+3, "COOL", AC_DIM, 1);
        font_draw_str(124, y2+3, "IAT",  AC_DIM, 1);
        font_draw_str(244, y2+3, "STFT", AC_DIM, 1);
        font_draw_str(364, y2+3, "LTFT", AC_DIM, 1);
        {
            char buf[10];
            snprintf(buf, sizeof(buf), "%.0fC", vs->coolant_temp_c);
            uint32_t c = (vs->coolant_temp_c > 100.0f) ? t->danger :
                         (vs->coolant_temp_c >  90.0f) ? t->warn   : RGBA(220,220,220,255);
            font_draw_str(4, y2+14, buf, c, 1);
        }
        {
            char buf[10];
            snprintf(buf, sizeof(buf), "%.0fC", vs->iat_c);
            font_draw_str(124, y2+14, buf, RGBA(220,220,220,255), 1);
        }
        {
            char buf[10];
            snprintf(buf, sizeof(buf), "%+.1f%%", vs->stft_pct);
            uint32_t c = (vs->stft_pct > 10.f || vs->stft_pct < -10.f) ? t->danger :
                         (vs->stft_pct >  5.f || vs->stft_pct <  -5.f) ? t->warn   : AC_GREEN;
            font_draw_str(244, y2+14, buf, c, 1);
        }
        {
            char buf[10];
            snprintf(buf, sizeof(buf), "%+.1f%%", vs->ltft_pct);
            uint32_t c = (vs->ltft_pct > 10.f || vs->ltft_pct < -10.f) ? t->danger :
                         (vs->ltft_pct >  5.f || vs->ltft_pct <  -5.f) ? t->warn   : AC_GREEN;
            font_draw_str(364, y2+14, buf, c, 1);
        }
    }

    renderer_draw_rect(0, 179, SCREEN_W, 1, AC_BORDER);

    /* ---- ROW 3: TIMING | AFR | MAF | FUEL   y=180..208 ---- */
    {
        int bh = 29, y3 = 180;
        ac_box(  0, y3, 120, bh);
        ac_box(120, y3, 120, bh);
        ac_box(240, y3, 120, bh);
        ac_box(360, y3, 120, bh);
        font_draw_str(  4, y3+3, "TIMING", AC_DIM, 1);
        font_draw_str(124, y3+3, "AFR",    AC_DIM, 1);
        font_draw_str(244, y3+3, "MAF",    AC_DIM, 1);
        font_draw_str(364, y3+3, "FUEL",   AC_DIM, 1);
        {
            char buf[10];
            snprintf(buf, sizeof(buf), "%+.1f", vs->timing_adv_deg);
            font_draw_str(4, y3+14, buf, RGBA(220,220,220,255), 1);
        }
        {
            if (d->afr_state < 0) {
                font_draw_str(124, y3+14, "N/A", AC_DIM, 1);
            } else {
                static const char *afrl[] = {"LEAN","STOICH","RICH"};
                uint32_t c = (d->afr_state == 0) ? t->danger :
                             (d->afr_state == 1) ? AC_GREEN  : t->warn;
                font_draw_str(124, y3+14, afrl[d->afr_state], c, 1);
            }
        }
        {
            char buf[10];
            snprintf(buf, sizeof(buf), "%.1fg/s", vs->maf_gs);
            font_draw_str(244, y3+14, buf, RGBA(220,220,220,255), 1);
        }
        {
            char buf[8];
            snprintf(buf, sizeof(buf), "%.0f%%", vs->fuel_level_pct);
            uint32_t fc = (vs->fuel_level_pct < 15.0f) ? t->danger :
                          (vs->fuel_level_pct < 30.0f) ? t->warn   : AC_GREEN;
            font_draw_str(364, y3+14, buf, fc, 1);
            int bw = 110;
            renderer_draw_rect(364, y3+23, bw, 4, RGBA(16,16,24,255));
            int f = (int)(vs->fuel_level_pct / 100.0f * bw);
            if (f > 0) renderer_draw_rect(364, y3+23, f, 4, fc);
        }
    }

    renderer_draw_rect(0, 209, SCREEN_W, 1, AC_BORDER);

    /* ---- ROW 4: G-force  y=210..271 ---- */
    font_draw_str(6, 212, "G-FORCE", AC_DIM, 1);
    draw_g_bar(6, 222, 468, d->accel_g, t);
    font_draw_str(  6, 237, "-2G", AC_DIM, 1);
    font_draw_str(230, 237, "0G",  AC_DIM, 1);
    font_draw_str(455, 237, "+2G", AC_DIM, 1);
    if (d->coasting)
        font_draw_str(172, 250, ">>  COASTING  <<", AC_CYAN, 1);
    else if (d->idle)
        font_draw_str(218, 250, "IDLE", AC_DIM, 1);
}

/* ------------------------------------------------------------------ */
/* Drag dashboard                                                      */
/* ------------------------------------------------------------------ */

static PerfState g_drag_perf = {0};
void dashboard_drag_reset(void) { g_drag_perf = (PerfState){0}; }

void dashboard_render_drag(const VehicleState *vs, const DerivedState *d) {
    PerfState *perf = &g_drag_perf;
    uint32_t     tick = time_now_ms();
    const Theme *t    = theme_current();

    renderer_draw_rect(0, 0, SCREEN_W, 272, AC_BG);

    /* ---- RPM color strip  y=0..10 ---- */
    {
        renderer_draw_rect(0, 0, SCREEN_W, 11, RGBA(16,16,24,255));
        float pct = vs->rpm / 8000.0f;
        if (pct > 1.0f) pct = 1.0f;
        int fill = (int)(pct * SCREEN_W);
        int z1   = (int)(4000.0f / 8000.0f * SCREEN_W);
        int z2   = (int)(6500.0f / 8000.0f * SCREEN_W);
        if (fill > 0) {
            if (fill <= z1) {
                renderer_draw_rect(0,  0, fill,    11, AC_CYAN);
            } else if (fill <= z2) {
                renderer_draw_rect(0,  0, z1,      11, AC_CYAN);
                renderer_draw_rect(z1, 0, fill-z1, 11, t->warn);
            } else {
                renderer_draw_rect(0,  0, z1,      11, AC_CYAN);
                renderer_draw_rect(z1, 0, z2-z1,   11, t->warn);
                renderer_draw_rect(z2, 0, fill-z2, 11, t->redline);
            }
        }
        /* shift lights: 5 dots at right end */
        static const float sl_thr[5] = {4000,5000,5800,6300,7000};
        static const uint32_t sl_on[5] = {
            RGBA(0,200,80,255), RGBA(0,200,80,255), RGBA(0,220,255,255),
            RGBA(255,200,0,255), RGBA(255,50,50,255)
        };
        int flash5 = (vs->rpm >= sl_thr[4]) && ((tick/80)%2);
        for (int i = 0; i < 5; i++) {
            int lit = (vs->rpm >= sl_thr[i]) && !flash5;
            int lx  = SCREEN_W - 5*(14+3) + i*17;
            renderer_draw_rect(lx, 1, 14, 9, lit ? sl_on[i] : RGBA(20,20,30,255));
        }
    }
    renderer_draw_rect(0, 11, SCREEN_W, 1, AC_BORDER);

    /* ---- state machine ---- */
    int done   = (!perf->timing_active && perf->accel_end_ms > 0);
    int timing = perf->timing_active;

    /* Auto-start: speed < 5, throttle > 80% */
    if (!timing && !done && vs->speed_kmh < 5.0f && vs->throttle_pct > 80.0f) {
        perf->timing_active   = 1;
        perf->accel_start_ms  = tick;
        perf->accel_end_ms    = 0;
        perf->peak_rpm        = vs->rpm;
    }
    /* Auto-stop: hit 100 km/h */
    if (timing && vs->speed_kmh >= 100.0f) {
        perf->timing_active = 0;
        perf->accel_end_ms  = tick;
        float elapsed = (float)(tick - perf->accel_start_ms) / 1000.0f;
        if (perf->best_0_100 <= 0.0f || elapsed < perf->best_0_100)
            perf->best_0_100 = elapsed;
    }
    if (timing && vs->rpm > perf->peak_rpm)
        perf->peak_rpm = vs->rpm;

    /* ---- label  y=14..21 ---- */
    {
        const char *lbl = "0 - 100 km/h";
        int tw = font_str_width(lbl, 1);
        font_draw_str((SCREEN_W - tw) / 2, 14, lbl, AC_DIM, 1);
    }

    /* ---- timer  y=24..63 (scale=4, 32px tall) ---- */
    {
        char buf[12];
        float elapsed = 0.0f;
        if (timing)
            elapsed = (float)(tick - perf->accel_start_ms) / 1000.0f;
        else if (done)
            elapsed = (float)(perf->accel_end_ms - perf->accel_start_ms) / 1000.0f;

        uint32_t tc;
        if (done)        { snprintf(buf, sizeof(buf), "%.2f", elapsed); tc = AC_GREEN; }
        else if (timing) { snprintf(buf, sizeof(buf), "%.2f", elapsed); tc = RGBA(255,255,255,255); }
        else             { snprintf(buf, sizeof(buf), "--.-"); tc = AC_DIM; }

        int tw = font_str_width(buf, 4);
        font_draw_str((SCREEN_W - tw) / 2, 24, buf, tc, 4);
    }

    /* ---- state text  y=66..73 ---- */
    {
        const char *stxt;
        uint32_t sc;
        if (done)        { stxt = "DONE";           sc = AC_GREEN; }
        else if (timing) { stxt = "TIMING...";      sc = RGBA(255,255,255,255); }
        else             { stxt = "FLOOR IT FROM STOP"; sc = AC_DIM; }
        int tw = font_str_width(stxt, 1);
        font_draw_str((SCREEN_W - tw) / 2, 66, stxt, sc, 1);
    }

    renderer_draw_rect(0, 76, SCREEN_W, 1, AC_BORDER);

    /* ---- speed + gear  y=78..109 ---- */
    {
        char sbuf[8], gbuf[4];
        snprintf(sbuf, sizeof(sbuf), "%.0f", vs->speed_kmh);
        int cg = (d->gear > 0 && d->gear <= 9) ? d->gear : 0;
        if (cg == 0) { gbuf[0]='N'; gbuf[1]='\0'; }
        else         { gbuf[0]=(char)('0'+cg); gbuf[1]='\0'; }

        uint32_t sc = (vs->speed_kmh >= 100.0f) ? AC_GREEN :
                      (vs->speed_kmh >  80.0f)  ? t->warn  : RGBA(220,220,220,255);
        int stw = font_str_width(sbuf, 3);
        font_draw_str((SCREEN_W - stw) / 2 - 20, 78, sbuf, sc, 3);
        font_draw_str((SCREEN_W + stw) / 2 -  8, 88, "km/h", AC_DIM, 1);
        font_draw_str((SCREEN_W + stw) / 2 +  8, 78, gbuf, AC_CYAN, 2);
    }
    /* speed bar  y=110..119 */
    renderer_draw_rect(40, 110, 400, 10, RGBA(16,16,24,255));
    {
        float pct = vs->speed_kmh / 100.0f;
        if (pct > 1.0f) pct = 1.0f;
        int fill = (int)(pct * 400);
        uint32_t sc = (vs->speed_kmh >= 100.0f) ? AC_GREEN : RGBA(200,200,200,255);
        if (fill > 0) renderer_draw_rect(40, 110, fill, 10, sc);
        /* 100 km/h marker */
        renderer_draw_rect(440, 108, 1, 14, RGBA(255,255,60,200));
    }
    font_draw_str(40, 121, "0", AC_DIM, 1);
    font_draw_str(426, 121, "100", AC_DIM, 1);

    renderer_draw_rect(0, 131, SCREEN_W, 1, AC_BORDER);

    /* ---- best + peak  y=133..143 ---- */
    {
        char buf[24];
        if (perf->best_0_100 > 0.0f)
            snprintf(buf, sizeof(buf), "BEST  %.2fs", perf->best_0_100);
        else
            snprintf(buf, sizeof(buf), "BEST  --");
        font_draw_str(10, 133, buf, AC_GREEN, 1);

        if (perf->peak_rpm > 0.0f)
            snprintf(buf, sizeof(buf), "PEAK RPM  %.0f", perf->peak_rpm);
        else
            snprintf(buf, sizeof(buf), "PEAK RPM  --");
        font_draw_str(260, 133, buf, AC_DIM, 1);
    }

    renderer_draw_rect(0, 145, SCREEN_W, 1, AC_BORDER);

    /* ---- THR + LOAD bars  y=147..176 ---- */
    {
        int hw = (SCREEN_W - 8) / 2;
        font_draw_str(4, 147, "THR", AC_DIM, 1);
        {
            char buf[8]; snprintf(buf, sizeof(buf), "%.0f%%", vs->throttle_pct);
            uint32_t tc = (vs->throttle_pct > 80.0f) ? t->redline :
                          (vs->throttle_pct > 50.0f) ? t->warn    : AC_GREEN;
            font_draw_str(30, 147, buf, tc, 1);
            renderer_draw_rect(4, 157, hw, 8, RGBA(16,16,24,255));
            int f = (int)(vs->throttle_pct / 100.0f * hw);
            if (f > 0) renderer_draw_rect(4, 157, f, 8, tc);
        }
        int lx = 4 + hw + 4;
        font_draw_str(lx, 147, "LOAD", AC_DIM, 1);
        {
            char buf[8]; snprintf(buf, sizeof(buf), "%.0f%%", vs->engine_load_pct);
            uint32_t lc = (vs->engine_load_pct > 80.0f) ? t->redline :
                          (vs->engine_load_pct > 60.0f) ? t->warn    : AC_GREEN;
            font_draw_str(lx+32, 147, buf, lc, 1);
            renderer_draw_rect(lx, 157, hw, 8, RGBA(16,16,24,255));
            int f = (int)(vs->engine_load_pct / 100.0f * hw);
            if (f > 0) renderer_draw_rect(lx, 157, f, 8, lc);
        }
    }

    renderer_draw_rect(0, 167, SCREEN_W, 1, AC_BORDER);

    /* ---- G-force bar  y=169..208 ---- */
    font_draw_str(6, 169, "G-FORCE", AC_DIM, 1);
    draw_g_bar(6, 179, 468, d->accel_g, t);
    font_draw_str(  6, 194, "-2G", AC_DIM, 1);
    font_draw_str(230, 194, "0G",  AC_DIM, 1);
    font_draw_str(455, 194, "+2G", AC_DIM, 1);

    renderer_draw_rect(0, 206, SCREEN_W, 1, AC_BORDER);

    /* ---- COOL + VOLT strip  y=208..271 ---- */
    {
        char buf[12];
        uint32_t cc = (vs->coolant_temp_c > 100.0f) ? t->danger :
                      (vs->coolant_temp_c >  90.0f) ? t->warn   : RGBA(220,220,220,255);
        snprintf(buf, sizeof(buf), "%.0fC", vs->coolant_temp_c);
        font_draw_str(6, 210, "COOL", AC_DIM, 1);
        font_draw_str(38, 210, buf, cc, 1);

        uint32_t vc = (vs->voltage_v < 11.5f) ? t->danger :
                      (vs->voltage_v < 12.5f) ? t->warn   : AC_DIM;
        snprintf(buf, sizeof(buf), "%.1fV", vs->voltage_v);
        font_draw_str(120, 210, "VOLT", AC_DIM, 1);
        font_draw_str(152, 210, buf, vc, 1);
    }

    /* X = reset */
    font_draw_str(6, 220, "X: reset", AC_DIM, 1);
}

/* ------------------------------------------------------------------ */
/* Status bar (bottom)                                                 */
/* ------------------------------------------------------------------ */

void dashboard_render_status_bar(const VehicleState *vs, DashMode mode, int connected) {
    const Theme *t = theme_current();
    static const char *mode_names[DASH_MODE_COUNT] = {
        "DIGITAL", "PERF", "ENGINE", "ECOTRIP", "JDM", "TOUGE", "VTEC", "ARCADE", "DRAG"
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

void dashboard_render(const VehicleState *vs, const DerivedState *d, DashMode mode) {
    static PerfState perf = {0};
    switch (mode) {
        case DASH_MODE_DIGITAL:     dashboard_render_digital(vs, d);            break;
        case DASH_MODE_PERFORMANCE: dashboard_render_performance(vs, &perf, d); break;
        case DASH_MODE_ENGINE:      dashboard_render_engine(vs);                break;
        case DASH_MODE_ECOTRIP:     dashboard_render_ecotrip(vs, d);            break;
        case DASH_MODE_JDM:         dashboard_render_jdm(vs, d);                break;
        case DASH_MODE_TOUGE:       dashboard_render_touge(vs, d);              break;
        case DASH_MODE_VTEC:        dashboard_render_vtec(vs, d);               break;
        case DASH_MODE_ARCADE:      dashboard_render_arcade(vs, d);             break;
        case DASH_MODE_DRAG:        dashboard_render_drag(vs, d);               break;
        default: break;
    }
}
