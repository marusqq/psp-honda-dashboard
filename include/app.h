#pragma once

typedef enum {
    APP_STATE_INIT = 0,
    APP_STATE_SETUP,            /* first-launch wizard or Select+Start settings */
    APP_STATE_WIFI_CONNECTING,
    APP_STATE_OBD_CONNECTING,
    APP_STATE_RUNNING,
    APP_STATE_ERROR,
    APP_STATE_EXIT
} AppState;

int  app_init(void);
void app_run(void);
void app_shutdown(void);

AppState app_get_state(void);
void     app_set_state(AppState state);
void     app_request_exit(void);
int      app_is_running(void);
