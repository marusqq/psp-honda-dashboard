#include <string.h>
#include "net/reconnect.h"
#include "utils/log.h"
#include "utils/time.h"

void reconnect_reset(ReconnectState *rs) {
    memset(rs, 0, sizeof(*rs));
    rs->delay_ms    = RECONNECT_BASE_DELAY_MS;
    rs->next_try_ms = 0;
}

int reconnect_should_try(const ReconnectState *rs, uint32_t now_ms) {
    return now_ms >= rs->next_try_ms;
}

int reconnect_try(ReconnectState *rs, TcpSocket *sock) {
    LOG_I("Reconnect attempt %d (delay %u ms)", rs->attempt + 1, rs->delay_ms);

    socket_close(sock);
    int result = socket_connect(sock);

    rs->attempt++;
    /* Exponential backoff capped at max delay */
    rs->delay_ms *= 2;
    if (rs->delay_ms > RECONNECT_MAX_DELAY_MS)
        rs->delay_ms = RECONNECT_MAX_DELAY_MS;

    rs->next_try_ms = time_now_ms() + rs->delay_ms;
    return result;
}

void reconnect_on_success(ReconnectState *rs) {
    LOG_I("Reconnect success after %d attempts", rs->attempt);
    rs->attempt  = 0;
    rs->delay_ms = RECONNECT_BASE_DELAY_MS;
}
