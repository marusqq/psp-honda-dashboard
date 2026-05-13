#pragma once

#define LOG_LEVEL_DEBUG 0
#define LOG_LEVEL_INFO  1
#define LOG_LEVEL_WARN  2
#define LOG_LEVEL_ERROR 3

void log_init(const char *path);
void log_write(int level, const char *fmt, ...);
void log_close(void);

#define LOG_D(fmt, ...) log_write(LOG_LEVEL_DEBUG, "[D] " fmt, ##__VA_ARGS__)
#define LOG_I(fmt, ...) log_write(LOG_LEVEL_INFO,  "[I] " fmt, ##__VA_ARGS__)
#define LOG_W(fmt, ...) log_write(LOG_LEVEL_WARN,  "[W] " fmt, ##__VA_ARGS__)
#define LOG_E(fmt, ...) log_write(LOG_LEVEL_ERROR, "[E] " fmt, ##__VA_ARGS__)
