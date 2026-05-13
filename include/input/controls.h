#pragma once
#include <stdint.h>

typedef enum {
    BTN_CROSS    = (1 << 0),
    BTN_CIRCLE   = (1 << 1),
    BTN_SQUARE   = (1 << 2),
    BTN_TRIANGLE = (1 << 3),
    BTN_L        = (1 << 4),
    BTN_R        = (1 << 5),
    BTN_UP       = (1 << 6),
    BTN_DOWN     = (1 << 7),
    BTN_LEFT     = (1 << 8),
    BTN_RIGHT    = (1 << 9),
    BTN_SELECT   = (1 << 10),
    BTN_START    = (1 << 11)
} Button;

typedef struct {
    uint32_t held;
    uint32_t pressed;
    uint32_t released;
} InputState;

void input_update(InputState *state);
int  input_pressed(const InputState *state, Button btn);
int  input_held(const InputState *state, Button btn);
