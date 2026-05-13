#include <pspctrl.h>
#include "input/controls.h"

static uint32_t psp_to_btn(uint32_t psp) {
    uint32_t b = 0;
    if (psp & PSP_CTRL_CROSS)    b |= BTN_CROSS;
    if (psp & PSP_CTRL_CIRCLE)   b |= BTN_CIRCLE;
    if (psp & PSP_CTRL_SQUARE)   b |= BTN_SQUARE;
    if (psp & PSP_CTRL_TRIANGLE) b |= BTN_TRIANGLE;
    if (psp & PSP_CTRL_LTRIGGER) b |= BTN_L;
    if (psp & PSP_CTRL_RTRIGGER) b |= BTN_R;
    if (psp & PSP_CTRL_UP)       b |= BTN_UP;
    if (psp & PSP_CTRL_DOWN)     b |= BTN_DOWN;
    if (psp & PSP_CTRL_LEFT)     b |= BTN_LEFT;
    if (psp & PSP_CTRL_RIGHT)    b |= BTN_RIGHT;
    if (psp & PSP_CTRL_SELECT)   b |= BTN_SELECT;
    if (psp & PSP_CTRL_START)    b |= BTN_START;
    return b;
}

void input_update(InputState *state) {
    SceCtrlData pad;
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    sceCtrlReadBufferPositive(&pad, 1);

    uint32_t now = psp_to_btn(pad.Buttons);
    state->pressed  = now & ~state->held;
    state->released = state->held & ~now;
    state->held     = now;
}

int input_pressed(const InputState *state, Button btn) {
    return (state->pressed & (uint32_t)btn) != 0;
}

int input_held(const InputState *state, Button btn) {
    return (state->held & (uint32_t)btn) != 0;
}
