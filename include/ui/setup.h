#pragma once
#include "config/settings.h"
#include "input/controls.h"

typedef enum {
    SETUP_RESULT_CONTINUE = 0,
    SETUP_RESULT_DONE,
    SETUP_RESULT_CANCEL,
    SETUP_RESULT_SKIP,   /* user skipped setup, run dashboard without OBD */
} SetupResult;

/* is_settings_mode=0: first-launch wizard
   is_settings_mode=1: in-flight settings menu (Select+Start) */
void        setup_init(Settings *s, int is_settings_mode);
SetupResult setup_update(InputState *input);
void        setup_render(void);
void        setup_shutdown(void);
