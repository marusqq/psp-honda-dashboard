#pragma once
#include <stdint.h>

uint32_t time_now_ms(void);
void     time_sleep_ms(uint32_t ms);
uint32_t time_elapsed_ms(uint32_t start_ms);
