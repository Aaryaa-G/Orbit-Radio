#include "afsk.h"

/* Compatibility wrapper for older uppercase name used by ax25.c */
void AFSK_EnqueueBit(uint8_t b)
{
    afsk_EnqueueBit(b);
}
