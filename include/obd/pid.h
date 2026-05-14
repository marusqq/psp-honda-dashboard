#pragma once
#include <stdint.h>

typedef enum {
    /* Core - always polled */
    PID_RPM = 0,
    PID_SPEED,
    PID_COOLANT_TEMP,
    PID_THROTTLE,
    PID_IAT,
    PID_ENGINE_LOAD,
    PID_VOLTAGE,
    /* Extended set 1 - ECU internals */
    PID_STFT,          /* short-term fuel trim bank 1, %           */
    PID_LTFT,          /* long-term  fuel trim bank 1, %           */
    PID_TIMING_ADV,    /* ignition timing advance, deg BTDC        */
    PID_RUNTIME,       /* engine run time since start, seconds     */
    PID_FUEL_LEVEL,    /* fuel tank level, %                       */
    PID_AMBIENT_TEMP,  /* ambient air temperature, °C              */
    /* Extended set 2 - sensors */
    PID_MAP,           /* manifold absolute pressure, kPa          */
    PID_MAF,           /* mass air flow, g/s                       */
    PID_O2_B1S1,       /* O2 sensor bank1 sensor1 (upstream), V   */
    PID_O2_B1S2,       /* O2 sensor bank1 sensor2 (downstream), V */
    PID_BARO,          /* barometric pressure, kPa                 */
    PID_REL_THROTTLE,  /* relative throttle position, %            */
    PID_ACCEL_POS,     /* accelerator pedal position D, %          */
    PID_OIL_TEMP,      /* engine oil temperature, °C               */
    PID_FUEL_RATE,     /* fuel consumption rate, L/h               */
    PID_ETHANOL,       /* ethanol fuel percentage, %               */
    /* Extended set 3 - diagnostic counters */
    PID_MIL_TIME,      /* minutes MIL has been on                  */
    PID_CLR_TIME,      /* minutes since DTCs were cleared          */
    PID_MIL_DIST,      /* km driven with MIL on                    */
    PID_CLR_DIST,      /* km driven since DTCs cleared             */
    PID_COUNT
} PidIndex;

typedef struct {
    const char *cmd;
    const char *name;
    const char *unit;
    float (*decode)(uint8_t a, uint8_t b);
    float min_val;
    float max_val;
    int   priority;   /* 0=highest (2x), 1=normal, 2=slow, 3=very slow (3x interval) */
} PidDef;

typedef struct {
    int      current;
    uint32_t last_poll_ms[PID_COUNT];
    int      poll_interval_ms;
    uint8_t  fail_count[PID_COUNT]; /* consecutive NO DATA responses   */
    uint8_t  skip[PID_COUNT];       /* 1 = unsupported, skip polling   */
    uint32_t active_mask;           /* bitmask of PIDs to poll; 0=all  */
} PidScheduler;

extern const PidDef PID_TABLE[PID_COUNT];

void     pid_scheduler_init(PidScheduler *sched, int poll_interval_ms);
PidIndex pid_scheduler_next(PidScheduler *sched, uint32_t now_ms);
