#include <string.h>
#include "telemetry/filter.h"

void filter_init(MovingAvg *f) {
    memset(f, 0, sizeof(*f));
}

float filter_update(MovingAvg *f, float val) {
    if (f->count < FILTER_WINDOW) {
        f->buf[f->head] = val;
        f->sum += val;
        f->count++;
    } else {
        f->sum -= f->buf[f->head];
        f->buf[f->head] = val;
        f->sum += val;
    }
    f->head = (f->head + 1) % FILTER_WINDOW;
    return f->sum / (float)f->count;
}

float filter_get(const MovingAvg *f) {
    if (f->count == 0)
        return 0.0f;
    return f->sum / (float)f->count;
}
