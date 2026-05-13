#include <pspkernel.h>
#include <psprtc.h>
#include "utils/time.h"

uint32_t time_now_ms(void) {
    return (uint32_t)(sceKernelGetSystemTimeWide() / 1000ULL);
}

void time_sleep_ms(uint32_t ms) {
    sceKernelDelayThread(ms * 1000);
}

uint32_t time_elapsed_ms(uint32_t start_ms) {
    uint32_t now = time_now_ms();
    return (now >= start_ms) ? (now - start_ms) : (0xFFFFFFFFu - start_ms + now + 1);
}
