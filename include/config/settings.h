#pragma once
#include <stdint.h>
#include "../ui/themes.h"
#include "../ui/dashboard.h"

#define SETTINGS_PATH    "ms0:/PSP/GAME/PSP-OBD2/settings.bin"
#define SETTINGS_MAGIC   0x4F424432u
#define SETTINGS_VERSION 4

typedef struct {
    uint32_t magic;
    uint32_t version;
    char     obd_ip[16];
    int      obd_port;
    int      poll_interval_ms;
    int      ap_config_idx;
    ThemeID  theme;
    DashMode default_mode;
    int      use_metric;
} Settings;

void settings_defaults(Settings *s);
int  settings_load(Settings *s);
int  settings_save(const Settings *s);
