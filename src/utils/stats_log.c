#include <stdio.h>
#include "utils/stats_log.h"
#include "utils/time.h"
#include "telemetry/derived.h"

#define STATS_PATH "ms0:/PSP/GAME/PSP-OBD2/stats.csv"

static FILE    *g_fp              = NULL;
static uint32_t g_session_start   = 0;

void stats_log_open(void) {
    if (g_fp) fclose(g_fp);
    g_fp = fopen(STATS_PATH, "a");
    if (!g_fp) return;
    g_session_start = time_now_ms();
    fprintf(g_fp,
        "#session\n"
        "t_s,rpm,spd_kmh,gear,clt_c,iat_c,thr_pct,load_pct,"
        "stft_pct,ltft_pct,timing_deg,maf_gs,volt_v,"
        "trip_km,trip_fuel_l,l100km,accel_g\n");
    fflush(g_fp);
}

void stats_log_sample(const VehicleState *vs, const DerivedState *d) {
    if (!g_fp) return;
    float t_s   = (float)(time_now_ms() - g_session_start) / 1000.0f;
    float l100  = (d->instant_l100km > DERIVED_NO_DATA) ? d->instant_l100km : 0.0f;
    fprintf(g_fp,
        "%.1f,%.0f,%.1f,%d,%.1f,%.1f,%.1f,%.1f,"
        "%.2f,%.2f,%.1f,%.2f,%.2f,"
        "%.2f,%.3f,%.1f,%.3f\n",
        t_s,
        vs->rpm, vs->speed_kmh, d->gear,
        vs->coolant_temp_c, vs->iat_c,
        vs->throttle_pct, vs->engine_load_pct,
        vs->stft_pct, vs->ltft_pct,
        vs->timing_adv_deg, vs->maf_gs, vs->voltage_v,
        d->trip_dist_km, d->trip_fuel_l,
        l100, d->accel_g);
    fflush(g_fp);
}

void stats_log_close(void) {
    if (g_fp) { fclose(g_fp); g_fp = NULL; }
}
