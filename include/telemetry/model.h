#pragma once
#include <stdint.h>
#include "../obd/pid.h"

#define TELEMETRY_STALE_MS 2000

typedef struct {
    float    rpm;
    float    speed_kmh;
    float    coolant_temp_c;
    float    throttle_pct;
    float    iat_c;
    float    engine_load_pct;
    float    voltage_v;
    uint32_t last_update_ms[PID_COUNT];
    int      valid[PID_COUNT];
} VehicleState;

void telemetry_init(VehicleState *state);
void telemetry_update(VehicleState *state, PidIndex field, float value, uint32_t now_ms);
int  telemetry_is_stale(const VehicleState *state, PidIndex field, uint32_t now_ms);
float telemetry_get(const VehicleState *state, PidIndex field);
