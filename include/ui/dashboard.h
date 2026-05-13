#pragma once
#include "../telemetry/model.h"
#include "../obd/diagnostics.h"

typedef enum {
    DASH_MODE_DIGITAL = 0,
    DASH_MODE_ANALOG,
    DASH_MODE_DIAGNOSTICS,
    DASH_MODE_PERFORMANCE,
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
void dashboard_render(const VehicleState *vs, const DtcList *dtc, DashMode mode);

void dashboard_render_digital(const VehicleState *vs);
void dashboard_render_analog(const VehicleState *vs);
void dashboard_render_diagnostics(const VehicleState *vs, const DtcList *dtc);
void dashboard_render_performance(const VehicleState *vs, PerfState *perf);
void dashboard_render_status_bar(const VehicleState *vs, DashMode mode, int connected);
