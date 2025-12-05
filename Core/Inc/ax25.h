#ifndef AX25_H
#define AX25_H

#include <stdint.h>

/* Build an AX.25 UI-frame and output bit-packed bytes (LSB-first).
 * out : output byte buffer (must be large enough)
 * len : pointer. On return *len = number of bits written.
 *
 * Addresses are normal ASCII callsign strings (e.g. "VU2CWN").
 * SSID is 0..15.
 *
 * The produced bitstream is:
 *   preamble flags (configurable count)
 *   address bytes (dst, src, paths) (each 7 bytes)
 *   control (0x03), PID (0xF0)
 *   payload bytes (ASCII)
 *   CRC (16-bit, low then high, LSB-first output)
 *   flush
 *   postamble flags
 *
 * The output bit-order is LSB-first per byte. The target AFSK routine
 * should accept bits in this order (NRZI/bit-stuffing is done by this module).
 */
void ax25_encode(uint8_t *out, uint16_t *len,
                 const char *src, uint8_t src_ssid,
                 const char *dst, uint8_t dst_ssid,
                 const char *path1, uint8_t p1s,
                 const char *path2, uint8_t p2s,
                 const char *msg);

/* Convenience: default number of flags before/after frame */
#define AX25_PREAMBLE_FLAGS 10
#define AX25_POSTAMBLE_FLAGS 3

#endif /* AX25_H */
