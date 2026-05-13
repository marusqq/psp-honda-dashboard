#include <string.h>
#include "obd/elm327.h"
#include "utils/log.h"
#include "utils/time.h"

static void strip_response(char *buf) {
    /* Remove trailing '>', spaces, CR, LF */
    int len = (int)strlen(buf);
    while (len > 0 && (buf[len-1] == '>' || buf[len-1] == ' ' ||
                       buf[len-1] == '\r' || buf[len-1] == '\n'))
        buf[--len] = '\0';
}

int elm327_send_cmd(TcpSocket *sock, const char *cmd, char *resp, int resp_max, int timeout_ms) {
    char buf[ELM327_CMD_MAX + 2];
    int  cmd_len = (int)strlen(cmd);

    if (cmd_len >= ELM327_CMD_MAX)
        return -1;

    memcpy(buf, cmd, (size_t)cmd_len);
    buf[cmd_len]     = '\r';
    buf[cmd_len + 1] = '\0';

    if (socket_send(sock, buf, cmd_len + 1) <= 0)
        return -1;

    int total = 0;
    uint32_t start = time_now_ms();

    while (total < resp_max - 1) {
        if (time_elapsed_ms(start) > (uint32_t)timeout_ms)
            break;

        char tmp[64];
        int n = socket_recv(sock, tmp, sizeof(tmp) - 1, 50);
        if (n <= 0)
            continue;

        tmp[n] = '\0';
        int copy = (total + n < resp_max - 1) ? n : (resp_max - 1 - total);
        memcpy(resp + total, tmp, (size_t)copy);
        total += copy;
        resp[total] = '\0';

        /* ELM327 ends response with '>' prompt */
        if (memchr(resp, '>', (size_t)total))
            break;
    }

    strip_response(resp);
    LOG_D("ELM cmd='%s' resp='%s'", cmd, resp);
    return total;
}

static int send_at(TcpSocket *sock, const char *cmd) {
    char resp[ELM327_RESP_MAX];
    elm327_send_cmd(sock, cmd, resp, sizeof(resp), 1000);
    /* "OK" response means success; other responses (e.g. version string) are also fine */
    return 0;
}

int elm327_init(TcpSocket *sock) {
    LOG_I("ELM327 init sequence");

    /* Full reset - takes up to 1 second for ELM to respond */
    char resp[ELM327_RESP_MAX];
    elm327_send_cmd(sock, "ATZ", resp, sizeof(resp), 2000);
    time_sleep_ms(500);

    send_at(sock, "ATE0");  /* echo off */
    send_at(sock, "ATL0");  /* linefeeds off */
    send_at(sock, "ATS0");  /* spaces off in responses */
    send_at(sock, "ATH0");  /* headers off */
    send_at(sock, "ATAT1"); /* adaptive timing mode 1 */
    send_at(sock, "ATSP0"); /* auto-detect protocol */

    LOG_I("ELM327 init done");
    return 0;
}

int elm327_query_pid(TcpSocket *sock, const char *pid_cmd, char *resp, int resp_max) {
    return elm327_send_cmd(sock, pid_cmd, resp, resp_max, SOCKET_TIMEOUT_MS);
}
