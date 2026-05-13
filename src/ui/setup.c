#include <stdio.h>
#include <string.h>
#include <pspnet_apctl.h>
#include "ui/setup.h"
#include "ui/renderer.h"
#include "ui/themes.h"
#include "ui/gauge.h"
#include "utils/font.h"
#include "utils/log.h"
#include "utils/time.h"
#include "net/wifi.h"
#include "net/socket.h"
#include "obd/elm327.h"

/* ------------------------------------------------------------------ */
/* Internal state                                                      */
/* ------------------------------------------------------------------ */

typedef enum {
    SCR_WELCOME = 0,
    SCR_NET_PICK,       /* scroll through AP slots 1-9                */
    SCR_NET_TESTING,    /* connecting + OBD sanity check               */
    SCR_NET_RESULT,     /* pass / fail result                          */
    SCR_SETTINGS,       /* full settings menu (Select+Start entry)     */
    SCR_NET_PICK_FROM_SETTINGS, /* network picker launched from settings */
} Screen;

typedef struct {
    /* test results per slot (0=untested, 1=ok, -1=fail) */
    int  slot_result[10];
    char slot_ssid[10][32];
} SlotCache;

static Settings  *g_s            = NULL;
static int        g_settings_mode = 0;
static Screen     g_screen       = SCR_WELCOME;
static int        g_sel_slot     = 1;   /* 1-9 */
static int        g_test_phase   = 0;   /* 0=idle, 1=start, 2=wifi, 3=obd */
static int        g_test_ok      = 0;
static char       g_test_msg[256] = {0};
static int        g_settings_cur = 0;   /* cursor in settings menu */
static SlotCache  g_cache;
static Screen     g_return_screen = SCR_SETTINGS;

#define SLOT_COUNT 9
#define SETTINGS_ITEMS 5  /* WiFi, Theme, Mode, Poll, Units */

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

static const char *theme_name(ThemeID t) {
    return g_themes[t].name;
}
static const char *mode_name(DashMode m) {
    static const char *n[] = {"Digital", "Analog", "Diagnostics", "Performance"};
    return n[m % DASH_MODE_COUNT];
}
static const char *poll_name(int ms) {
    if (ms <= 100) return "100ms (fast)";
    if (ms <= 200) return "200ms";
    return "500ms (slow)";
}
static const char *units_name(int metric) {
    return metric ? "Metric (km/h, C)" : "Imperial (mph, F)";
}

static void draw_title(const char *t, uint32_t col) {
    renderer_draw_rect(0, 0, SCREEN_W, 18, RGBA(20, 20, 20, 255));
    font_draw_str(8, 4, t, col, 1);
    renderer_draw_rect(0, 18, SCREEN_W, 1, col);
}
static void draw_hint(const char *h) {
    renderer_draw_rect(0, 258, SCREEN_W, 14, RGBA(10, 10, 10, 255));
    font_draw_str(8, 261, h, theme_current()->text_secondary, 1);
}
/* ------------------------------------------------------------------ */
/* Sanity check: WiFi + OBD                                           */
/* ------------------------------------------------------------------ */

static void run_test(int slot) {
    WifiStatus ws;
    wifi_get_status(&ws);

    /* Disconnect if already connected to a different network */
    if (ws.connected)
        wifi_shutdown();

    snprintf(g_test_msg, sizeof(g_test_msg), "Connecting to slot %d...", slot);
    LOG_I("Setup: testing slot %d", slot);

    if (wifi_init() != 0 || wifi_connect(slot) != 0) {
        snprintf(g_test_msg, sizeof(g_test_msg), "Slot %d: WiFi connection failed", slot);
        g_cache.slot_result[slot] = -1;
        g_test_ok = 0;
        return;
    }

    /* Read SSID of the network we just connected to */
    union SceNetApctlInfo info;
    if (sceNetApctlGetInfo(PSP_NET_APCTL_INFO_SSID, &info) == 0) {
        memcpy(g_cache.slot_ssid[slot], (const char *)info.ssid,
               sizeof(g_cache.slot_ssid[slot]) - 1);
        g_cache.slot_ssid[slot][sizeof(g_cache.slot_ssid[slot]) - 1] = '\0';
    } else {
        snprintf(g_cache.slot_ssid[slot], sizeof(g_cache.slot_ssid[slot]), "slot %d", slot);
    }

    /* Try reaching the OBD adapter */
    snprintf(g_test_msg, sizeof(g_test_msg), "Checking OBD at %s:%d...",
             g_s->obd_ip, g_s->obd_port);

    TcpSocket sock;
    socket_init(&sock, g_s->obd_ip, g_s->obd_port);
    if (socket_connect(&sock) != 0) {
        snprintf(g_test_msg, sizeof(g_test_msg),
                 "WiFi OK (%s) but OBD unreachable at %s:%d",
                 g_cache.slot_ssid[slot], g_s->obd_ip, g_s->obd_port);
        g_cache.slot_result[slot] = -1;
        g_test_ok = 0;
        socket_close(&sock);
        return;
    }

    /* Quick ELM327 handshake */
    char resp[ELM327_RESP_MAX];
    elm327_send_cmd(&sock, "ATI", resp, sizeof(resp), 2000);
    socket_close(&sock);

    if (strlen(resp) == 0) {
        snprintf(g_test_msg, sizeof(g_test_msg),
                 "WiFi OK (%s) - OBD connected but no ELM response",
                 g_cache.slot_ssid[slot]);
        /* Partial success - adapter reachable but ELM not responding yet */
        g_cache.slot_result[slot] = 1;
        g_test_ok = 1;
        snprintf(g_test_msg, sizeof(g_test_msg),
                 "OK  Network: %s  Adapter: %s:%d",
                 g_cache.slot_ssid[slot], g_s->obd_ip, g_s->obd_port);
    } else {
        g_cache.slot_result[slot] = 1;
        g_test_ok = 1;
        snprintf(g_test_msg, sizeof(g_test_msg),
                 "OK  Network: %s  ELM: %s",
                 g_cache.slot_ssid[slot], resp);
    }

    LOG_I("Setup test slot %d: %s", slot, g_test_msg);
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

void setup_init(Settings *s, int is_settings_mode) {
    g_s             = s;
    g_settings_mode = is_settings_mode;
    g_sel_slot      = (s->ap_config_idx >= 1 && s->ap_config_idx <= SLOT_COUNT)
                      ? s->ap_config_idx : 1;
    g_settings_cur  = 0;
    g_test_phase    = 0;
    g_test_ok       = 0;
    memset(&g_cache, 0, sizeof(g_cache));
    memset(g_test_msg, 0, sizeof(g_test_msg));

    g_screen = is_settings_mode ? SCR_SETTINGS : SCR_WELCOME;
    LOG_I("Setup init (settings_mode=%d)", is_settings_mode);
}

SetupResult setup_update(InputState *input) {
    switch (g_screen) {

    /* ---- WELCOME ---- */
    case SCR_WELCOME:
        if (input_pressed(input, BTN_CROSS) || input_pressed(input, BTN_START))
            g_screen = SCR_NET_PICK;
        if (g_settings_mode && input_pressed(input, BTN_CIRCLE))
            return SETUP_RESULT_CANCEL;
        break;

    /* ---- NETWORK PICKER ---- */
    case SCR_NET_PICK:
    case SCR_NET_PICK_FROM_SETTINGS:
        if (input_pressed(input, BTN_UP))
            g_sel_slot = (g_sel_slot > 1) ? g_sel_slot - 1 : SLOT_COUNT;
        if (input_pressed(input, BTN_DOWN))
            g_sel_slot = (g_sel_slot < SLOT_COUNT) ? g_sel_slot + 1 : 1;

        if (input_pressed(input, BTN_CROSS)) {
            g_return_screen = g_screen;
            g_screen = SCR_NET_TESTING;
            g_test_phase = 1;   /* trigger test next render cycle */
        }

        if (input_pressed(input, BTN_CIRCLE)) {
            if (g_screen == SCR_NET_PICK_FROM_SETTINGS)
                g_screen = SCR_SETTINGS;
            else if (g_settings_mode)
                return SETUP_RESULT_CANCEL;
            /* else: ignore cancel on first-launch wizard */
        }

        if (input_pressed(input, BTN_START)) {
            /* Confirm current slot even without test */
            g_s->ap_config_idx = g_sel_slot;
            if (g_screen == SCR_NET_PICK_FROM_SETTINGS)
                g_screen = SCR_SETTINGS;
            else
                return SETUP_RESULT_DONE;
        }
        break;

    /* ---- TESTING ---- */
    case SCR_NET_TESTING:
        if (g_test_phase == 1) {
            g_test_phase = 2;  /* will run test in render */
        }
        break;

    /* ---- RESULT ---- */
    case SCR_NET_RESULT:
        if (input_pressed(input, BTN_CROSS)) {
            if (g_test_ok) {
                g_s->ap_config_idx = g_sel_slot;
                if (g_return_screen == SCR_NET_PICK_FROM_SETTINGS)
                    g_screen = SCR_SETTINGS;
                else
                    return SETUP_RESULT_DONE;
            } else {
                g_screen = g_return_screen;  /* back to picker to try another */
            }
        }
        if (input_pressed(input, BTN_CIRCLE))
            g_screen = g_return_screen;
        break;

    /* ---- SETTINGS MENU ---- */
    case SCR_SETTINGS: {
        if (input_pressed(input, BTN_UP))
            g_settings_cur = (g_settings_cur + SETTINGS_ITEMS - 1) % SETTINGS_ITEMS;
        if (input_pressed(input, BTN_DOWN))
            g_settings_cur = (g_settings_cur + 1) % SETTINGS_ITEMS;

        if (input_pressed(input, BTN_CROSS)) {
            switch (g_settings_cur) {
            case 0: /* WiFi - open picker */
                g_screen = SCR_NET_PICK_FROM_SETTINGS;
                break;
            case 1: /* Theme */
                g_s->theme = (ThemeID)((g_s->theme + 1) % THEME_COUNT);
                theme_set(g_s->theme);
                break;
            case 2: /* Mode */
                g_s->default_mode = (DashMode)((g_s->default_mode + 1) % DASH_MODE_COUNT);
                break;
            case 3: /* Poll interval */
                if (g_s->poll_interval_ms <= 100)       g_s->poll_interval_ms = 200;
                else if (g_s->poll_interval_ms <= 200)  g_s->poll_interval_ms = 500;
                else                                    g_s->poll_interval_ms = 100;
                break;
            case 4: /* Units */
                g_s->use_metric = !g_s->use_metric;
                break;
            }
        }

        /* Start = save and exit */
        if (input_pressed(input, BTN_START)) {
            settings_save(g_s);
            return SETUP_RESULT_DONE;
        }
        /* Circle = exit without save */
        if (input_pressed(input, BTN_CIRCLE))
            return SETUP_RESULT_CANCEL;
        break;
    }

    default:
        break;
    }

    return SETUP_RESULT_CONTINUE;
}

/* ------------------------------------------------------------------ */
/* Rendering                                                           */
/* ------------------------------------------------------------------ */

void setup_render(void) {
    const Theme *t = theme_current();
    renderer_clear(t->bg);

    /* Run blocking test here so "Testing..." is visible for at least 1 frame */
    if (g_screen == SCR_NET_TESTING && g_test_phase == 2) {
        draw_title("NETWORK TEST", t->accent);
        font_draw_str(16, 60, "Connecting - please wait...", t->text_secondary, 1);
        font_draw_str(16, 80, g_test_msg[0] ? g_test_msg : "Starting...",
                      t->text_primary, 1);
        /* Let this frame render, then we'll actually test on next call */
        g_test_phase = 3;
        return;
    }
    if (g_screen == SCR_NET_TESTING && g_test_phase == 3) {
        run_test(g_sel_slot);
        g_test_phase = 0;
        g_screen = SCR_NET_RESULT;
        /* Fall through to render result immediately */
    }

    switch (g_screen) {

    case SCR_WELCOME:
        draw_title("PSP OBD2 DASHBOARD  SETUP", t->accent);
        font_draw_str(16, 40, "First-time setup", t->text_primary, 2);
        font_draw_str(16, 72, "You need to select the WiFi network", t->text_secondary, 1);
        font_draw_str(16, 88, "used by your OBD adapter (V-link).", t->text_secondary, 1);
        font_draw_str(16, 110, "Make sure:", t->text_primary, 1);
        font_draw_str(24, 126, "1. OBD adapter plugged into car", t->text_secondary, 1);
        font_draw_str(24, 142, "2. PSP WiFi switch ON", t->text_secondary, 1);
        font_draw_str(24, 158, "3. V-link network saved in PSP", t->text_secondary, 1);
        font_draw_str(24, 174, "   Settings > Network Settings", t->text_secondary, 1);
        draw_hint("X / Start: Begin setup");
        break;

    case SCR_NET_PICK:
    case SCR_NET_PICK_FROM_SETTINGS:
        draw_title("SELECT WIFI NETWORK (AP SLOT)", t->accent);
        font_draw_str(16, 24, "Choose the slot configured for your OBD adapter,", t->text_secondary, 1);
        font_draw_str(16, 36, "then press X to test it.", t->text_secondary, 1);

        for (int i = 1; i <= SLOT_COUNT; i++) {
            int y = 52 + (i - 1) * 18;
            int sel = (i == g_sel_slot);
            uint32_t row_col = sel ? t->text_primary : t->text_secondary;

            char label[24];
            snprintf(label, sizeof(label), "Slot %d", i);

            char status[64] = "";
            if (g_cache.slot_result[i] == 1)
                snprintf(status, sizeof(status), "OK - %s", g_cache.slot_ssid[i]);
            else if (g_cache.slot_result[i] == -1)
                snprintf(status, sizeof(status), "FAIL");

            if (sel) renderer_draw_rect(0, y, SCREEN_W, 16, RGBA(40, 40, 40, 255));
            if (sel) font_draw_str(6, y + 3, ">", t->accent, 1);

            uint32_t st_col = (g_cache.slot_result[i] == 1) ? COLOR_GREEN :
                              (g_cache.slot_result[i] == -1) ? t->danger : t->text_secondary;
            font_draw_str(18, y + 3, label, row_col, 1);
            font_draw_str(90, y + 3, status, st_col, 1);
        }
        draw_hint("UP/DOWN: Select  X: Test  Start: Confirm  O: Back");
        break;

    case SCR_NET_TESTING:
        draw_title("NETWORK TEST", t->accent);
        font_draw_str(16, 60, "Connecting - please wait...", t->text_secondary, 1);
        font_draw_str(16, 80, g_test_msg[0] ? g_test_msg : "Starting...",
                      t->text_primary, 1);
        break;

    case SCR_NET_RESULT: {
        draw_title("TEST RESULT", g_test_ok ? COLOR_GREEN : t->danger);

        uint32_t res_col = g_test_ok ? COLOR_GREEN : t->danger;
        const char *res_str = g_test_ok ? "SUCCESS" : "FAILED";
        font_draw_str(16, 36, res_str, res_col, 2);

        /* Wrap long test_msg across multiple lines */
        {
            char tmp[96];
            strncpy(tmp, g_test_msg, sizeof(tmp) - 1);
            tmp[sizeof(tmp) - 1] = '\0';
            int y = 72;
            char *p = tmp;
            while (*p && y < 220) {
                char line[52];
                int i = 0;
                while (i < 51 && p[i] && p[i] != '\n') { line[i] = p[i]; i++; }
                line[i] = '\0';
                font_draw_str(16, y, line, t->text_primary, 1);
                p += i + (p[i] == '\n' ? 1 : 0);
                y += 14;
                if (i == 51) { /* wrap at word boundary next iter */ }
            }
        }

        if (g_test_ok)
            draw_hint("X: Use this network  O: Back to list");
        else
            draw_hint("X / O: Back to list  (try another slot)");
        break;
    }

    case SCR_SETTINGS: {
        draw_title("SETTINGS", t->accent);

        char wifi_val[48];
        if (g_cache.slot_result[g_s->ap_config_idx] == 1)
            snprintf(wifi_val, sizeof(wifi_val), "Slot %d (%s)",
                     g_s->ap_config_idx, g_cache.slot_ssid[g_s->ap_config_idx]);
        else
            snprintf(wifi_val, sizeof(wifi_val), "Slot %d", g_s->ap_config_idx);

        char poll_val[24];
        snprintf(poll_val, sizeof(poll_val), "%s", poll_name(g_s->poll_interval_ms));

        const char *labels[SETTINGS_ITEMS] = {
            "WiFi Network",
            "Theme",
            "Default Mode",
            "Poll Rate",
            "Units",
        };
        const char *values[SETTINGS_ITEMS] = {
            wifi_val,
            theme_name(g_s->theme),
            mode_name(g_s->default_mode),
            poll_val,
            units_name(g_s->use_metric),
        };

        for (int i = 0; i < SETTINGS_ITEMS; i++) {
            int y = 32 + i * 22;
            int sel = (i == g_settings_cur);
            uint32_t lcol = sel ? t->text_primary : t->text_secondary;
            uint32_t vcol = sel ? t->accent : t->text_secondary;

            if (sel) renderer_draw_rect(0, y, SCREEN_W, 20, RGBA(40, 40, 40, 255));
            if (sel) font_draw_str(6, y + 5, ">", t->accent, 1);
            font_draw_str(18, y + 5, labels[i], lcol, 1);
            font_draw_str(170, y + 5, values[i], vcol, 1);
        }

        font_draw_str(16, 160, "Changes take effect immediately.", t->text_secondary, 1);
        draw_hint("UP/DOWN: Move  X: Change  Start: Save & Exit  O: Exit");
        break;
    }

    default:
        break;
    }
}

void setup_shutdown(void) {
    /* nothing to free */
}
