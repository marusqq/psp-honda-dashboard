#pragma once
#include "../net/socket.h"

#define ELM327_RESP_MAX 128
#define ELM327_CMD_MAX  32
#define ELM327_PROMPT   '>'

int elm327_init(TcpSocket *sock);
int elm327_init_quick(TcpSocket *sock); /* reconnect: skip ATZ and bus_ready */
int elm327_send_cmd(TcpSocket *sock, const char *cmd, char *resp, int resp_max, int timeout_ms);
int elm327_query_pid(TcpSocket *sock, const char *pid_cmd, char *resp, int resp_max);

/*
 * Probe all known ELM327 WiFi adapter endpoints.
 * On success: sock is left connected, out_ip/out_port filled, returns 0.
 * status/status_max: caller-supplied buffer updated each probe attempt (for UI).
 * Returns -1 if no candidate responds.
 */
int elm327_probe(TcpSocket *sock, char *out_ip, int ip_max, int *out_port,
                 char *status, int status_max);
