#include "aprs.h"
#include "ax25.h"
#include <string.h>

/* Definitions */
char src_call[10]   = "ORBITR";
uint8_t src_ssid    = 5;

char dst_call[10]   = "VU2CWN";
uint8_t dst_ssid    = 0;

char path1_call[10] = "";
uint8_t path1_ssid  = 0;

char path2_call[10] = "";
uint8_t path2_ssid  = 0;

void APRS_SetSource(const char *call, uint8_t ssid) {
    strncpy(src_call, call, sizeof(src_call)-1);
    src_call[sizeof(src_call)-1]=0;
    src_ssid = ssid;
}
void APRS_SetDestination(const char *call, uint8_t ssid) {
    strncpy(dst_call, call, sizeof(dst_call)-1);
    dst_call[sizeof(dst_call)-1]=0;
    dst_ssid = ssid;
}
void APRS_SetPath1(const char *call, uint8_t ssid) {
    strncpy(path1_call, call, sizeof(path1_call)-1);
    path1_call[sizeof(path1_call)-1]=0;
    path1_ssid = ssid;
}
void APRS_SetPath2(const char *call, uint8_t ssid) {
    strncpy(path2_call, call, sizeof(path2_call)-1);
    path2_call[sizeof(path2_call)-1]=0;
    path2_ssid = ssid;
}

/* Build AX.25 header, payload and hand to AX25 module */
void APRS_Send(const char *msg) {
    AX25_Init();

    AX25_SetDestination(dst_call, dst_ssid);
    AX25_SetSource(src_call, src_ssid);

    if (path1_call[0]) AX25_AddRepeater(path1_call, path1_ssid);
    if (path2_call[0]) AX25_AddRepeater(path2_call, path2_ssid);

    AX25_BeginFrame();
    AX25_SendString(msg);
    AX25_EndFrame();

    /* AX25_EndFrame enqueues CRC, flags into bit FIFO and starts modulation.
       APRS_Send returns immediately, but user code waits by checking AFSK_isBusy() */
}
