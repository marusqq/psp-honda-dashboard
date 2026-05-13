#pragma once
#include <stdint.h>

typedef enum {
    THEME_OEM_HONDA = 0,
    THEME_RACING_TUNER,
    THEME_COUNT
} ThemeID;

typedef struct {
    const char *name;
    uint32_t    bg;
    uint32_t    text_primary;
    uint32_t    text_secondary;
    uint32_t    accent;
    uint32_t    warn;
    uint32_t    danger;
    uint32_t    gauge_track;
    uint32_t    gauge_fill;
    uint32_t    needle;
    uint32_t    redline;
    int         font_scale;
    int         use_animations;
} Theme;

extern const Theme g_themes[THEME_COUNT];

void          theme_set(ThemeID id);
ThemeID       theme_get(void);
const Theme  *theme_current(void);
