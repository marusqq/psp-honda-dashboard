#pragma once
#include "../net/socket.h"

#define ELM327_RESP_MAX 128
#define ELM327_CMD_MAX  32
#define ELM327_PROMPT   '>'

int elm327_init(TcpSocket *sock);
int elm327_send_cmd(TcpSocket *sock, const char *cmd, char *resp, int resp_max, int timeout_ms);
int elm327_query_pid(TcpSocket *sock, const char *pid_cmd, char *resp, int resp_max);
