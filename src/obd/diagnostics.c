#include <string.h>
#include <stdio.h>
#include "obd/diagnostics.h"
#include "obd/elm327.h"
#include "obd/parser.h"
#include "utils/log.h"

void dtc_format_code(uint8_t b1, uint8_t b2, char *out_6) {
    static const char system_chars[] = "PCBU";
    char sys = system_chars[(b1 >> 6) & 0x03];
    int  num = ((b1 & 0x3F) << 8) | b2;
    snprintf(out_6, DTC_CODE_LEN, "%c%04X", sys, num);
}

int dtc_read(TcpSocket *sock, DtcList *out) {
    char resp[512];
    memset(out, 0, sizeof(*out));

    int n = elm327_send_cmd(sock, "03", resp, sizeof(resp), 2000);
    if (n <= 0)
        return -1;

    if (strstr(resp, "NO DATA") || strstr(resp, "43 00")) {
        out->read_ok = 1;
        return 0;
    }

    uint8_t bytes[64];
    int nb = obd_parse_bytes(resp, bytes, (int)sizeof(bytes));

    /* Response: 43 XX [b1 b2] [b1 b2] ... */
    /* bytes[0] = 0x43, bytes[1] = DTC count */
    if (nb < 2 || bytes[0] != 0x43)
        return -1;

    int dtc_bytes = nb - 2;
    int i = 2;
    while (i + 1 < nb && out->count < DTC_MAX) {
        if (bytes[i] == 0 && bytes[i+1] == 0) {
            i += 2;
            continue;
        }
        dtc_format_code(bytes[i], bytes[i+1], out->codes[out->count]);
        out->count++;
        i += 2;
    }
    (void)dtc_bytes;

    out->read_ok = 1;
    LOG_I("DTC read: %d codes", out->count);
    return 0;
}

int dtc_clear(TcpSocket *sock) {
    char resp[ELM327_RESP_MAX];
    elm327_send_cmd(sock, "04", resp, sizeof(resp), 3000);
    LOG_I("DTC clear sent");
    return 0;
}
