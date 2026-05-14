#include <string.h>
#include "telemetry/model.h"

static float *field_ptr(VehicleState *s, PidIndex f) {
    switch (f) {
        case PID_RPM:          return &s->rpm;
        case PID_SPEED:        return &s->speed_kmh;
        case PID_COOLANT_TEMP: return &s->coolant_temp_c;
        case PID_THROTTLE:     return &s->throttle_pct;
        case PID_IAT:          return &s->iat_c;
        case PID_ENGINE_LOAD:  return &s->engine_load_pct;
        case PID_VOLTAGE:      return &s->voltage_v;
        case PID_STFT:         return &s->stft_pct;
        case PID_LTFT:         return &s->ltft_pct;
        case PID_TIMING_ADV:   return &s->timing_adv_deg;
        case PID_RUNTIME:      return &s->runtime_s;
        case PID_FUEL_LEVEL:   return &s->fuel_level_pct;
        case PID_AMBIENT_TEMP: return &s->ambient_temp_c;
        case PID_MAP:          return &s->map_kpa;
        case PID_MAF:          return &s->maf_gs;
        case PID_O2_B1S1:      return &s->o2_b1s1_v;
        case PID_O2_B1S2:      return &s->o2_b1s2_v;
        case PID_BARO:         return &s->baro_kpa;
        case PID_REL_THROTTLE: return &s->rel_throttle_pct;
        case PID_ACCEL_POS:    return &s->accel_pos_pct;
        case PID_OIL_TEMP:     return &s->oil_temp_c;
        case PID_FUEL_RATE:    return &s->fuel_rate_lh;
        case PID_ETHANOL:      return &s->ethanol_pct;
        case PID_MIL_TIME:     return &s->mil_time_min;
        case PID_CLR_TIME:     return &s->clr_time_min;
        case PID_MIL_DIST:     return &s->mil_dist_km;
        case PID_CLR_DIST:     return &s->clr_dist_km;
        default:               return NULL;
    }
}

void telemetry_init(VehicleState *state) {
    memset(state, 0, sizeof(*state));
}

void telemetry_update(VehicleState *state, PidIndex field, float value, uint32_t now_ms) {
    float *fp = field_ptr(state, field);
    if (!fp) return;
    *fp = value;
    state->last_update_ms[field] = now_ms;
    state->valid[field] = 1;
}

int telemetry_is_stale(const VehicleState *state, PidIndex field, uint32_t now_ms) {
    if (!state->valid[field]) return 1;
    return (now_ms - state->last_update_ms[field]) > TELEMETRY_STALE_MS;
}

float telemetry_get(const VehicleState *state, PidIndex field) {
    const float *fp = field_ptr((VehicleState *)state, field);
    return fp ? *fp : 0.0f;
}
