#pragma once

typedef struct {
    char ip[16];
    int  connected;
    int  ap_config_idx;
} WifiStatus;

int  wifi_init(void);
int  wifi_connect(int ap_config_idx);
int  wifi_is_connected(void);
void wifi_get_status(WifiStatus *out);
void wifi_shutdown(void);
