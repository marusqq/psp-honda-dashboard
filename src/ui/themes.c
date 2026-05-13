#include "ui/themes.h"
#include "ui/renderer.h"

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
