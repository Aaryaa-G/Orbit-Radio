/* ax25.c
 * Minimal AX.25 encoder - outputs frame WITHOUT flag bytes
 * Flag bytes are added by afsk_generate()
 */

#include "ax25.h"
#include <string.h>
#include <stdint.h>

static void write_callsign(uint8_t *buf, const char *call, uint8_t ssid, uint8_t last)
{
    char tmp[7];
    memset(tmp, ' ', 6);
    tmp[6] = 0;

    if (call) {
        size_t l = strlen(call);
        if (l > 6) l = 6;
        for (size_t i = 0; i < l; ++i) {
            /* Convert to uppercase for consistency */
            char c = call[i];
            if (c >= 'a' && c <= 'z') c -= 32;
            tmp[i] = c;
        }
    }

    for (int i = 0; i < 6; ++i) {
        buf[i] = ((uint8_t)tmp[i]) << 1;
    }

    uint8_t ssid_byte = (uint8_t)((ssid & 0x0F) << 1);
    ssid_byte |= 0x60;  /* Reserved bits set */
    if (last) ssid_byte |= 0x01;
    buf[6] = ssid_byte;
}

static uint16_t crc_ccitt_update(uint16_t crc, uint8_t data)
{
    crc ^= (uint16_t)data;
    for (int i = 0; i < 8; ++i) {
        if (crc & 1)
            crc = (crc >> 1) ^ 0x8408;
        else
            crc = (crc >> 1);
    }
    return crc;
}

void ax25_encode(uint8_t *out, uint16_t *len,
                 const char *src, uint8_t src_ssid,
                 const char *dst, uint8_t dst_ssid,
                 const char *p1, uint8_t p1s,
                 const char *p2, uint8_t p2s,
                 const char *msg)
{
    if (!out || !len) return;

    uint16_t idx = 0;

    /* NO start flag - afsk_generate adds preamble */

    uint8_t have_p1 = (p1 && p1[0]);
    uint8_t have_p2 = (p2 && p2[0]);

    /* Destination */
    write_callsign(&out[idx], dst, dst_ssid, 0);
    idx += 7;

    /* Source */
    write_callsign(&out[idx], src, src_ssid, (!have_p1 && !have_p2));
    idx += 7;

    /* Path 1 */
    if (have_p1) {
        write_callsign(&out[idx], p1, p1s, !have_p2);
        idx += 7;
    }

    /* Path 2 */
    if (have_p2) {
        write_callsign(&out[idx], p2, p2s, 1);
        idx += 7;
    }

    /* Control and PID */
    out[idx++] = 0x03;  /* UI frame */
    out[idx++] = 0xF0;  /* No layer 3 */

    /* Payload */
    if (msg) {
        size_t mlen = strlen(msg);
        for (size_t i = 0; i < mlen; ++i) {
            out[idx++] = (uint8_t)msg[i];
        }
    }

    /* Compute CRC over all data so far */
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < idx; ++i) {
        crc = crc_ccitt_update(crc, out[i]);
    }
    crc = ~crc;

    /* Append FCS (LSB first) */
    out[idx++] = (uint8_t)(crc & 0xFF);
    out[idx++] = (uint8_t)((crc >> 8) & 0xFF);

    /* NO trailing flag - afsk_generate adds tail */

    *len = idx;
}
