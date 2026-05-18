#include <string.h>
#include <ctype.h>
#include "obd/parser.h"
#include "obd/pid.h"

int obd_hex_byte(const char *s, uint8_t *out) {
    uint8_t hi, lo;
    if (!isxdigit((unsigned char)s[0]) || !isxdigit((unsigned char)s[1]))
        return -1;
    hi = (uint8_t)(isdigit((unsigned char)s[0]) ? s[0] - '0' : toupper((unsigned char)s[0]) - 'A' + 10);
    lo = (uint8_t)(isdigit((unsigned char)s[1]) ? s[1] - '0' : toupper((unsigned char)s[1]) - 'A' + 10);
    *out = (uint8_t)((hi << 4) | lo);
    return 0;
}

int obd_parse_bytes(const char *resp, uint8_t *bytes, int max_bytes) {
    int count = 0;
    const char *p = resp;
    while (*p && count < max_bytes) {
        while (*p == ' ' || *p == '\r' || *p == '\n')
            p++;
        if (!*p)
            break;
        if (obd_hex_byte(p, &bytes[count]) == 0) {
            count++;
            p += 2;
        } else {
            p++;
        }
    }
    return count;
}

ParseResult obd_parse_response(PidIndex pid_idx, const char *resp) {
    ParseResult result = {0, pid_idx, 0.0f};

    if (!resp || resp[0] == '\0')
        return result;

    /* Skip "NO DATA", "ERROR", "?" responses */
    if (strstr(resp, "NO DATA") || strstr(resp, "ERROR") || resp[0] == '?')
        return result;

    /* AT commands (e.g. ATRV) return human-readable text, not OBD2 hex frames.
       Parse the first float value from the response string. */
    if (PID_TABLE[pid_idx].cmd[0] == 'A' && PID_TABLE[pid_idx].cmd[1] == 'T') {
        float val = 0.0f;
        const char *p = resp;
        /* skip non-numeric prefix */
        while (*p && *p != '-' && !(*p >= '0' && *p <= '9')) p++;
        if (*p) {
            float sign = 1.0f;
            if (*p == '-') { sign = -1.0f; p++; }
            while (*p >= '0' && *p <= '9') { val = val * 10.0f + (*p++ - '0'); }
            if (*p == '.') {
                float f = 0.1f; p++;
                while (*p >= '0' && *p <= '9') { val += (*p++ - '0') * f; f *= 0.1f; }
            }
            result.value = val * sign;
            result.valid = 1;
        }
        return result;
    }

    uint8_t bytes[8];
    int n = obd_parse_bytes(resp, bytes, 8);

    /* Minimum: mode byte (41) + PID byte + at least 1 data byte */
    if (n < 3)
        return result;

    /* bytes[0] = 0x41 (positive response to mode 01)
       bytes[1] = PID code
       bytes[2] = A, bytes[3] = B (if present) */
    if (bytes[0] != 0x41)
        return result;

    /* Verify the response PID matches what was requested -- ISO 9141-2 can
       deliver a late response to a previous query; discard mismatches. */
    uint8_t expected_pid = 0;
    obd_hex_byte(&PID_TABLE[pid_idx].cmd[2], &expected_pid);
    if (bytes[1] != expected_pid)
        return result;

    uint8_t a = bytes[2];
    uint8_t b = (n >= 4) ? bytes[3] : 0;

    result.value = PID_TABLE[pid_idx].decode(a, b);
    result.valid = 1;
    return result;
}
