#pragma once
#include <stdint.h>
#include "pid.h"

typedef struct {
    int      valid;
    PidIndex pid_idx;
    float    value;
} ParseResult;

ParseResult obd_parse_response(PidIndex pid_idx, const char *resp);
int         obd_parse_bytes(const char *resp, uint8_t *bytes, int max_bytes);
int         obd_hex_byte(const char *s, uint8_t *out);
