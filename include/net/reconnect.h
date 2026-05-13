#pragma once
#include <stdint.h>
#include "socket.h"

#define RECONNECT_BASE_DELAY_MS 1000
#define RECONNECT_MAX_DELAY_MS  30000

typedef struct {
    int      attempt;
    uint32_t delay_ms;
    uint32_t next_try_ms;
} ReconnectState;

void reconnect_reset(ReconnectState *rs);
int  reconnect_should_try(const ReconnectState *rs, uint32_t now_ms);
int  reconnect_try(ReconnectState *rs, TcpSocket *sock);
void reconnect_on_success(ReconnectState *rs);
