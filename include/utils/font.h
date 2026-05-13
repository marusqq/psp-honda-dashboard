#pragma once
#include <stdint.h>

/* 8x8 bitmap font, printable ASCII 0x20-0x7E (95 chars).
   Each char = 8 bytes (one per row), MSB = leftmost pixel.
   Public domain IBM CP437 subset. */

extern const uint8_t font8x8[95][8];

void font_draw_char(int x, int y, char c, uint32_t color, int scale);
void font_draw_str(int x, int y, const char *str, uint32_t color, int scale);
int  font_str_width(const char *str, int scale);
