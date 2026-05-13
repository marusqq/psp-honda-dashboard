#include <string.h>
#include <pspnet.h>
#include <pspnet_inet.h>
#include <pspnet_apctl.h>
#include "net/wifi.h"
#include "utils/log.h"
#include "utils/time.h"

#define WIFI_CONNECT_TIMEOUT_MS 15000

static WifiStatus g_status;

int wifi_init(void) {
    memset(&g_status, 0, sizeof(g_status));

    int r;
    r = sceNetInit(0x20000, 0x20, 0x1000, 0x20, 0x1000);
    if (r < 0) { LOG_E("sceNetInit: %08x", r); return -1; }

    r = sceNetInetInit();
    if (r < 0) { LOG_E("sceNetInetInit: %08x", r); return -1; }

    r = sceNetApctlInit(0x1400, 48);
    if (r < 0) { LOG_E("sceNetApctlInit: %08x", r); return -1; }

    LOG_I("WiFi stack initialized");
    return 0;
}

int wifi_connect(int ap_config_idx) {
    g_status.connected    = 0;
    g_status.ap_config_idx = ap_config_idx;

    LOG_I("Connecting to AP config %d", ap_config_idx);
    int r = sceNetApctlConnect(ap_config_idx);
    if (r < 0) {
        LOG_E("sceNetApctlConnect: %08x", r);
        return -1;
    }

    uint32_t start = time_now_ms();
    int state = 0;

    while (state != PSP_NET_APCTL_STATE_GOT_IP) {
        if (time_elapsed_ms(start) > WIFI_CONNECT_TIMEOUT_MS) {
            LOG_E("WiFi connect timeout");
            return -1;
        }

        r = sceNetApctlGetState(&state);
        if (r < 0) {
            LOG_E("sceNetApctlGetState: %08x", r);
            return -1;
        }
        time_sleep_ms(50);
    }

    /* Get assigned IP */
    union SceNetApctlInfo info;
    sceNetApctlGetInfo(PSP_NET_APCTL_INFO_IP, &info);
    memcpy(g_status.ip, info.ip, sizeof(g_status.ip) - 1);
    g_status.ip[sizeof(g_status.ip) - 1] = '\0';
    g_status.connected = 1;

    LOG_I("WiFi connected, IP: %s", g_status.ip);
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
    sceNetApctlDisconnect();
    sceNetApctlTerm();
    sceNetInetTerm();
    sceNetTerm();
    g_status.connected = 0;
    LOG_I("WiFi shutdown");
}
