#pragma once
#include <stdint.h>

#define VGATE_DEFAULT_IP    "192.168.0.10"
#define VGATE_DEFAULT_PORT  35000
#define SOCKET_BUF_SIZE     256
#define SOCKET_TIMEOUT_MS   400

typedef struct {
    int  fd;
    int  connected;
    char ip[16];
    int  port;
} TcpSocket;

int  socket_init(TcpSocket *sock, const char *ip, int port);
int  socket_connect(TcpSocket *sock);
int  socket_send(TcpSocket *sock, const char *data, int len);
int  socket_recv(TcpSocket *sock, char *buf, int max_len, int timeout_ms);
int  socket_is_connected(TcpSocket *sock);
void socket_close(TcpSocket *sock);
