#include "ui/themes.h"
#include "ui/renderer.h"
#include "ui/dashboard.h"

const Theme g_themes[THEME_COUNT] = {
    [THEME_OEM_HONDA] = {
        .name           = "OEM Honda",
        .bg             = RGBA(12,  12,  12,  255),
        .text_primary   = RGBA(220, 220, 220, 255),
        .text_secondary = RGBA(140, 140, 140, 255),
        .accent         = RGBA(200, 200, 200, 255),
        .warn           = RGBA(255, 200, 0,   255),
        .danger         = RGBA(220, 30,  30,  255),
        .gauge_track    = RGBA(40,  40,  40,  255),
        .gauge_fill     = RGBA(200, 200, 200, 255),
        .needle         = RGBA(255, 255, 255, 255),
        .redline        = RGBA(220, 30,  30,  255),
        .font_scale     = 1,
        .use_animations = 0,
        .needle_style   = 0,
        .default_mode   = DASH_MODE_DIGITAL,
        .redline_flash  = 0,
    },
    [THEME_RACING_TUNER] = {
        .name           = "Racing Tuner",
        .bg             = RGBA(5,   5,   10,  255),
        .text_primary   = RGBA(255, 255, 255, 255),
        .text_secondary = RGBA(180, 180, 220, 255),
        .accent         = RGBA(0,   200, 255, 255),
        .warn           = RGBA(255, 180, 0,   255),
        .danger         = RGBA(255, 40,  40,  255),
        .gauge_track    = RGBA(20,  20,  40,  255),
        .gauge_fill     = RGBA(0,   200, 255, 255),
        .needle         = RGBA(255, 60,  60,  255),
        .redline        = RGBA(255, 20,  20,  255),
        .font_scale     = 2,
        .use_animations = 1,
        .needle_style   = 1,
        .default_mode   = DASH_MODE_ANALOG,
        .redline_flash  = 1,
    },
    [THEME_MIDNIGHT_JDM] = {
        .name           = "Midnight JDM",
        .bg             = RGBA(8,   0,   12,  255),
        .text_primary   = RGBA(200, 230, 255, 255),
        .text_secondary = RGBA(100, 80,  130, 255),
        .accent         = RGBA(160, 60,  255, 255),
        .warn           = RGBA(255, 160, 0,   255),
        .danger         = RGBA(255, 20,  120, 255),
        .gauge_track    = RGBA(20,  10,  30,  255),
        .gauge_fill     = RGBA(80,  120, 255, 255),
        .needle         = RGBA(255, 240, 255, 255),
        .redline        = RGBA(255, 20,  60,  255),
        .font_scale     = 1,
        .use_animations = 1,
        .needle_style   = 1,
        .default_mode   = DASH_MODE_JDM,
        .redline_flash  = 1,
    },
    [THEME_NEON_TOKYO] = {
        .name           = "Neon Tokyo",
        .bg             = RGBA(0,   0,   0,   255),
        .text_primary   = RGBA(0,   255, 80,  255),
        .text_secondary = RGBA(0,   100, 40,  255),
        .accent         = RGBA(0,   255, 80,  255),
        .warn           = RGBA(255, 220, 0,   255),
        .danger         = RGBA(255, 30,  30,  255),
        .gauge_track    = RGBA(0,   25,  10,  255),
        .gauge_fill     = RGBA(0,   255, 80,  255),
        .needle         = RGBA(200, 255, 200, 255),
        .redline        = RGBA(255, 30,  30,  255),
        .font_scale     = 1,
        .use_animations = 1,
        .needle_style   = 1,
        .default_mode   = DASH_MODE_JDM,
        .redline_flash  = 1,
    },
    [THEME_SUNRISE] = {
        .name           = "Sunrise",
        .bg             = RGBA(10,  6,   3,   255),
        .text_primary   = RGBA(255, 235, 190, 255),
        .text_secondary = RGBA(150, 100, 50,  255),
        .accent         = RGBA(255, 120, 0,   255),
        .warn           = RGBA(255, 220, 0,   255),
        .danger         = RGBA(220, 30,  30,  255),
        .gauge_track    = RGBA(25,  15,  5,   255),
        .gauge_fill     = RGBA(255, 120, 0,   255),
        .needle         = RGBA(255, 200, 100, 255),
        .redline        = RGBA(220, 30,  30,  255),
        .font_scale     = 1,
        .use_animations = 1,
        .needle_style   = 0,
        .default_mode   = DASH_MODE_ANALOG,
        .redline_flash  = 0,
    },
};

static ThemeID g_current = THEME_OEM_HONDA;

void theme_set(ThemeID id) {
    if (id >= 0 && id < THEME_COUNT)
        g_current = id;
}

ThemeID theme_get(void) {
    return g_current;
}

const Theme *theme_current(void) {
    return &g_themes[g_current];
}
