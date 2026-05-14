#pragma once
#include "../telemetry/model.h"
#include "../telemetry/derived.h"
#include "../obd/diagnostics.h"

typedef enum {
    DASH_MODE_DIGITAL = 0,
    DASH_MODE_ANALOG,
    DASH_MODE_DIAGNOSTICS,
    DASH_MODE_PERFORMANCE,
    DASH_MODE_ENGINE,   /* fuel trims, timing advance, ECU internals */
    DASH_MODE_TRIP,     /* runtime, fuel level, ambient, session stats */
    DASH_MODE_SENSORS,  /* MAP/MAF, O2, oil temp, fuel rate, all sensors */
    DASH_MODE_ECONOMY,  /* gear, L/100km, range, G-force, AFR */
    DASH_MODE_JDM,      /* NFS/JDM style: big tach+speedo, RPM strip, gear */
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
void dashboard_render(const VehicleState *vs, const DtcList *dtc,
                      const DerivedState *d, DashMode mode);

void dashboard_render_digital(const VehicleState *vs, const DerivedState *d);
void dashboard_render_analog(const VehicleState *vs, const DerivedState *d);
void dashboard_render_diagnostics(const VehicleState *vs, const DtcList *dtc);
void dashboard_render_performance(const VehicleState *vs, PerfState *perf,
                                  const DerivedState *d);
void dashboard_render_engine(const VehicleState *vs);
void dashboard_render_trip(const VehicleState *vs, const DerivedState *d);
void dashboard_render_sensors(const VehicleState *vs, const DerivedState *d);
void dashboard_render_economy(const VehicleState *vs, const DerivedState *d);
void dashboard_render_jdm(const VehicleState *vs, const DerivedState *d);
void dashboard_trip_reset_session(void);
void dashboard_render_status_bar(const VehicleState *vs, DashMode mode, int connected);
