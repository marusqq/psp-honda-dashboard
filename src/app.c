#include <stdio.h>
#include <string.h>
#include <math.h>
#include <pspkernel.h>
#include <pspwlan.h>
#include "app.h"
#include "net/wifi.h"
#include "net/socket.h"
#include "net/reconnect.h"
#include "obd/elm327.h"
#include "obd/pid.h"
#include "obd/parser.h"
#include "obd/diagnostics.h"
#include "telemetry/model.h"
#include "telemetry/derived.h"
#include "telemetry/filter.h"
#include "ui/renderer.h"
#include "ui/dashboard.h"
#include "ui/themes.h"
#include "ui/setup.h"
#include "input/controls.h"
#include "config/settings.h"
#include "utils/log.h"
#include "utils/time.h"
#include "utils/memory.h"
#include "utils/font.h"

#define TARGET_FPS     30
#define FRAME_TIME_MS  (1000 / TARGET_FPS)

static volatile int  g_running        = 0;
static volatile int  g_obd_poll_busy  = 0;  /* 1 while OBD thread is inside a socket op */
static SceUID        g_obd_thread_id  = -1;
static AppState      g_state          = APP_STATE_INIT;
static Settings      g_settings;
static TcpSocket     g_sock;
static ReconnectState g_reconnect;
static VehicleState  g_vehicle;
static DtcList       g_dtc;
static PidScheduler  g_sched;
static MovingAvg     g_filters[PID_COUNT];
static DerivedState  g_derived;
static DashMode      g_dash_mode;
static InputState    g_input;
static int           g_in_setup      = 0;
static int           g_demo_mode     = 0;   /* 1 = show fake data              */
static int           g_main_menu_sel = 0;   /* 0=connect 1=demo 2=settings     */
static int           g_help_visible  = 0;   /* help overlay toggle             */
static char          g_obd_status[128] = {0};

/* ------------------------------------------------------------------ */

void app_request_exit(void) { g_running = 0; }
int  app_is_running(void)   { return g_running; }
AppState app_get_state(void)     { return g_state; }
void     app_set_state(AppState s) { g_state = s; }

/* ------------------------------------------------------------------ */
/* Setup / settings helpers                                            */
/* ------------------------------------------------------------------ */

static void enter_setup(int settings_mode) {
    g_in_setup = 1;
    /* Pause OBD polling if we came from running state */
    if (settings_mode && socket_is_connected(&g_sock))
        socket_close(&g_sock);
    setup_init(&g_settings, settings_mode);
}

static void leave_setup(SetupResult result) {
    g_in_setup = 0;
    setup_shutdown();

    if (result == SETUP_RESULT_DONE) {
        /* Apply saved settings and return to main menu to let user connect */
        theme_set(g_settings.theme);
        g_dash_mode = (DashMode)theme_current()->default_mode;
        pid_scheduler_init(&g_sched, g_settings.poll_interval_ms);
        reconnect_reset(&g_reconnect);
        memset(&g_dtc, 0, sizeof(g_dtc));
        g_demo_mode = 0;
        g_state     = APP_STATE_MAIN_MENU;
        LOG_I("Setup done, returning to main menu");
        return;
    }

    /* Skip or Cancelled -> back to main menu */
    g_demo_mode = 0;
    g_state     = APP_STATE_MAIN_MENU;
    LOG_I("Setup cancelled/skipped, returning to main menu");
}

/* Forcibly close all connections and return to the main menu.
   Safe to call from any app state, including while OBD is active. */
static void return_to_main_menu(void) {
    /* Signal OBD thread to stop, then wait for it to finish any in-progress
       socket op before we close the socket under it (max 600 ms). */
    g_state = APP_STATE_MAIN_MENU;
    for (int i = 0; g_obd_poll_busy && i < 600; i++)
        sceKernelDelayThread(1000);

    socket_close(&g_sock);
    wifi_shutdown();
    telemetry_init(&g_vehicle);
    memset(&g_dtc, 0, sizeof(g_dtc));
    for (int i = 0; i < PID_COUNT; i++)
        filter_init(&g_filters[i]);
    pid_scheduler_init(&g_sched, g_settings.poll_interval_ms);
    memset(&g_derived, 0, sizeof(g_derived));
    g_demo_mode    = 0;
    g_in_setup     = 0;
    g_help_visible = 0;
    g_obd_status[0] = '\0';
    LOG_I("Returned to main menu");
}

/* ------------------------------------------------------------------ */

static void poll_obd(void); /* defined below */

/* ------------------------------------------------------------------ */
/* OBD polling thread -- runs independently so the render loop never   */
/* blocks waiting for ISO 9141-2 bus responses (~300-400 ms each).     */
/* ------------------------------------------------------------------ */

static int obd_thread_func(SceSize args, void *argp) {
    (void)args; (void)argp;
    uint32_t dtc_next_try_ms = 0;

    while (g_running) {
        if (g_state != APP_STATE_RUNNING || g_demo_mode) {
            sceKernelDelayThread(5000); /* 5 ms -- idle when not in RUNNING */
            continue;
        }

        g_obd_poll_busy = 1;
        poll_obd();

        if (!socket_is_connected(&g_sock)) {
            LOG_W("OBD connection lost, will re-probe");
            reconnect_reset(&g_reconnect);
            g_obd_status[0] = '\0';
            g_state = APP_STATE_OBD_CONNECTING;
            g_obd_poll_busy = 0;
            continue;
        }

        if (!g_dtc.read_ok && time_now_ms() >= dtc_next_try_ms) {
            if (dtc_read(&g_sock, &g_dtc) != 0)
                dtc_next_try_ms = time_now_ms() + 5000;
        }

        g_obd_poll_busy = 0;
    }

    g_obd_poll_busy = 0;
    return 0;
}

/* ------------------------------------------------------------------ */

int app_init(void) {
    mem_init();

    LOG_I("WLAN switch: %d  devkit: 0x%08x",
          sceWlanGetSwitchState(), (unsigned)sceKernelDevkitVersion());

    settings_load(&g_settings);
    theme_set(g_settings.theme);
    g_dash_mode = g_settings.default_mode;

    for (int i = 0; i < PID_COUNT; i++)
        filter_init(&g_filters[i]);

    telemetry_init(&g_vehicle);
    memset(&g_dtc, 0, sizeof(g_dtc));
    pid_scheduler_init(&g_sched, g_settings.poll_interval_ms);

    if (renderer_init() != 0) {
        LOG_E("renderer_init failed");
        return -1;
    }

    dashboard_init();
    g_running = 1;

    g_obd_thread_id = sceKernelCreateThread("obd_poll", obd_thread_func,
                                             0x20, 0x2000, 0, NULL);
    if (g_obd_thread_id >= 0)
        sceKernelStartThread(g_obd_thread_id, 0, NULL);
    else
        LOG_W("OBD thread creation failed (id=%d), polling will block render", g_obd_thread_id);

    g_state = APP_STATE_MAIN_MENU;
    LOG_I("App init OK");
    return 0;
}

/* ------------------------------------------------------------------ */

static void handle_input(void) {
    input_update(&g_input);

    /* Panic combo: works from any state including setup and connecting screens */
    if (input_held(&g_input, BTN_L) && input_held(&g_input, BTN_R) &&
        input_pressed(&g_input, BTN_START)) {
        LOG_W("L+R+Start panic -> returning to main menu");
        if (g_in_setup) setup_shutdown();
        return_to_main_menu();
        return;
    }

    if (g_in_setup)
        return;

    /* Main menu: navigate and confirm */
    if (g_state == APP_STATE_MAIN_MENU) {
        if (input_pressed(&g_input, BTN_UP))
            g_main_menu_sel = (g_main_menu_sel + 2) % 3;
        if (input_pressed(&g_input, BTN_DOWN))
            g_main_menu_sel = (g_main_menu_sel + 1) % 3;
        if (input_pressed(&g_input, BTN_CROSS)) {
            switch (g_main_menu_sel) {
            case 0:
                if (g_settings.ap_config_idx == 0) {
                    enter_setup(0);
                    g_state = APP_STATE_SETUP;
                } else {
                    socket_init(&g_sock, g_settings.obd_ip, g_settings.obd_port);
                    reconnect_reset(&g_reconnect);
                    g_state = APP_STATE_WIFI_CONNECTING;
                }
                break;
            case 1:
                g_demo_mode = 1;
                g_state = APP_STATE_RUNNING;
                break;
            case 2:
                enter_setup(1);
                g_state = APP_STATE_SETUP;
                break;
            }
        }
        return;
    }

    /* Start: return to main menu from any non-menu state */
    if (input_pressed(&g_input, BTN_START)) {
        LOG_I("Start -> returning to main menu");
        return_to_main_menu();
        return;
    }

    /* Select: toggle help overlay */
    if (input_pressed(&g_input, BTN_SELECT)) {
        g_help_visible = !g_help_visible;
        return;
    }

    /* L/R: cycle dashboard mode */
    if (input_pressed(&g_input, BTN_L))
        g_dash_mode = (DashMode)((g_dash_mode + DASH_MODE_COUNT - 1) % DASH_MODE_COUNT);
    if (input_pressed(&g_input, BTN_R))
        g_dash_mode = (DashMode)((g_dash_mode + 1) % DASH_MODE_COUNT);

    /* Cross: cycle theme */
    if (input_pressed(&g_input, BTN_CROSS)) {
        ThemeID next = (ThemeID)((theme_get() + 1) % THEME_COUNT);
        theme_set(next);
        g_dash_mode = (DashMode)theme_current()->default_mode;
    }

    /* Triangle on TRIP/ECONOMY: reset trip */
    if ((g_dash_mode == DASH_MODE_TRIP || g_dash_mode == DASH_MODE_ECONOMY) &&
        input_pressed(&g_input, BTN_TRIANGLE)) {
        dashboard_trip_reset_session();
        derived_reset_trip(&g_derived);
    }
}

/* ------------------------------------------------------------------ */

/* Animate fake vehicle data so demo mode looks alive */
static void simulate_demo(void) {
    static uint32_t demo_start_ms = 0;
    static int demo_inited = 0;

    if (!demo_inited) {
        demo_inited    = 1;
        demo_start_ms  = time_now_ms();
        /* Mark all PIDs as supported so UI shows values not "..." */
        for (int i = 0; i < PID_COUNT; i++)
            g_vehicle.supported[i] = 1;
    }

    float t       = (float)time_now_ms() / 1000.0f;
    float elapsed = (float)(time_now_ms() - demo_start_ms) / 1000.0f;

    /* RPM: slow sine 800..6000, with occasional high-rev burst */
    float rpm_norm = (sinf(t * 0.6f) * 0.5f + 0.5f);
    float burst    = (sinf(t * 0.17f) > 0.7f) ? 0.35f : 0.0f;
    rpm_norm = rpm_norm * (1.0f - burst) + burst;
    if (rpm_norm > 1.0f) rpm_norm = 1.0f;

    /* Core */
    g_vehicle.rpm             = 800.0f  + rpm_norm * 5200.0f;
    g_vehicle.speed_kmh       = rpm_norm * rpm_norm * 130.0f;
    g_vehicle.throttle_pct    = rpm_norm * 72.0f;
    g_vehicle.engine_load_pct = 18.0f   + rpm_norm * 58.0f;
    g_vehicle.coolant_temp_c  = 87.0f   + sinf(t * 0.08f) * 3.5f;
    g_vehicle.iat_c           = 27.0f   + sinf(t * 0.04f) * 2.0f;
    g_vehicle.voltage_v       = 14.1f   + sinf(t * 0.25f) * 0.15f;

    /* ECU internals */
    g_vehicle.stft_pct       = sinf(t * 1.3f) * 4.5f;
    g_vehicle.ltft_pct       = sinf(t * 0.12f) * 2.5f;
    g_vehicle.timing_adv_deg = 12.0f + rpm_norm * 18.0f;
    g_vehicle.runtime_s      = elapsed;
    g_vehicle.fuel_level_pct = 73.0f - elapsed * 0.003f;
    g_vehicle.ambient_temp_c = 19.5f + sinf(t * 0.02f) * 1.5f;

    /* Sensors */
    g_vehicle.map_kpa         = 30.0f  + rpm_norm * 71.0f;       /* vacuum to boost proxy */
    g_vehicle.maf_gs          = 2.0f   + rpm_norm * 18.0f;
    g_vehicle.o2_b1s1_v       = 0.45f  + sinf(t * 2.1f) * 0.35f; /* switching lambda */
    g_vehicle.o2_b1s2_v       = 0.65f  + sinf(t * 0.3f) * 0.05f; /* post-cat stable */
    g_vehicle.baro_kpa        = 101.3f + sinf(t * 0.005f) * 0.3f;
    g_vehicle.rel_throttle_pct = rpm_norm * 68.0f;
    g_vehicle.accel_pos_pct   = rpm_norm * 55.0f;
    g_vehicle.oil_temp_c      = 85.0f  + sinf(t * 0.05f) * 5.0f;
    g_vehicle.fuel_rate_lh    = 0.8f   + rpm_norm * 5.5f;
    g_vehicle.ethanol_pct     = 10.0f;                           /* E10 pump fuel */

    /* Diagnostic counters (static-ish, grow slowly) */
    g_vehicle.mil_time_min    = 0.0f;
    g_vehicle.clr_time_min    = 1440.0f + elapsed / 60.0f;
    g_vehicle.mil_dist_km     = 0.0f;
    g_vehicle.clr_dist_km     = 280.0f + g_vehicle.speed_kmh * elapsed / 3600.0f;
}

/* PIDs needed per dashboard mode. Always OR'd with CORE (RPM|SPEED|FUEL_RATE)
   so derived trip/accel data accumulates even on non-core screens. */
static uint32_t get_mode_pid_mask(DashMode mode) {
#define M(p) (1u << PID_##p)
    static const uint32_t core = M(RPM) | M(SPEED) | M(FUEL_RATE);
    uint32_t mask;
    switch (mode) {
    case DASH_MODE_DIGITAL:
        mask = M(RPM)|M(SPEED)|M(COOLANT_TEMP)|M(THROTTLE)|M(IAT)|
               M(ENGINE_LOAD)|M(VOLTAGE)|M(OIL_TEMP)|M(FUEL_LEVEL);
        break;
    case DASH_MODE_ANALOG:
        mask = M(RPM)|M(SPEED)|M(COOLANT_TEMP)|M(ENGINE_LOAD)|
               M(VOLTAGE)|M(OIL_TEMP);
        break;
    case DASH_MODE_DIAGNOSTICS:
        mask = M(RPM)|M(SPEED)|M(MIL_TIME)|M(CLR_TIME)|
               M(MIL_DIST)|M(CLR_DIST);
        break;
    case DASH_MODE_PERFORMANCE:
        mask = M(RPM)|M(SPEED)|M(THROTTLE)|M(ENGINE_LOAD);
        break;
    case DASH_MODE_ENGINE:
        mask = M(RPM)|M(ENGINE_LOAD)|M(STFT)|M(LTFT)|M(TIMING_ADV)|
               M(IAT)|M(COOLANT_TEMP)|M(OIL_TEMP)|M(VOLTAGE)|
               M(MAP)|M(MAF)|M(BARO)|M(FUEL_LEVEL)|M(AMBIENT_TEMP)|M(RUNTIME);
        break;
    case DASH_MODE_TRIP:
        mask = M(RPM)|M(SPEED)|M(RUNTIME)|M(FUEL_LEVEL)|M(FUEL_RATE)|
               M(AMBIENT_TEMP)|M(IAT)|M(COOLANT_TEMP)|M(OIL_TEMP)|
               M(VOLTAGE)|M(ETHANOL);
        break;
    case DASH_MODE_SENSORS:
        mask = M(MAP)|M(MAF)|M(BARO)|M(FUEL_RATE)|M(O2_B1S1)|M(O2_B1S2)|
               M(OIL_TEMP)|M(ETHANOL)|M(ACCEL_POS)|M(REL_THROTTLE)|
               M(MIL_TIME)|M(CLR_TIME)|M(MIL_DIST)|M(CLR_DIST);
        break;
    case DASH_MODE_ECONOMY:
        mask = M(RPM)|M(SPEED)|M(THROTTLE)|M(ENGINE_LOAD)|
               M(FUEL_RATE)|M(O2_B1S1)|M(FUEL_LEVEL);
        break;
    case DASH_MODE_JDM:
        mask = M(RPM)|M(SPEED)|M(COOLANT_TEMP)|M(VOLTAGE)|M(THROTTLE);
        break;
    case DASH_MODE_TOUGE:
        mask = M(RPM)|M(SPEED)|M(THROTTLE)|M(ENGINE_LOAD)|M(IAT)|
               M(COOLANT_TEMP)|M(TIMING_ADV)|M(O2_B1S1)|M(FUEL_RATE)|M(MAF);
        break;
    case DASH_MODE_VTEC:
        mask = M(RPM)|M(SPEED)|M(THROTTLE)|M(ENGINE_LOAD)|M(IAT)|
               M(COOLANT_TEMP)|M(STFT)|M(LTFT)|M(TIMING_ADV)|M(O2_B1S1)|
               M(FUEL_RATE)|M(MAF);
        break;
    default:
        return 0; /* 0 = poll all */
    }
    return mask | core;
#undef M
}

static void poll_obd(void) {
    if (!socket_is_connected(&g_sock))
        return;

    uint32_t now = time_now_ms();
    PidIndex pid = pid_scheduler_next(&g_sched, now);
    if (pid >= PID_COUNT)
        return; /* nothing due this frame */

    char resp[ELM327_RESP_MAX];
    int n = elm327_query_pid(&g_sock, PID_TABLE[pid].cmd, resp, sizeof(resp));
    if (n <= 0) {
        LOG_W("poll_obd: socket dead on PID[%d] %s (%s)", (int)pid,
              PID_TABLE[pid].name, PID_TABLE[pid].cmd);
        socket_close(&g_sock);
        return;
    }

    g_sched.last_poll_ms[pid] = time_now_ms();

    ParseResult pr = obd_parse_response(pid, resp);
    if (pr.valid) {
        int first_hit = (g_vehicle.supported[pid] != 1);
        g_vehicle.supported[pid] = 1;
        g_sched.fail_count[pid]  = 0;
        float smoothed = filter_update(&g_filters[pid], pr.value);
        telemetry_update(&g_vehicle, pid, smoothed, time_now_ms());
        if (first_hit)
            LOG_I("PID[%d] %s first valid: %.2f %s", (int)pid,
                  PID_TABLE[pid].name, smoothed, PID_TABLE[pid].unit);
        else
            LOG_D("PID[%d] %s = %.2f %s", (int)pid,
                  PID_TABLE[pid].name, smoothed, PID_TABLE[pid].unit);
    } else if (g_vehicle.supported[pid] != 1) {
        /* SEARCHING.../STOPPED are transient bus-detection states; don't
           penalise the PID.  Only NO DATA or '?' mean the PID is actually
           unsupported by this ECU. */
        if (resp[0] == '\0' ||
            strstr(resp, "SEARCHING") || strstr(resp, "STOPPED") ||
            strstr(resp, "BUS INIT")  || strstr(resp, "UNABLE TO CONNECT")) {
            LOG_D("PID[%d] %s transient bus state: '%s'", (int)pid,
                  PID_TABLE[pid].name, resp);
        } else if (strstr(resp, "NO DATA") || strchr(resp, '?')) {
            if (g_sched.fail_count[pid] < 255) g_sched.fail_count[pid]++;
            LOG_D("PID[%d] %s NO DATA (fail %d/3)", (int)pid,
                  PID_TABLE[pid].name, (int)g_sched.fail_count[pid]);
            if (g_sched.fail_count[pid] >= 3) {
                g_vehicle.supported[pid] = 2;
                g_sched.skip[pid]        = 1;
                LOG_I("PID[%d] %s (%s) marked unsupported, skip polling",
                      (int)pid, PID_TABLE[pid].name, PID_TABLE[pid].cmd);
            }
        } else {
            LOG_W("PID[%d] %s unrecognised response: '%s'", (int)pid,
                  PID_TABLE[pid].name, resp);
        }
    }
}

/* ------------------------------------------------------------------ */

/* ---- Connecting-screen car animation -------------------------------- */

#define CONN_ROAD_Y    252
#define CONN_CAR_W     52
#define CONN_CAR_CYCLE 2000  /* ms per full left-to-right traversal */

static void draw_honda_car(int cx) {
    int ry = CONN_ROAD_Y;

    uint32_t body   = RGBA(215, 215, 220, 255);
    uint32_t roof_c = RGBA(222, 222, 228, 255);
    uint32_t glass  = RGBA(95,  148, 195, 210);
    uint32_t whl    = RGBA(22,  22,  22,  255);
    uint32_t hub    = RGBA(135, 135, 140, 255);
    uint32_t hl     = RGBA(255, 245, 130, 255);
    uint32_t tl     = RGBA(218, 18,  18,  255);
    uint32_t dark   = RGBA(32,  32,  36,  255);

    /* Shadow */
    renderer_draw_rect(cx+4,  ry,     44,  3,  RGBA(0, 0, 0, 55));

    /* Wheels (drawn behind lower body) */
    renderer_draw_rect(cx+5,  ry-9,  11,  9,  whl);  /* rear  */
    renderer_draw_rect(cx+8,  ry-7,   5,  5,  hub);
    renderer_draw_rect(cx+36, ry-9,  11,  9,  whl);  /* front */
    renderer_draw_rect(cx+39, ry-7,   5,  5,  hub);

    /* Lower body */
    renderer_draw_rect(cx+1,  ry-19, 50, 11,  body);

    /* Wheel arch cutouts */
    renderer_draw_rect(cx+5,  ry-12, 11,  4,  dark);
    renderer_draw_rect(cx+36, ry-12, 11,  4,  dark);

    /* Door crease */
    renderer_draw_rect(cx+1,  ry-15, 50,  1,  RGBA(175, 175, 180, 255));

    /* Front A-pillar (front = right, car moves right) */
    renderer_draw_filled_tri(
        (float)(cx+39), (float)(ry-19),
        (float)(cx+51), (float)(ry-19),
        (float)(cx+40), (float)(ry-28), body);
    /* Rear C-pillar */
    renderer_draw_filled_tri(
        (float)(cx+1),  (float)(ry-19),
        (float)(cx+13), (float)(ry-19),
        (float)(cx+12), (float)(ry-28), body);

    /* Roof */
    renderer_draw_rect(cx+12, ry-28, 28, 10,  roof_c);

    /* Windows */
    renderer_draw_rect(cx+13, ry-27, 11,  7,  glass);  /* rear  */
    renderer_draw_rect(cx+25, ry-28,  2,  9,  body);   /* B-pillar */
    renderer_draw_rect(cx+27, ry-27, 11,  7,  glass);  /* front */

    /* Headlights (front = right) */
    renderer_draw_rect(cx+49, ry-17,  2,  3,  hl);
    renderer_draw_rect(cx+49, ry-14,  2,  2,  RGBA(200, 200, 200, 255));

    /* Taillights (rear = left) */
    renderer_draw_rect(cx+1,  ry-17,  2,  5,  tl);

    /* Bumpers */
    renderer_draw_rect(cx+49, ry-11,  3,  2,  RGBA(185, 185, 190, 255));
    renderer_draw_rect(cx+1,  ry-11,  3,  2,  RGBA(185, 185, 190, 255));
}

static void render_connecting_screen(const char *msg) {
    uint32_t     now = time_now_ms();
    const Theme *t   = theme_current();

    renderer_begin_frame();
    renderer_clear(t->bg);

    /* Title */
    {
        const char *title = "Honda OBD2 Dashboard";
        int tx = (SCREEN_W - (int)strlen(title) * 8) / 2;
        font_draw_str(tx, 85, title, t->text_primary, 1);
    }

    /* Spinner + status line */
    {
        static const char *spin = "-\\|/";
        char buf[160];
        snprintf(buf, sizeof(buf), "%c  %s", spin[(now / 200) % 4], msg);
        int tx = (SCREEN_W - (int)strlen(buf) * 8) / 2;
        if (tx < 4) tx = 4;
        font_draw_str(tx, 108, buf, t->text_secondary, 1);
    }

    /* Road fill */
    renderer_draw_rect(0, CONN_ROAD_Y,    SCREEN_W, SCREEN_H - CONN_ROAD_Y,
                       RGBA(52, 54, 58, 255));
    /* Road top/bottom edges (faded yellow curb line) */
    renderer_draw_rect(0, CONN_ROAD_Y,    SCREEN_W, 2, RGBA(80, 80, 55, 255));
    renderer_draw_rect(0, SCREEN_H - 3,   SCREEN_W, 3, RGBA(80, 80, 55, 255));

    /* Animated center dashes - scroll left as car moves right */
    int dash_off = (int)((now / 28) % 40);
    for (int x = -dash_off; x < SCREEN_W + 40; x += 40)
        renderer_draw_rect(x, CONN_ROAD_Y + 9, 22, 3, RGBA(185, 185, 185, 200));

    /* Car: full traversal in CONN_CAR_CYCLE ms */
    int travel = SCREEN_W + CONN_CAR_W + 20;
    int car_x  = (int)((now % (uint32_t)CONN_CAR_CYCLE)
                       * (uint32_t)travel / (uint32_t)CONN_CAR_CYCLE)
                 - CONN_CAR_W - 10;
    draw_honda_car(car_x);

    renderer_end_frame();
}

/* ---- Help overlay (shown on top of any dashboard via Select) -------- */

static void draw_help_overlay(void) {
    const Theme *t  = theme_current();
    int px = 50, py = 28, pw = SCREEN_W - 100, ph = SCREEN_H - 56;

    renderer_draw_rect(px,    py,    pw,   ph,   RGBA(0, 0, 0, 215));
    renderer_draw_rect(px,    py,    pw,    1,   t->accent);
    renderer_draw_rect(px,    py+ph, pw,    1,   t->accent);
    renderer_draw_rect(px,    py,     1,   ph,   t->accent);
    renderer_draw_rect(px+pw, py,     1,   ph,   t->accent);

    int cx = px + pw / 2;
    const char *title = "CONTROLS";
    font_draw_str(cx - (int)strlen(title) * 4, py + 8, title, t->accent, 1);
    renderer_draw_rect(px, py + 20, pw, 1, RGBA(40, 40, 55, 255));

    static const struct { const char *btn; const char *act; } map[] = {
        { "L / R",      "Cycle dashboard screens"        },
        { "X",          "Cycle colour theme"             },
        { "Start",      "Return to main menu"            },
        { "Select",     "Toggle this help"               },
        { "Triangle",   "Reset trip  (TRIP / ECONOMY)"  },
        { "L+R+Start",  "Emergency disconnect"           },
    };
    for (int i = 0; i < 6; i++) {
        int y = py + 26 + i * 22;
        font_draw_str(px + 14, y, map[i].btn, t->accent,       1);
        font_draw_str(px + 108, y, map[i].act, t->text_primary, 1);
    }

    const char *close = "Press Select to close";
    font_draw_str(cx - (int)strlen(close) * 4, py + ph - 14,
                  close, t->text_secondary, 1);
}

/* ---- Main menu -------------------------------------------------------- */

static void render_main_menu(void) {
    uint32_t     now = time_now_ms();
    const Theme *t   = theme_current();

    renderer_begin_frame();
    renderer_clear(t->bg);

    /* Header */
    renderer_draw_rect(0, 0, SCREEN_W, 28, RGBA(12, 12, 18, 255));
    font_draw_str(12, 9, "HONDA OBD2 DASHBOARD", t->text_primary, 1);
    int wlan = sceWlanGetSwitchState();
    const char *wstr = wlan ? "WLAN ON" : "WLAN OFF";
    font_draw_str(SCREEN_W - (int)strlen(wstr) * 8 - 10, 9,
                  wstr, wlan ? COLOR_GREEN : t->danger, 1);
    renderer_draw_rect(0, 28, SCREEN_W, 1, t->accent);

    /* Three menu items */
    char obd_sub[56];
    if (g_settings.ap_config_idx > 0)
        snprintf(obd_sub, sizeof(obd_sub), "%s : %d",
                 g_settings.obd_ip, g_settings.obd_port);
    else
        snprintf(obd_sub, sizeof(obd_sub), "First-time setup required");

    static const char *labels[3] = {
        "Connect to OBD",
        "Sample Data",
        "Settings",
    };
    const char *subs[3] = {
        obd_sub,
        "Simulated live sensor data",
        "WiFi network / theme / poll rate",
    };

    for (int i = 0; i < 3; i++) {
        int y   = 38 + i * 62;
        int sel = (i == g_main_menu_sel);

        if (sel) {
            renderer_draw_rect(10, y - 2, SCREEN_W - 20, 52, RGBA(22, 24, 36, 255));
            renderer_draw_rect(10, y - 2, 4, 52, t->accent);
        }

        int lscale = sel ? 2 : 1;
        int ly     = y + (sel ? 4 : 12);
        int sy     = ly + lscale * 8 + 4;

        font_draw_str(22, ly, labels[i], sel ? t->text_primary  : t->text_secondary, lscale);
        font_draw_str(22, sy, subs[i],   sel ? t->accent        : RGBA(62, 62, 74, 255), 1);
    }

    /* Hint */
    const char *hint = "Up/Down: move   X: confirm";
    font_draw_str((SCREEN_W - (int)strlen(hint) * 8) / 2,
                  225, hint, t->text_secondary, 1);

    /* Road + car animation */
    renderer_draw_rect(0, CONN_ROAD_Y,    SCREEN_W, SCREEN_H - CONN_ROAD_Y,
                       RGBA(52, 54, 58, 255));
    renderer_draw_rect(0, CONN_ROAD_Y,    SCREEN_W, 2,  RGBA(80, 80, 55, 255));
    renderer_draw_rect(0, SCREEN_H - 3,   SCREEN_W, 3,  RGBA(80, 80, 55, 255));
    int dash_off = (int)((now / 28) % 40);
    for (int x = -dash_off; x < SCREEN_W + 40; x += 40)
        renderer_draw_rect(x, CONN_ROAD_Y + 9, 22, 3, RGBA(185, 185, 185, 200));
    int travel = SCREEN_W + CONN_CAR_W + 20;
    int car_x  = (int)((now % (uint32_t)CONN_CAR_CYCLE)
                       * (uint32_t)travel / (uint32_t)CONN_CAR_CYCLE)
                 - CONN_CAR_W - 10;
    draw_honda_car(car_x);

    renderer_end_frame();
}

/* ------------------------------------------------------------------ */

static void render_status(const char *msg) {
    renderer_begin_frame();
    renderer_clear(theme_current()->bg);
    font_draw_str(10, 120, msg, theme_current()->text_primary, 1);
    renderer_end_frame();
}

/* ------------------------------------------------------------------ */

void app_run(void) {
    while (g_running) {
        uint32_t frame_start = time_now_ms();

        /* Setup / settings mode */
        if (g_in_setup) {
            input_update(&g_input);
            SetupResult r = setup_update(&g_input);

            renderer_begin_frame();
            setup_render();
            renderer_end_frame();

            if (r != SETUP_RESULT_CONTINUE)
                leave_setup(r);

            uint32_t e = time_elapsed_ms(frame_start);
            if (e < FRAME_TIME_MS) time_sleep_ms(FRAME_TIME_MS - e);
            continue;
        }

        handle_input();

        switch (g_state) {

        case APP_STATE_MAIN_MENU:
            render_main_menu();
            break;

        case APP_STATE_SETUP:
            /* Will be handled above on next iteration */
            break;

        case APP_STATE_WIFI_CONNECTING: {
            char wstatus[64];
            snprintf(wstatus, sizeof(wstatus), "Connecting to WiFi (slot %d)...",
                     g_settings.ap_config_idx);
            render_connecting_screen(wstatus);
            LOG_I("WiFi connecting: slot %d", g_settings.ap_config_idx);
            if (wifi_init() == 0 &&
                wifi_connect(g_settings.ap_config_idx) == 0) {
                g_state = APP_STATE_OBD_CONNECTING;
                reconnect_reset(&g_reconnect);
            } else {
                LOG_E("WiFi failed for slot %d - waiting for reconfigure",
                      g_settings.ap_config_idx);
                render_status("WiFi failed. Press Start to return to menu.");
                time_sleep_ms(2000);
            }
            break;
        }

        case APP_STATE_OBD_CONNECTING:
            render_connecting_screen(g_obd_status[0] ? g_obd_status : "Connecting to OBD adapter...");
            if (reconnect_should_try(&g_reconnect, time_now_ms())) {
                if (g_reconnect.attempt == 0) {
                    LOG_I("OBD connect: saved endpoint=%s:%d poll_interval=%dms",
                          g_settings.obd_ip, g_settings.obd_port,
                          g_settings.poll_interval_ms);
                    /* First attempt: probe all known endpoints */
                    char found_ip[16];
                    int  found_port = 0;
                    snprintf(g_obd_status, sizeof(g_obd_status),
                             "Probing OBD adapters (0/%d)...", 8);
                    render_connecting_screen(g_obd_status);
                    if (elm327_probe(&g_sock, found_ip, sizeof(found_ip), &found_port,
                                     g_obd_status, sizeof(g_obd_status)) == 0) {
                        /* Persist working endpoint */
                        snprintf(g_settings.obd_ip, sizeof(g_settings.obd_ip), "%s", found_ip);
                        g_settings.obd_port = found_port;
                        settings_save(&g_settings);
                        /* sock is already connected from probe - just init ELM327 */
                        if (elm327_init(&g_sock) == 0) {
                            reconnect_on_success(&g_reconnect);
                            g_state = APP_STATE_RUNNING;
                            LOG_I("OBD ready at %s:%d", found_ip, found_port);
                        } else {
                            LOG_E("ELM327 init failed after probe connect");
                            socket_close(&g_sock);
                            reconnect_try(&g_reconnect, &g_sock);
                        }
                    } else {
                        /* Probe exhausted - start backoff, retry saved endpoint */
                        snprintf(g_obd_status, sizeof(g_obd_status),
                                 "No adapter found. Retrying %s:%d...",
                                 g_settings.obd_ip, g_settings.obd_port);
                        reconnect_try(&g_reconnect, &g_sock);
                    }
                } else {
                    /* Subsequent attempts: reconnect known endpoint with backoff */
                    snprintf(g_obd_status, sizeof(g_obd_status),
                             "Reconnecting %s:%d (attempt %d)...",
                             g_settings.obd_ip, g_settings.obd_port,
                             g_reconnect.attempt + 1);
                    socket_init(&g_sock, g_settings.obd_ip, g_settings.obd_port);
                    if (reconnect_try(&g_reconnect, &g_sock) == 0) {
                        if (elm327_init(&g_sock) == 0) {
                            reconnect_on_success(&g_reconnect);
                            g_obd_status[0] = '\0';
                            g_state = APP_STATE_RUNNING;
                            LOG_I("OBD ready");
                        } else {
                            LOG_E("ELM327 init failed after reconnect");
                            socket_close(&g_sock);
                        }
                    }
                }
            } else {
                time_sleep_ms(200);
            }
            break;

        case APP_STATE_RUNNING: {
            static uint32_t last_frame_ms = 0;
            uint32_t now_ms  = time_now_ms();
            uint32_t dt_ms   = (last_frame_ms == 0) ? FRAME_TIME_MS
                                                     : (now_ms - last_frame_ms);
            last_frame_ms = now_ms;

            if (g_demo_mode) {
                simulate_demo();
            } else {
                static uint32_t s_last_mask = 0;
                uint32_t new_mask = get_mode_pid_mask(g_dash_mode);
                if (new_mask != s_last_mask) {
                    LOG_I("active_mask 0x%08x -> 0x%08x (mode %d)",
                          (unsigned)s_last_mask, (unsigned)new_mask, (int)g_dash_mode);
                    s_last_mask = new_mask;
                }
                g_sched.active_mask = new_mask;
                /* OBD polling, socket-dead detection, and DTC read run in obd_thread_func */
            }

            derived_compute(&g_derived, &g_vehicle, dt_ms);

            renderer_begin_frame();
            renderer_clear(theme_current()->bg);
            dashboard_render(&g_vehicle, &g_dtc, &g_derived, g_dash_mode);
            dashboard_render_status_bar(&g_vehicle, g_dash_mode,
                                        g_demo_mode ? 2
                                                    : socket_is_connected(&g_sock));
            /* Redline flash: semi-transparent red pulse at >= 6500 RPM */
            if (theme_current()->redline_flash && g_vehicle.rpm >= 6500.0f) {
                if ((time_now_ms() / 120) % 2 == 0)
                    renderer_draw_rect(0, 0, SCREEN_W, SCREEN_H - 12,
                                       RGBA(255, 0, 0, 35));
            }
            if (g_help_visible) draw_help_overlay();
            renderer_end_frame();
            break;
        }

        case APP_STATE_ERROR:
            render_status("Fatal error. Press Start to return to menu.");
            break;

        default:
            g_running = 0;
            break;
        }

        uint32_t elapsed = time_elapsed_ms(frame_start);
        if (elapsed < FRAME_TIME_MS)
            time_sleep_ms(FRAME_TIME_MS - elapsed);
    }
}

/* ------------------------------------------------------------------ */

void app_shutdown(void) {
    g_running = 0;
    if (g_obd_thread_id >= 0)
        sceKernelTerminateDeleteThread(g_obd_thread_id);
    socket_close(&g_sock);
    wifi_shutdown();
    renderer_shutdown();
    LOG_I("App shutdown complete");
}
