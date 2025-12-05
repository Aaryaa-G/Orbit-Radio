/* afsk.h
 * AFSK1200 generator header
 * VERSION 2
 */

#ifndef AFSK_H
#define AFSK_H

#include <stdint.h>

/* Initialize the AFSK module - call once at startup */
void afsk_Init(void);

/* Generate AFSK bit stream from AX.25 frame data
 * The frame should NOT include flag bytes (0x7E) - this function adds them.
 * Frame format: [addresses][control][PID][payload][FCS]
 */
void afsk_generate(const uint8_t *frame, uint16_t frame_len);

/* Start the AFSK transmission (enables timer output) */
void afsk_start(void);

/* Stop the AFSK transmission */
void afsk_stop(void);

/* Called by timer ISR at 9600 Hz sample rate */
void afsk_timer_tick(void);

/* Check if transmission is still in progress */
uint8_t afsk_isBusy(void);

/* Get number of bits remaining in FIFO (for debugging) */
uint32_t afsk_getBitsRemaining(void);

#endif /* AFSK_H */
