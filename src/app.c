#include <string.h>
#include <pspkernel.h>
#include "app.h"
#include "net/wifi.h"
#include "net/socket.h"
#include "net/reconnect.h"
#include "obd/elm327.h"
#include "obd/pid.h"
#include "obd/parser.h"
#include "obd/diagnostics.h"
#include "telemetry/model.h"
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

static volatile int  g_running  = 0;
static AppState      g_state    = APP_STATE_INIT;
static Settings      g_settings;
static TcpSocket     g_sock;
static ReconnectState g_reconnect;
static VehicleState  g_vehicle;
static DtcList       g_dtc;
static PidScheduler  g_sched;
static MovingAvg     g_filters[PID_COUNT];
static DashMode      g_dash_mode;
static InputState    g_input;
static int           g_in_setup = 0;

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
        theme_set(g_settings.theme);
        g_dash_mode = g_settings.default_mode;
        pid_scheduler_init(&g_sched, g_settings.poll_interval_ms);

        /* WiFi was connected by setup sanity check - jump straight to OBD */
        socket_init(&g_sock, g_settings.obd_ip, g_settings.obd_port);
        reconnect_reset(&g_reconnect);
        memset(&g_dtc, 0, sizeof(g_dtc));
        g_state = APP_STATE_OBD_CONNECTING;
        LOG_I("Setup done, connecting to OBD");
    } else {
        /* Cancelled - return to wherever we were */
        if (g_state == APP_STATE_RUNNING) {
            /* Reconnect since we closed the socket */
            g_state = APP_STATE_OBD_CONNECTING;
        } else {
            /* Was in setup from init - treat cancel as retry */
            g_state = APP_STATE_OBD_CONNECTING;
        }
    }
}

/* ------------------------------------------------------------------ */

int app_init(void) {
    mem_init();

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

    /* First launch: ap_config_idx == 0 means never configured */
    if (g_settings.ap_config_idx == 0) {
        LOG_I("First launch - showing setup wizard");
        enter_setup(0);
        g_state = APP_STATE_SETUP;
    } else {
        g_state = APP_STATE_WIFI_CONNECTING;
    }

    LOG_I("App init OK");
    return 0;
}

/* ------------------------------------------------------------------ */

static void handle_input(void) {
    input_update(&g_input);

    if (g_in_setup)
        return;  /* app.c input suppressed during setup */

    /* Select+Start: open settings menu */
    if (input_held(&g_input, BTN_SELECT) && input_pressed(&g_input, BTN_START)) {
        LOG_I("Select+Start: opening settings");
        enter_setup(1);
        g_state = APP_STATE_SETUP;
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
    }
}

/* ------------------------------------------------------------------ */

static void poll_obd(void) {
    if (!socket_is_connected(&g_sock))
        return;

    uint32_t now = time_now_ms();
    PidIndex pid = pid_scheduler_next(&g_sched, now);

    char resp[ELM327_RESP_MAX];
    int n = elm327_query_pid(&g_sock, PID_TABLE[pid].cmd, resp, sizeof(resp));
    if (n <= 0) {
        socket_close(&g_sock);
        return;
    }

    g_sched.last_poll_ms[pid] = time_now_ms();

    ParseResult pr = obd_parse_response(pid, resp);
    if (pr.valid) {
        float smoothed = filter_update(&g_filters[pid], pr.value);
        telemetry_update(&g_vehicle, pid, smoothed, time_now_ms());
    }
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
    socket_init(&g_sock, g_settings.obd_ip, g_settings.obd_port);
    reconnect_reset(&g_reconnect);

    while (g_running) {
        uint32_t frame_start = time_now_ms();

        input_update(&g_input);

        /* Setup / settings mode */
        if (g_in_setup) {
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

        case APP_STATE_SETUP:
            /* Will be handled above on next iteration */
            break;

        case APP_STATE_WIFI_CONNECTING:
            render_status("Connecting to WiFi...");
            if (wifi_init() == 0 &&
                wifi_connect(g_settings.ap_config_idx) == 0) {
                g_state = APP_STATE_OBD_CONNECTING;
            } else {
                render_status("WiFi failed. Select+Start to reconfigure.");
                time_sleep_ms(3000);
            }
            break;

        case APP_STATE_OBD_CONNECTING:
            render_status("Connecting to OBD adapter...");
            socket_init(&g_sock, g_settings.obd_ip, g_settings.obd_port);
            if (socket_connect(&g_sock) == 0) {
                if (elm327_init(&g_sock) == 0) {
                    reconnect_on_success(&g_reconnect);
                    g_state = APP_STATE_RUNNING;
                    LOG_I("OBD ready");
                } else {
                    socket_close(&g_sock);
                    time_sleep_ms(2000);
                }
            } else {
                if (reconnect_should_try(&g_reconnect, time_now_ms()))
                    reconnect_try(&g_reconnect, &g_sock);
                else
                    time_sleep_ms(200);
            }
            break;

        case APP_STATE_RUNNING:
            poll_obd();

            if (!socket_is_connected(&g_sock)) {
                LOG_W("OBD connection lost");
                g_state = APP_STATE_OBD_CONNECTING;
            }

            if (!g_dtc.read_ok && socket_is_connected(&g_sock))
                dtc_read(&g_sock, &g_dtc);

            renderer_begin_frame();
            renderer_clear(theme_current()->bg);
            dashboard_render(&g_vehicle, &g_dtc, g_dash_mode);
            dashboard_render_status_bar(&g_vehicle, g_dash_mode,
                                        socket_is_connected(&g_sock));
            renderer_end_frame();
            break;

        case APP_STATE_ERROR:
            render_status("Fatal error. Select+Start to reconfigure.");
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
    socket_close(&g_sock);
    wifi_shutdown();
    renderer_shutdown();
    LOG_I("App shutdown complete");
}
