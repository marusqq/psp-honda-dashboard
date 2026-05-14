#pragma once
#include <stdint.h>

typedef enum {
    THEME_OEM_HONDA = 0,
    THEME_RACING_TUNER,
    THEME_MIDNIGHT_JDM,
    THEME_NEON_TOKYO,
    THEME_SUNRISE,
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
    int         font_scale;      /* 1=normal, 2=large primary values */
    int         use_animations;  /* 1=lerp needle, 0=instant */
    int         needle_style;    /* 0=line, 1=filled triangle */
    int         default_mode;    /* preferred DashMode (int to avoid circular include) */
    int         redline_flash;   /* 1=flash screen red when RPM >= 6500 */
} Theme;

extern const Theme g_themes[THEME_COUNT];

void          theme_set(ThemeID id);
ThemeID       theme_get(void);
const Theme  *theme_current(void);
