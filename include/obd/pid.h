#pragma once
#include <stdint.h>

typedef enum {
    PID_RPM = 0,
    PID_SPEED,
    PID_COOLANT_TEMP,
    PID_THROTTLE,
    PID_IAT,
    PID_ENGINE_LOAD,
    PID_VOLTAGE,
    PID_COUNT
} PidIndex;

typedef struct {
    const char *cmd;
    const char *name;
    const char *unit;
    float (*decode)(uint8_t a, uint8_t b);
    float min_val;
    float max_val;
    int   priority;
} PidDef;

typedef struct {
    int      current;
    uint32_t last_poll_ms[PID_COUNT];
    int      poll_interval_ms;
} PidScheduler;

extern const PidDef PID_TABLE[PID_COUNT];

void     pid_scheduler_init(PidScheduler *sched, int poll_interval_ms);
PidIndex pid_scheduler_next(PidScheduler *sched, uint32_t now_ms);
