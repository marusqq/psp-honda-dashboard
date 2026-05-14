#pragma once
#include <stdint.h>
#include "../obd/pid.h"

#define TELEMETRY_STALE_MS 2000

typedef struct {
    /* Core */
    float    rpm;
    float    speed_kmh;
    float    coolant_temp_c;
    float    throttle_pct;
    float    iat_c;
    float    engine_load_pct;
    float    voltage_v;
    /* ECU internals */
    float    stft_pct;
    float    ltft_pct;
    float    timing_adv_deg;
    float    runtime_s;
    float    fuel_level_pct;
    float    ambient_temp_c;
    /* Sensors */
    float    map_kpa;
    float    maf_gs;
    float    o2_b1s1_v;
    float    o2_b1s2_v;
    float    baro_kpa;
    float    rel_throttle_pct;
    float    accel_pos_pct;
    float    oil_temp_c;
    float    fuel_rate_lh;
    float    ethanol_pct;
    /* Diagnostic counters */
    float    mil_time_min;
    float    clr_time_min;
    float    mil_dist_km;
    float    clr_dist_km;
    /* Per-PID metadata */
    uint32_t last_update_ms[PID_COUNT];
    int      valid[PID_COUNT];
    /* 0=unpolled, 1=confirmed supported, 2=confirmed unsupported */
    uint8_t  supported[PID_COUNT];
} VehicleState;

void  telemetry_init(VehicleState *state);
void  telemetry_update(VehicleState *state, PidIndex field, float value, uint32_t now_ms);
int   telemetry_is_stale(const VehicleState *state, PidIndex field, uint32_t now_ms);
float telemetry_get(const VehicleState *state, PidIndex field);
