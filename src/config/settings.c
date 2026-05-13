#include <stdio.h>
#include <string.h>
#include "config/settings.h"
#include "net/socket.h"
#include "utils/log.h"

void settings_defaults(Settings *s) {
    memset(s, 0, sizeof(*s));
    s->magic           = SETTINGS_MAGIC;
    s->version         = SETTINGS_VERSION;
    strncpy(s->obd_ip, VGATE_DEFAULT_IP, sizeof(s->obd_ip) - 1);
    s->obd_port        = VGATE_DEFAULT_PORT;
    s->poll_interval_ms = 200;
    s->ap_config_idx   = 0;  /* 0 = unconfigured, triggers setup wizard */
    s->theme           = THEME_OEM_HONDA;
    s->default_mode    = DASH_MODE_DIGITAL;
    s->use_metric      = 1;
}

int settings_load(Settings *s) {
    FILE *fp = fopen(SETTINGS_PATH, "rb");
    if (!fp) {
        LOG_I("No settings file, using defaults");
        settings_defaults(s);
        return 0;
    }

    Settings tmp;
    size_t n = fread(&tmp, 1, sizeof(tmp), fp);
    fclose(fp);

    if (n != sizeof(tmp) || tmp.magic != SETTINGS_MAGIC || tmp.version != SETTINGS_VERSION) {
        LOG_W("Settings corrupt/version mismatch, using defaults");
        settings_defaults(s);
        return 0;
    }

    *s = tmp;
    LOG_I("Settings loaded from %s", SETTINGS_PATH);
    return 1;
}

int settings_save(const Settings *s) {
    FILE *fp = fopen(SETTINGS_PATH, "wb");
    if (!fp) {
        LOG_E("Cannot open settings for write: %s", SETTINGS_PATH);
        return -1;
    }

    size_t n = fwrite(s, 1, sizeof(*s), fp);
    fclose(fp);

    if (n != sizeof(*s)) {
        LOG_E("Settings write incomplete");
        return -1;
    }

    LOG_I("Settings saved");
    return 0;
}
