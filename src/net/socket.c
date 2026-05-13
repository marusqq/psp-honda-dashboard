#include <string.h>
#include <pspnet_inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <unistd.h>
#include "net/socket.h"
#include "utils/log.h"
#include "utils/time.h"

int socket_init(TcpSocket *sock, const char *ip, int port) {
    memset(sock, 0, sizeof(*sock));
    strncpy(sock->ip, ip, sizeof(sock->ip) - 1);
    sock->port = port;
    sock->fd   = -1;
    return 0;
}

int socket_connect(TcpSocket *sock) {
    sock->connected = 0;

    sock->fd = sceNetInetSocket(AF_INET, SOCK_STREAM, 0);
    if (sock->fd < 0) {
        LOG_E("socket() failed: %d", sock->fd);
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = sceNetHtons((unsigned short)sock->port);
    sceNetInetInetAton(sock->ip, &addr.sin_addr);

    LOG_I("Connecting to %s:%d", sock->ip, sock->port);
    int r = sceNetInetConnect(sock->fd, (struct sockaddr *)&addr, sizeof(addr));
    if (r < 0) {
        LOG_E("connect() failed: %d", r);
        sceNetInetClose(sock->fd);
        sock->fd = -1;
        return -1;
    }

    sock->connected = 1;
    LOG_I("Connected to Vgate adapter");
    return 0;
}

int socket_send(TcpSocket *sock, const char *data, int len) {
    if (!sock->connected || sock->fd < 0)
        return -1;

    int sent = (int)sceNetInetSend(sock->fd, data, (size_t)len, 0);
    if (sent < 0) {
        LOG_W("send() failed: %d", sent);
        sock->connected = 0;
    }
    return sent;
}

int socket_recv(TcpSocket *sock, char *buf, int max_len, int timeout_ms) {
    if (!sock->connected || sock->fd < 0)
        return -1;

    SceNetInetFdSet rdset;
    SceNetInetFdZero(&rdset);
    SceNetInetFdSet(sock->fd, &rdset);

    struct SceNetInetTimeval tv;
    tv.tv_sec  = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int ready = sceNetInetSelect(sock->fd + 1, &rdset, NULL, NULL, &tv);
    if (ready <= 0)
        return 0;

    int n = (int)sceNetInetRecv(sock->fd, buf, (size_t)(max_len - 1), 0);
    if (n <= 0) {
        if (n < 0)
            LOG_W("recv() failed: %d", n);
        sock->connected = 0;
        return n;
    }
    buf[n] = '\0';
    return n;
}

int socket_is_connected(TcpSocket *sock) {
    return sock->connected && sock->fd >= 0;
}

void socket_close(TcpSocket *sock) {
    if (sock->fd >= 0) {
        sceNetInetClose(sock->fd);
        sock->fd = -1;
    }
    sock->connected = 0;
}
