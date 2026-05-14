#include <string.h>
#include <pspnet.h>
#include <pspnet_inet.h>
#include <pspnet_apctl.h>
#include <psputility.h>
#include "net/wifi.h"
#include "utils/log.h"
#include "utils/time.h"

#define WIFI_CONNECT_TIMEOUT_MS 15000

/* 0x8002013a: sceNetInit returns this when net stack already initialized */
#define SCE_NET_ERROR_ALREADY_INIT 0x8002013a

static WifiStatus g_status;
static int        g_initialized = 0;

static void teardown_stack(void) {
    sceNetApctlDisconnect();
    sceNetApctlTerm();
    sceNetInetTerm();
    sceNetTerm();
    g_initialized    = 0;
    g_status.connected = 0;
}

int wifi_init(void) {
    if (g_initialized) {
        LOG_W("wifi_init: tearing down previous session");
        teardown_stack();
    }
    memset(&g_status, 0, sizeof(g_status));

    /* Load net modules - required before sceNetInit on all firmware versions.
     * Returns 0 on success or positive module ID if already loaded; both are OK. */
    int mr;
    mr = sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON);
    LOG_I("LoadNetModule COMMON: 0x%08x (%s)", (unsigned)mr, mr >= 0 ? "ok" : "err");
    mr = sceUtilityLoadNetModule(PSP_NET_MODULE_INET);
    LOG_I("LoadNetModule INET: 0x%08x (%s)", (unsigned)mr, mr >= 0 ? "ok" : "err");

    int r;
    r = sceNetInit(0x20000, 0x20, 0x1000, 0x20, 0x1000);
    if (r == (int)SCE_NET_ERROR_ALREADY_INIT) {
        LOG_W("sceNetInit: already initialized (0x%08x), continuing", (unsigned)r);
    } else if (r < 0) {
        LOG_E("sceNetInit failed: 0x%08x", (unsigned)r);
        return -1;
    } else {
        LOG_I("sceNetInit ok");
    }

    r = sceNetInetInit();
    if (r < 0) { LOG_E("sceNetInetInit failed: 0x%08x", (unsigned)r); return -1; }
    LOG_I("sceNetInetInit ok");

    r = sceNetApctlInit(0x1400, 48);
    if (r < 0) { LOG_E("sceNetApctlInit failed: 0x%08x", (unsigned)r); return -1; }
    LOG_I("sceNetApctlInit ok");

    g_initialized = 1;
    LOG_I("WiFi stack ready");
    return 0;
}

static const char *apctl_state_name(int s) {
    switch (s) {
    case 0: return "DISCONNECTED";
    case 1: return "SCANNING";
    case 2: return "JOINING";
    case 3: return "GETTING_IP";
    case 4: return "GOT_IP";
    default: return "UNKNOWN";
    }
}

int wifi_connect(int ap_config_idx) {
    g_status.connected    = 0;
    g_status.ap_config_idx = ap_config_idx;

    LOG_I("sceNetApctlConnect(slot=%d)", ap_config_idx);
    int r = sceNetApctlConnect(ap_config_idx);
    if (r < 0) {
        LOG_E("sceNetApctlConnect failed: 0x%08x", (unsigned)r);
        return -1;
    }

    uint32_t start = time_now_ms();
    int state = 0;
    int prev_state = -1;

    while (state != PSP_NET_APCTL_STATE_GOT_IP) {
        uint32_t elapsed = time_elapsed_ms(start);
        if (elapsed > WIFI_CONNECT_TIMEOUT_MS) {
            LOG_E("WiFi connect timeout after %u ms (last state: %s)",
                  elapsed, apctl_state_name(state));
            return -1;
        }

        r = sceNetApctlGetState(&state);
        if (r < 0) {
            LOG_E("sceNetApctlGetState failed: 0x%08x", (unsigned)r);
            return -1;
        }

        if (state != prev_state) {
            LOG_I("WiFi state -> %d (%s)  [%u ms]",
                  state, apctl_state_name(state), elapsed);
            prev_state = state;
        }
        time_sleep_ms(50);
    }

    /* Get assigned IP and connected SSID */
    union SceNetApctlInfo info;
    sceNetApctlGetInfo(PSP_NET_APCTL_INFO_IP, &info);
    memcpy(g_status.ip, info.ip, sizeof(g_status.ip) - 1);
    g_status.ip[sizeof(g_status.ip) - 1] = '\0';

    char ssid[33] = {0};
    if (sceNetApctlGetInfo(PSP_NET_APCTL_INFO_SSID, &info) == 0)
        memcpy(ssid, (const char *)info.ssid, sizeof(ssid) - 1);

    g_status.connected = 1;
    LOG_I("WiFi connected: SSID='%s' IP=%s", ssid, g_status.ip);
    return 0;
}

int wifi_is_connected(void) {
    int state = 0;
    sceNetApctlGetState(&state);
    g_status.connected = (state == PSP_NET_APCTL_STATE_GOT_IP);
    return g_status.connected;
}

void wifi_get_status(WifiStatus *out) {
    if (out)
        *out = g_status;
}

void wifi_shutdown(void) {
    if (!g_initialized) return;
    teardown_stack();
    LOG_I("WiFi shutdown");
}
