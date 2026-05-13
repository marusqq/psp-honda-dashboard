#include <string.h>
#include <ctype.h>
#include "obd/parser.h"

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

    uint8_t a = bytes[2];
    uint8_t b = (n >= 4) ? bytes[3] : 0;

    result.value = PID_TABLE[pid_idx].decode(a, b);
    result.valid = 1;
    return result;
}
