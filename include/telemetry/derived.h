#pragma once
#include <stdint.h>
#include "model.h"

#define DERIVED_TANK_L   45.0f  /* assumed tank capacity in litres */
#define DERIVED_NO_DATA  -1.0f  /* sentinel: not computable */

typedef struct {
    /* Drivetrain */
    int   gear;           /* 1-6, 0=neutral/stopped, -1=unknown */
    /* Fuel economy */
    float instant_l100km; /* DERIVED_NO_DATA if speed < 5 km/h or no PID */
    float instant_lh;     /* raw L/h, DERIVED_NO_DATA if PID missing */
    float trip_l100km;    /* session average, DERIVED_NO_DATA until moving */
    float trip_fuel_l;    /* session fuel consumed (L) */
    float trip_dist_km;   /* session distance (km) */
    float range_km;       /* estimated remaining range, DERIVED_NO_DATA if insufficient */
    /* Dynamics */
    float accel_g;        /* longitudinal G: positive=accel, negative=brake */
    float power_pct;      /* relative power 0-100% (load * rpm fraction) */
    /* Status flags */
    int   coasting;       /* 1 = throttle~0 && speed>10 && rpm>800 */
    int   idle;           /* 1 = rpm<1100 && speed<3 */
    /* AFR interpretation from upstream O2 sensor */
    int   afr_state;      /* -1=unknown, 0=lean, 1=stoich, 2=rich */
} DerivedState;

void derived_compute(DerivedState *d, const VehicleState *vs, uint32_t dt_ms);
void derived_reset_trip(DerivedState *d);
