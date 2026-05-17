#include <math.h>
#include "telemetry/derived.h"
#include "obd/pid.h"

/* Typical Honda FWD gear ratios (speed_kmh / rpm) */
static const float GEAR_RATIOS[7] = {
    0.0f,      /* unused index 0 */
    0.0052f,   /* 1st */
    0.0094f,   /* 2nd */
    0.0142f,   /* 3rd */
    0.0195f,   /* 4th */
    0.0267f,   /* 5th */
    0.0337f,   /* 6th */
};

static int estimate_gear(float speed_kmh, float rpm) {
    if (speed_kmh < 3.0f || rpm < 400.0f) return 0;
    float ratio = speed_kmh / rpm;
    int   best  = -1;
    float best_err = 1.0e9f;
    for (int g = 1; g <= 6; g++) {
        float err = fabsf(ratio - GEAR_RATIOS[g]) / GEAR_RATIOS[g];
        if (err < best_err) { best_err = err; best = g; }
    }
    return (best_err < 0.30f) ? best : -1;
}

void derived_reset_trip(DerivedState *d) {
    d->trip_fuel_l  = 0.0f;
    d->trip_dist_km = 0.0f;
    d->trip_l100km  = DERIVED_NO_DATA;
    d->range_km     = DERIVED_NO_DATA;
}

void derived_compute(DerivedState *d, const VehicleState *vs, uint32_t dt_ms) {
    static float prev_speed = 0.0f;
    static int   first      = 1;

    float dt_h = (float)dt_ms / 3600000.0f;
    float dt_s = (float)dt_ms / 1000.0f;

    /* Gear */
    d->gear = estimate_gear(vs->speed_kmh, vs->rpm);

    /* Fuel economy -- native PID 5E preferred, MAF-derived fallback */
    float effective_lh = DERIVED_NO_DATA;
    if (vs->supported[PID_FUEL_RATE] == 1) {
        effective_lh = vs->fuel_rate_lh;
    } else if (vs->supported[PID_MAF] == 1 && vs->maf_gs >= 0.0f) {
        /* MAF (g/s) -> L/h: divide by stoich AFR (14.7) and fuel density (0.737 kg/L), scale to L/h */
        effective_lh = vs->maf_gs * (3.6f / (14.7f * 0.737f));
    }
    int fuel_ok = (effective_lh != DERIVED_NO_DATA);
    d->instant_lh = effective_lh;

    if (fuel_ok && vs->speed_kmh >= 5.0f) {
        float l100 = (effective_lh / vs->speed_kmh) * 100.0f;
        d->instant_l100km = (l100 > 99.9f) ? 99.9f : l100;
    } else {
        d->instant_l100km = DERIVED_NO_DATA;
    }

    /* Trip accumulation */
    if (fuel_ok && dt_h > 0.0f)
        d->trip_fuel_l += effective_lh * dt_h;
    d->trip_dist_km += vs->speed_kmh * dt_h;

    if (d->trip_dist_km > 0.05f && fuel_ok && d->trip_fuel_l > 0.0f)
        d->trip_l100km = (d->trip_fuel_l / d->trip_dist_km) * 100.0f;

    /* Estimated range */
    if (vs->supported[PID_FUEL_LEVEL] == 1 && d->trip_l100km > 0.5f) {
        float rem_l = (vs->fuel_level_pct / 100.0f) * DERIVED_TANK_L;
        d->range_km = (rem_l / d->trip_l100km) * 100.0f;
    } else {
        d->range_km = DERIVED_NO_DATA;
    }

    /* G-force (longitudinal) */
    if (first || dt_ms > 2000) {
        prev_speed  = vs->speed_kmh;
        d->accel_g  = 0.0f;
        first       = 0;
    } else if (dt_s > 0.001f) {
        float delta_mps = (vs->speed_kmh - prev_speed) / 3.6f;
        float g = delta_mps / dt_s / 9.81f;
        if (g >  2.0f) g =  2.0f;
        if (g < -2.0f) g = -2.0f;
        d->accel_g = g;
    }
    prev_speed = vs->speed_kmh;

    /* Relative power: engine_load * (rpm fraction of redline) */
    float pwr = vs->engine_load_pct * (vs->rpm / 8000.0f);
    d->power_pct = (pwr > 100.0f) ? 100.0f : pwr;

    /* Coasting / idle flags */
    d->coasting = (vs->throttle_pct < 2.0f &&
                   vs->speed_kmh > 10.0f   &&
                   vs->rpm > 800.0f);
    d->idle     = (vs->rpm > 400.0f && vs->rpm < 1100.0f && vs->speed_kmh < 3.0f);

    /* AFR state from upstream O2 sensor voltage */
    if (vs->supported[PID_O2_B1S1] != 1) {
        d->afr_state = -1;
    } else if (vs->o2_b1s1_v < 0.35f) {
        d->afr_state = 0; /* lean */
    } else if (vs->o2_b1s1_v > 0.55f) {
        d->afr_state = 2; /* rich */
    } else {
        d->afr_state = 1; /* stoich */
    }
}
