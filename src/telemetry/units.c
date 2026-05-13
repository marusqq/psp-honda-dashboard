#include "telemetry/units.h"

float units_kmh_to_mph(float kmh) { return kmh * 0.621371f; }
float units_c_to_f(float c)       { return c * 1.8f + 32.0f; }
float units_rpm_to_krpm(float rpm) { return rpm / 1000.0f; }
float units_v_display(float v)     { return v; }
