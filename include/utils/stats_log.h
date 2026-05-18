#pragma once
#include "telemetry/model.h"
#include "telemetry/derived.h"

void stats_log_open(void);
void stats_log_sample(const VehicleState *vs, const DerivedState *d);
void stats_log_close(void);
