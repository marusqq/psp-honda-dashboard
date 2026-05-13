#pragma once

#define FILTER_WINDOW 4

typedef struct {
    float buf[FILTER_WINDOW];
    int   head;
    int   count;
    float sum;
} MovingAvg;

void  filter_init(MovingAvg *f);
float filter_update(MovingAvg *f, float val);
float filter_get(const MovingAvg *f);
