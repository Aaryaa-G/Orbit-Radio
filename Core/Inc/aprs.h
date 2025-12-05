#ifndef APRS_H
#define APRS_H

#include <stdint.h>

/* ================= EXTERNAL VARIABLES ================= */
extern char src_call[10];
extern uint8_t src_ssid;

extern char dst_call[10];
extern uint8_t dst_ssid;

extern char path1_call[10];
extern uint8_t path1_ssid;

extern char path2_call[10];
extern uint8_t path2_ssid;

/* ================= FUNCTION PROTOTYPES ================= */
void APRS_SetSource(const char *call, uint8_t ssid);
void APRS_SetDestination(const char *call, uint8_t ssid);
void APRS_SetPath1(const char *call, uint8_t ssid);
void APRS_SetPath2(const char *call, uint8_t ssid);

void APRS_Send(const char *msg);

#endif
