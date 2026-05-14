#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <psprtc.h>
#include "utils/log.h"

static FILE *g_log_fp = NULL;

void log_init(const char *path) {
    g_log_fp = fopen(path, "w");
}

void log_write(int level, const char *fmt, ...) {
    if (!g_log_fp)
        return;

    static const char *level_str[] = {"DBG", "INF", "WRN", "ERR"};
    if (level < 0 || level > 3)
        level = 3;

    u64 tick;
    sceRtcGetCurrentTick(&tick);
    u32 ms = (u32)(tick / (sceRtcGetTickResolution() / 1000));

    fprintf(g_log_fp, "[%08lu][%s] ", (unsigned long)ms, level_str[level]);

    va_list args;
    va_start(args, fmt);
    vfprintf(g_log_fp, fmt, args);
    va_end(args);

    fprintf(g_log_fp, "\n");
    fflush(g_log_fp);
}

void log_close(void) {
    if (g_log_fp) {
        fclose(g_log_fp);
        g_log_fp = NULL;
    }
}
