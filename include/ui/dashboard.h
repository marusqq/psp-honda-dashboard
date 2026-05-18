#pragma once
#include "../telemetry/model.h"
#include "../telemetry/derived.h"

typedef enum {
    DASH_MODE_DIGITAL = 0,
    DASH_MODE_PERFORMANCE,
    DASH_MODE_ENGINE,   /* fuel trims, timing advance, ECU internals */
    DASH_MODE_ECOTRIP,  /* combined: instant L/100km, trip stats, AFR, G-force */
    DASH_MODE_JDM,      /* NFS/JDM style: big tach+speedo, RPM strip, gear */
    DASH_MODE_TOUGE,    /* touge: power band strip, huge gear, shift lights */
    DASH_MODE_VTEC,     /* VTEC: animated badge, RPM/throttle/load bars */
    DASH_MODE_ARCADE,   /* pixel-art sedan, RoR1 style, scrolling road */
    DASH_MODE_COUNT
} DashMode;

typedef struct {
    float    accel_start_speed;
    uint32_t accel_start_ms;
    float    accel_end_speed;
    uint32_t accel_end_ms;
    float    peak_rpm;
    int      timing_active;
    float    best_0_100;
} PerfState;

void dashboard_init(void);
void dashboard_render(const VehicleState *vs, const DerivedState *d, DashMode mode);

void dashboard_render_digital(const VehicleState *vs, const DerivedState *d);
void dashboard_render_performance(const VehicleState *vs, PerfState *perf,
                                  const DerivedState *d);
void dashboard_render_engine(const VehicleState *vs);
void dashboard_render_ecotrip(const VehicleState *vs, const DerivedState *d);
void dashboard_render_jdm(const VehicleState *vs, const DerivedState *d);
void dashboard_render_touge(const VehicleState *vs, const DerivedState *d);
void dashboard_render_vtec(const VehicleState *vs, const DerivedState *d);
void dashboard_render_arcade(const VehicleState *vs, const DerivedState *d);
void dashboard_trip_reset_session(void);
void dashboard_render_status_bar(const VehicleState *vs, DashMode mode, int connected);
