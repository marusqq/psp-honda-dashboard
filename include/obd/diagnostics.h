#pragma once
#include "../net/socket.h"

#define DTC_MAX      16
#define DTC_CODE_LEN 6

typedef struct {
    char codes[DTC_MAX][DTC_CODE_LEN];
    int  count;
    int  read_ok;
} DtcList;

int  dtc_read(TcpSocket *sock, DtcList *out);
int  dtc_clear(TcpSocket *sock);
void dtc_format_code(uint8_t b1, uint8_t b2, char *out_6);
