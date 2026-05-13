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
    switch (field) {
        case PID_RPM:          return state->rpm;
        case PID_SPEED:        return state->speed_kmh;
        case PID_COOLANT_TEMP: return state->coolant_temp_c;
        case PID_THROTTLE:     return state->throttle_pct;
        case PID_IAT:          return state->iat_c;
        case PID_ENGINE_LOAD:  return state->engine_load_pct;
        case PID_VOLTAGE:      return state->voltage_v;
        case PID_STFT:         return state->stft_pct;
        case PID_LTFT:         return state->ltft_pct;
        case PID_TIMING_ADV:   return state->timing_adv_deg;
        case PID_RUNTIME:      return state->runtime_s;
        case PID_FUEL_LEVEL:   return state->fuel_level_pct;
        case PID_AMBIENT_TEMP: return state->ambient_temp_c;
        default:               return 0.0f;
    }
}
