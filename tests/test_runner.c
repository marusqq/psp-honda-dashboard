/* Native unit test runner - no PSP required, compiles with standard gcc.
   Tests pure-logic modules: parser, pid decode, filter, units, reconnect. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

/* ------------------------------------------------------------------ */
/* Minimal test harness                                                */
/* ------------------------------------------------------------------ */

static int g_pass = 0;
static int g_fail = 0;

#define ASSERT(cond) do { \
    if (cond) { g_pass++; } \
    else { \
        fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
        g_fail++; \
    } \
} while (0)

#define ASSERT_FLOAT(a, b, eps) \
    ASSERT(fabsf((float)(a) - (float)(b)) < (float)(eps))

#define TEST(name) static void name(void)

/* ------------------------------------------------------------------ */
/* Stub types so we can include PSP-independent logic directly         */
/* ------------------------------------------------------------------ */

typedef unsigned char  uint8_t_stub;
typedef unsigned int   uint32_t_stub;

/* ------------------------------------------------------------------ */
/* Inline copies of pure-logic functions (no PSP deps)                */
/* ------------------------------------------------------------------ */

/* --- PID decode functions (from pid.c) --- */
static float pid_decode_rpm(uint8_t a, uint8_t b)  { return ((a * 256.0f) + b) / 4.0f; }
static float pid_decode_speed(uint8_t a, uint8_t b) { (void)b; return (float)a; }
static float pid_decode_temp(uint8_t a, uint8_t b)  { (void)b; return (float)a - 40.0f; }
static float pid_decode_pct(uint8_t a, uint8_t b)   { (void)b; return a * 100.0f / 255.0f; }
static float pid_decode_volt(uint8_t a, uint8_t b)  { return ((a * 256.0f) + b) / 1000.0f; }

/* --- Moving average filter (from filter.c) --- */
#define FILTER_WINDOW 4
typedef struct { float buf[FILTER_WINDOW]; int head, count; float sum; } MovingAvg;
static void filter_init(MovingAvg *f) { memset(f, 0, sizeof(*f)); }
static float filter_update(MovingAvg *f, float val) {
    if (f->count < FILTER_WINDOW) {
        f->buf[f->head] = val; f->sum += val; f->count++;
    } else {
        f->sum -= f->buf[f->head]; f->buf[f->head] = val; f->sum += val;
    }
    f->head = (f->head + 1) % FILTER_WINDOW;
    return f->sum / (float)f->count;
}

/* --- Unit conversions (from units.c) --- */
static float units_kmh_to_mph(float k) { return k * 0.621371f; }
static float units_c_to_f(float c)     { return c * 1.8f + 32.0f; }

/* --- hex byte parser (from parser.c) --- */
#include <ctype.h>
static int obd_hex_byte(const char *s, uint8_t *out) {
    if (!isxdigit((unsigned char)s[0]) || !isxdigit((unsigned char)s[1])) return -1;
    uint8_t hi = (uint8_t)(isdigit((unsigned char)s[0]) ? s[0]-'0' : toupper((unsigned char)s[0])-'A'+10);
    uint8_t lo = (uint8_t)(isdigit((unsigned char)s[1]) ? s[1]-'0' : toupper((unsigned char)s[1])-'A'+10);
    *out = (uint8_t)((hi << 4) | lo);
    return 0;
}
static int obd_parse_bytes(const char *resp, uint8_t *bytes, int max) {
    int count = 0;
    const char *p = resp;
    while (*p && count < max) {
        while (*p == ' ' || *p == '\r' || *p == '\n') p++;
        if (!*p) break;
        if (obd_hex_byte(p, &bytes[count]) == 0) { count++; p += 2; } else { p++; }
    }
    return count;
}

/* --- DTC formatter (from diagnostics.c) --- */
static void dtc_format_code(uint8_t b1, uint8_t b2, char *out) {
    static const char sys[] = "PCBU";
    char c = sys[(b1 >> 6) & 0x03];
    int  n = ((b1 & 0x3F) << 8) | b2;
    snprintf(out, 6, "%c%04X", c, n);
}

/* --- Reconnect state (from reconnect.c) --- */
#define RECONNECT_BASE_DELAY_MS 1000
#define RECONNECT_MAX_DELAY_MS  30000
typedef struct { int attempt; uint32_t delay_ms, next_try_ms; } ReconnectState;
static void reconnect_reset(ReconnectState *rs) {
    memset(rs, 0, sizeof(*rs)); rs->delay_ms = RECONNECT_BASE_DELAY_MS;
}
static int reconnect_should_try(const ReconnectState *rs, uint32_t now_ms) {
    return now_ms >= rs->next_try_ms;
}
static void reconnect_advance(ReconnectState *rs, uint32_t now_ms) {
    rs->attempt++;
    rs->delay_ms *= 2;
    if (rs->delay_ms > RECONNECT_MAX_DELAY_MS) rs->delay_ms = RECONNECT_MAX_DELAY_MS;
    rs->next_try_ms = now_ms + rs->delay_ms;
}
static void reconnect_on_success(ReconnectState *rs) {
    rs->attempt = 0; rs->delay_ms = RECONNECT_BASE_DELAY_MS;
}

/* ================================================================== */
/* Test cases                                                          */
/* ================================================================== */

TEST(test_pid_rpm) {
    /* 0x1A 0xF8 = 6904 decimal -> RPM = 6904/4 = 1726 */
    ASSERT_FLOAT(pid_decode_rpm(0x1A, 0xF8), 1726.0f, 0.01f);
    /* 0x00 0x00 = 0 RPM */
    ASSERT_FLOAT(pid_decode_rpm(0x00, 0x00), 0.0f, 0.01f);
    /* 0xFF 0xFF = 65535/4 = 16383.75 RPM (max) */
    ASSERT_FLOAT(pid_decode_rpm(0xFF, 0xFF), 16383.75f, 0.01f);
    /* Typical idle: ~800 RPM = 3200 raw = 0x0C 0x80 */
    ASSERT_FLOAT(pid_decode_rpm(0x0C, 0x80), 800.0f, 0.01f);
}

TEST(test_pid_speed) {
    ASSERT_FLOAT(pid_decode_speed(0x64, 0), 100.0f, 0.01f);  /* 100 km/h */
    ASSERT_FLOAT(pid_decode_speed(0x00, 0), 0.0f, 0.01f);
    ASSERT_FLOAT(pid_decode_speed(0xFF, 0), 255.0f, 0.01f);
}

TEST(test_pid_temp) {
    /* Coolant at 90C: raw = 90+40 = 130 = 0x82 */
    ASSERT_FLOAT(pid_decode_temp(130, 0), 90.0f, 0.01f);
    /* -40C: raw = 0 */
    ASSERT_FLOAT(pid_decode_temp(0, 0), -40.0f, 0.01f);
    /* 0C: raw = 40 */
    ASSERT_FLOAT(pid_decode_temp(40, 0), 0.0f, 0.01f);
}

TEST(test_pid_throttle_pct) {
    /* 100%: raw = 0xFF */
    ASSERT_FLOAT(pid_decode_pct(0xFF, 0), 100.0f, 0.1f);
    /* 0%: raw = 0 */
    ASSERT_FLOAT(pid_decode_pct(0x00, 0), 0.0f, 0.01f);
    /* 50%: raw = 0x7F or 0x80 */
    float pct50 = pid_decode_pct(0x80, 0);
    ASSERT(pct50 > 49.0f && pct50 < 51.0f);
}

TEST(test_pid_voltage) {
    /* 14.4V: raw = 14400 = 0x3840 -> A=0x38=56, B=0x40=64 */
    ASSERT_FLOAT(pid_decode_volt(0x38, 0x40), 14.4f, 0.001f);
    /* 12.0V: 12000 = 0x2EE0 -> A=0x2E=46, B=0xE0=224 */
    ASSERT_FLOAT(pid_decode_volt(0x2E, 0xE0), 12.0f, 0.001f);
    /* 0V */
    ASSERT_FLOAT(pid_decode_volt(0x00, 0x00), 0.0f, 0.001f);
}

TEST(test_moving_avg_filter) {
    MovingAvg f;
    filter_init(&f);

    /* Single value */
    float v = filter_update(&f, 100.0f);
    ASSERT_FLOAT(v, 100.0f, 0.01f);

    /* Average of two */
    v = filter_update(&f, 200.0f);
    ASSERT_FLOAT(v, 150.0f, 0.01f);

    /* Average of three */
    v = filter_update(&f, 300.0f);
    ASSERT_FLOAT(v, 200.0f, 0.01f);

    /* Average of four (window full) */
    v = filter_update(&f, 400.0f);
    ASSERT_FLOAT(v, 250.0f, 0.01f);

    /* Window slides: drops 100, adds 500 -> (200+300+400+500)/4 = 350 */
    v = filter_update(&f, 500.0f);
    ASSERT_FLOAT(v, 350.0f, 0.01f);

    /* Constant input converges to that value */
    filter_init(&f);
    for (int i = 0; i < 20; i++) filter_update(&f, 42.0f);
    ASSERT_FLOAT(filter_update(&f, 42.0f), 42.0f, 0.001f);
}

TEST(test_unit_conversions) {
    ASSERT_FLOAT(units_kmh_to_mph(100.0f), 62.1371f, 0.001f);
    ASSERT_FLOAT(units_kmh_to_mph(0.0f),   0.0f,     0.001f);
    ASSERT_FLOAT(units_c_to_f(0.0f),       32.0f,    0.001f);
    ASSERT_FLOAT(units_c_to_f(100.0f),     212.0f,   0.001f);
    ASSERT_FLOAT(units_c_to_f(-40.0f),    -40.0f,    0.001f); /* -40 C = -40 F */
}

TEST(test_obd_hex_byte) {
    uint8_t b;
    ASSERT(obd_hex_byte("FF", &b) == 0 && b == 0xFF);
    ASSERT(obd_hex_byte("00", &b) == 0 && b == 0x00);
    ASSERT(obd_hex_byte("1A", &b) == 0 && b == 0x1A);
    ASSERT(obd_hex_byte("f8", &b) == 0 && b == 0xF8);
    ASSERT(obd_hex_byte("ZZ", &b) != 0);  /* invalid */
    ASSERT(obd_hex_byte("GG", &b) != 0);  /* invalid */
}

TEST(test_obd_parse_bytes) {
    uint8_t buf[8];
    /* Typical RPM response: "41 0C 1A F8" */
    int n = obd_parse_bytes("41 0C 1A F8", buf, 8);
    ASSERT(n == 4);
    ASSERT(buf[0] == 0x41);
    ASSERT(buf[1] == 0x0C);
    ASSERT(buf[2] == 0x1A);
    ASSERT(buf[3] == 0xF8);

    /* Response with no spaces */
    n = obd_parse_bytes("410C1AF8", buf, 8);
    ASSERT(n == 4);
    ASSERT(buf[0] == 0x41 && buf[3] == 0xF8);

    /* Garbage in response */
    n = obd_parse_bytes("41 0C NO DATA", buf, 8);
    ASSERT(n >= 2);
    ASSERT(buf[0] == 0x41);

    /* Empty */
    n = obd_parse_bytes("", buf, 8);
    ASSERT(n == 0);

    /* Overflow protection */
    n = obd_parse_bytes("41 0C 1A F8 00 11 22 33 44 55", buf, 4);
    ASSERT(n == 4);
}

TEST(test_dtc_format) {
    char code[6];
    /* P0420: b1=0x04, b2=0x20 */
    dtc_format_code(0x04, 0x20, code);
    ASSERT(strcmp(code, "P0420") == 0);

    /* C0000: b1=0x40, b2=0x00 */
    dtc_format_code(0x40, 0x00, code);
    ASSERT(strcmp(code, "C0000") == 0);

    /* B0001: b1=0x80, b2=0x01 */
    dtc_format_code(0x80, 0x01, code);
    ASSERT(strcmp(code, "B0001") == 0);

    /* U0100: b1=0xC0+1=0xC1, b2=0x00 */
    dtc_format_code(0xC1, 0x00, code);
    ASSERT(strcmp(code, "U0100") == 0);
}

TEST(test_reconnect_backoff) {
    ReconnectState rs;
    reconnect_reset(&rs);

    ASSERT(rs.attempt == 0);
    ASSERT(rs.delay_ms == 1000);
    ASSERT(reconnect_should_try(&rs, 0));     /* first try at t=0 */

    reconnect_advance(&rs, 0);
    ASSERT(rs.attempt == 1);
    ASSERT(rs.delay_ms == 2000);
    ASSERT(rs.next_try_ms == 2000);
    ASSERT(!reconnect_should_try(&rs, 1000)); /* too soon */
    ASSERT(reconnect_should_try(&rs, 2000));  /* at threshold */

    reconnect_advance(&rs, 2000);
    ASSERT(rs.delay_ms == 4000);

    reconnect_advance(&rs, 6000);
    ASSERT(rs.delay_ms == 8000);

    /* Cap at 30000ms */
    for (int i = 0; i < 10; i++) reconnect_advance(&rs, 0);
    ASSERT(rs.delay_ms == 30000);

    /* Success resets */
    reconnect_on_success(&rs);
    ASSERT(rs.attempt == 0);
    ASSERT(rs.delay_ms == 1000);
}

TEST(test_elm327_response_parse_roundtrip) {
    /* Verify that hex parse -> decode -> value is correct for each PID */
    uint8_t bytes[8];

    /* RPM at 3000: 3000*4 = 12000 = 0x2EE0, A=0x2E, B=0xE0 */
    obd_parse_bytes("41 0C 2E E0", bytes, 8);
    ASSERT_FLOAT(pid_decode_rpm(bytes[2], bytes[3]), 3000.0f, 0.01f);

    /* Speed 80 km/h */
    obd_parse_bytes("41 0D 50", bytes, 8);
    ASSERT_FLOAT(pid_decode_speed(bytes[2], 0), 80.0f, 0.01f);

    /* Coolant 85C: raw = 85+40=125=0x7D */
    obd_parse_bytes("41 05 7D", bytes, 8);
    ASSERT_FLOAT(pid_decode_temp(bytes[2], 0), 85.0f, 0.01f);
}

/* ================================================================== */
/* Main                                                                */
/* ================================================================== */

int main(void) {
    printf("Running PSP OBD2 Dashboard unit tests...\n\n");

    test_pid_rpm();
    test_pid_speed();
    test_pid_temp();
    test_pid_throttle_pct();
    test_pid_voltage();
    test_moving_avg_filter();
    test_unit_conversions();
    test_obd_hex_byte();
    test_obd_parse_bytes();
    test_dtc_format();
    test_reconnect_backoff();
    test_elm327_response_parse_roundtrip();

    printf("\nResults: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
