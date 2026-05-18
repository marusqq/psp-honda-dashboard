#include <string.h>
#include "obd/pid.h"
#include "utils/time.h"

/* Decode helpers */
static float decode_rpm(uint8_t a, uint8_t b)       { return ((a * 256.0f) + b) / 4.0f; }
static float decode_speed(uint8_t a, uint8_t b)      { (void)b; return (float)a; }
static float decode_temp(uint8_t a, uint8_t b)       { (void)b; return (float)a - 40.0f; }
static float decode_pct(uint8_t a, uint8_t b)        { (void)b; return a * 100.0f / 255.0f; }
static float decode_volt(uint8_t a, uint8_t b)       { return ((a * 256.0f) + b) / 1000.0f; }
static float decode_fuel_trim(uint8_t a, uint8_t b)  { (void)b; return (a / 128.0f - 1.0f) * 100.0f; }
static float decode_timing_adv(uint8_t a, uint8_t b) { (void)b; return (float)a / 2.0f - 64.0f; }
static float decode_runtime(uint8_t a, uint8_t b)    { return (float)(a * 256 + b); }
static float decode_kpa(uint8_t a, uint8_t b)        { (void)b; return (float)a; }
static float decode_maf(uint8_t a, uint8_t b)        { return (float)(a * 256 + b) / 100.0f; }
static float decode_o2v(uint8_t a, uint8_t b)        { (void)b; return (float)a / 200.0f; }
static float decode_fuel_rate(uint8_t a, uint8_t b)  { return (float)(a * 256 + b) / 20.0f; }
static float decode_u16(uint8_t a, uint8_t b)        { return (float)(a * 256 + b); }

const PidDef PID_TABLE[PID_COUNT] = {
    /* Core */
    [PID_RPM]          = {"010C", "RPM",              "rpm", decode_rpm,        0,    8000, 0},
    [PID_SPEED]        = {"010D", "Speed",             "kmh", decode_speed,      0,     240, 1},
    [PID_COOLANT_TEMP] = {"0105", "Coolant Temp",      "C",   decode_temp,      -40,   215, 2},
    [PID_THROTTLE]     = {"0111", "Throttle",          "%",   decode_pct,        0,     100, 1},
    [PID_IAT]          = {"010F", "Intake Air Temp",   "C",   decode_temp,      -40,   215, 3},
    [PID_ENGINE_LOAD]  = {"0104", "Engine Load",       "%",   decode_pct,        0,     100, 2},
    [PID_VOLTAGE]      = {"ATRV", "Voltage",           "V",   decode_volt,       0,      20, 3},
    /* ECU internals */
    [PID_STFT]         = {"0106", "ST Fuel Trim",      "%",   decode_fuel_trim,-100,   100, 1},
    [PID_LTFT]         = {"0107", "LT Fuel Trim",      "%",   decode_fuel_trim,-100,   100, 3},
    [PID_TIMING_ADV]   = {"010E", "Timing Advance",    "deg", decode_timing_adv,-64,    64, 2},
    [PID_RUNTIME]      = {"011F", "Engine Runtime",    "s",   decode_runtime,    0,  65535, 3},
    [PID_FUEL_LEVEL]   = {"012F", "Fuel Level",        "%",   decode_pct,        0,     100, 3},
    [PID_AMBIENT_TEMP] = {"0146", "Ambient Temp",      "C",   decode_temp,      -40,   215, 3},
    /* Sensors */
    [PID_MAP]          = {"010B", "MAP Pressure",      "kPa", decode_kpa,        0,     255, 1},
    [PID_MAF]          = {"0110", "MAF Air Flow",      "g/s", decode_maf,        0,     655, 1},
    [PID_O2_B1S1]      = {"0114", "O2 B1S1",           "V",   decode_o2v,        0,    1.28, 1},
    [PID_O2_B1S2]      = {"0115", "O2 B1S2",           "V",   decode_o2v,        0,    1.28, 2},
    [PID_BARO]         = {"0133", "Barometric",        "kPa", decode_kpa,        0,     255, 3},
    [PID_REL_THROTTLE] = {"0145", "Rel Throttle",      "%",   decode_pct,        0,     100, 2},
    [PID_ACCEL_POS]    = {"0149", "Accel Pedal",       "%",   decode_pct,        0,     100, 1},
    [PID_OIL_TEMP]     = {"015C", "Oil Temp",          "C",   decode_temp,      -40,   215, 3},
    [PID_FUEL_RATE]    = {"015E", "Fuel Rate",         "L/h", decode_fuel_rate,  0,    3276, 2},
    [PID_ETHANOL]      = {"0152", "Ethanol %",         "%",   decode_pct,        0,     100, 3},
    /* Diagnostic counters */
    [PID_MIL_TIME]     = {"014D", "MIL On Time",       "min", decode_u16,        0,   65535, 3},
    [PID_CLR_TIME]     = {"014E", "Since CLR Time",    "min", decode_u16,        0,   65535, 3},
    [PID_MIL_DIST]     = {"0121", "MIL Distance",      "km",  decode_u16,        0,   65535, 3},
    [PID_CLR_DIST]     = {"0131", "Since CLR Dist",    "km",  decode_u16,        0,   65535, 3},
};

void pid_scheduler_init(PidScheduler *sched, int poll_interval_ms) {
    memset(sched, 0, sizeof(*sched));
    sched->poll_interval_ms = poll_interval_ms;
}

PidIndex pid_scheduler_next(PidScheduler *sched, uint32_t now_ms) {
    for (int pass = 0; pass < PID_COUNT; pass++) {
        int idx = (sched->current + pass) % PID_COUNT;

        if (sched->skip[idx])
            continue;

        /* Skip PIDs not required by the active dashboard */
        if (sched->active_mask && !(sched->active_mask & (1u << idx)))
            continue;

        uint32_t interval = (uint32_t)sched->poll_interval_ms;
        if      (PID_TABLE[idx].priority == 0) interval /= 2;
        else if (PID_TABLE[idx].priority >= 3) interval *= 3;

        if ((now_ms - sched->last_poll_ms[idx]) >= interval) {
            sched->current = (idx + 1) % PID_COUNT;
            return (PidIndex)idx;
        }
    }
    /* Nothing due this frame */
    return PID_COUNT;
}
