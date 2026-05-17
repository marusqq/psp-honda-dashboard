#include <stdio.h>
#include <string.h>
#include "obd/elm327.h"
#include "utils/log.h"
#include "utils/time.h"

static const struct { const char *ip; int port; } g_probe_candidates[] = {
    { "192.168.0.10",  35000 },  /* Vgate iCar 2 default */
    { "192.168.0.10",   3000 },  /* alternate Vgate firmware */
    { "192.168.0.10",     23 },  /* telnet-style adapters */
    { "192.168.1.1",   35000 },  /* some routed/clone adapters */
    { "192.168.1.10",  35000 },
    { "192.168.0.1",   35000 },
    { "10.0.0.1",      35000 },
    { "192.168.4.1",   35000 },  /* ESP8266/ESP32-based adapters */
};
#define PROBE_CANDIDATE_COUNT \
    ((int)(sizeof(g_probe_candidates) / sizeof(g_probe_candidates[0])))

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
    int n = elm327_send_cmd(sock, cmd, resp, sizeof(resp), 1000);
    if (n <= 0)
        LOG_W("send_at '%s': no response (socket may be dead)", cmd);
    else
        LOG_I("send_at '%s' -> '%s'", cmd, resp);
    return 0;
}

/* Send 0100 (supported PID bitmask) until ELM327 has finished bus detection.
   SEARCHING... / STOPPED mean the protocol negotiation is still in progress.
   NO DATA or a valid hex frame means the bus is alive and ready.
   Non-fatal: if it times out we carry on and let the poll loop handle it. */
static void elm327_wait_bus_ready(TcpSocket *sock) {
    uint32_t start    = time_now_ms();
    uint32_t deadline = start + 15000;
    int attempt = 0;
    LOG_I("Waiting for OBD bus ready (max 10s)...");
    while (time_now_ms() < deadline) {
        char resp[ELM327_RESP_MAX];
        elm327_send_cmd(sock, "0100", resp, sizeof(resp), 2000);
        attempt++;
        unsigned elapsed = (unsigned)(time_now_ms() - start);
        if (strstr(resp, "SEARCHING") || strstr(resp, "STOPPED") ||
            strstr(resp, "BUS INIT")  || resp[0] == '\0') {
            LOG_I("bus_ready attempt %d (+%u ms): still negotiating '%s'", attempt, elapsed, resp);
            time_sleep_ms(300);
            continue;
        }
        LOG_I("Bus ready +%u ms (%d attempts): '%s'", elapsed, attempt, resp);
        return;
    }
    LOG_W("elm327_wait_bus_ready: 15s timeout after %d attempts -- poll loop will handle remaining SEARCHING responses", attempt);
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
    send_at(sock, "ATAT2"); /* adaptive timing mode 2 - aggressive timeout reduction */
    send_at(sock, "ATSP3"); /* ISO 9141-2 (pre-CAN Honda/Acura) */

    /* Wait for bus detection to complete before handing off to poll loop */
    elm327_wait_bus_ready(sock);

    LOG_I("ELM327 init done");
    return 0;
}

int elm327_query_pid(TcpSocket *sock, const char *pid_cmd, char *resp, int resp_max) {
    return elm327_send_cmd(sock, pid_cmd, resp, resp_max, SOCKET_TIMEOUT_MS);
}

int elm327_probe(TcpSocket *sock, char *out_ip, int ip_max, int *out_port,
                 char *status, int status_max) {
    LOG_I("ELM327 probe: trying %d candidates", PROBE_CANDIDATE_COUNT);

    for (int i = 0; i < PROBE_CANDIDATE_COUNT; i++) {
        const char *ip   = g_probe_candidates[i].ip;
        int         port = g_probe_candidates[i].port;

        if (status && status_max > 0)
            snprintf(status, (size_t)status_max,
                     "Probing %s:%d  (%d/%d)...", ip, port,
                     i + 1, PROBE_CANDIDATE_COUNT);

        LOG_I("Probe %d/%d: %s:%d", i + 1, PROBE_CANDIDATE_COUNT, ip, port);

        socket_init(sock, ip, port);
        if (socket_connect(sock) != 0) {
            LOG_I("Probe %s:%d - no connection", ip, port);
            socket_close(sock);
            continue;
        }

        /* Flush stale bytes left in adapter TCP buffer from a previous session.
           Send a bare CR to terminate any partial command, then drain the reply. */
        {
            char flush[64];
            int flushed = 0;
            socket_send(sock, "\r", 1);
            time_sleep_ms(60);
            int fn;
            while ((fn = socket_recv(sock, flush, sizeof(flush) - 1, 20)) > 0)
                flushed += fn;
            if (flushed > 0)
                LOG_D("Probe %s:%d - flushed %d stale bytes", ip, port, flushed);
        }

        /* Confirm it's an ELM327-compatible adapter */
        char resp[ELM327_RESP_MAX];
        int  n = elm327_send_cmd(sock, "ATI", resp, sizeof(resp), 2000);
        if (n > 0 && resp[0] != '\0') {
            /* Sanity: ELM327 ATI always contains "ELM" */
            if (!strstr(resp, "ELM") && !strstr(resp, "elm"))
                LOG_W("Probe %s:%d - ATI='%s' looks garbled (missing ELM)", ip, port, resp);
            LOG_I("Probe found adapter at %s:%d  ATI='%s'", ip, port, resp);
            if (out_ip)   { strncpy(out_ip, ip, (size_t)(ip_max - 1)); out_ip[ip_max - 1] = '\0'; }
            if (out_port) *out_port = port;
            if (status && status_max > 0)
                snprintf(status, (size_t)status_max, "Found: %s:%d  %s", ip, port, resp);
            return 0;
        }

        LOG_W("Probe %s:%d - ATI returned empty/no response (n=%d), skipping", ip, port, n);
        socket_close(sock);
    }

    LOG_E("Probe: no adapter found on any candidate");
    if (status && status_max > 0)
        snprintf(status, (size_t)status_max, "No OBD adapter found (%d addresses tried)",
                 PROBE_CANDIDATE_COUNT);
    return -1;
}
