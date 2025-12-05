/* afsk.c
 * AFSK1200 generator: bit-stuffing, NRZI, timer-driven sample output.
 * VERSION 2 - Optimized DAC writes, verified logic for Direwolf compatibility
 */

#include "afsk.h"
#include "main.h"
#include <string.h>
#include <stdint.h>

/* FIFO for bits (simple circular buffer) */
#define AFSK_FIFO_SIZE  (8192)
static volatile uint8_t afsk_fifo[AFSK_FIFO_SIZE];
static volatile uint32_t fifo_head = 0, fifo_tail = 0, fifo_count = 0;

/* NRZI / sample state
 * CRITICAL: AX.25 idles at MARK (1200 Hz), so initial state must be 1
 */
static volatile uint8_t nrzi_tone_state = 1;  /* 1 = MARK (1200 Hz) */
static volatile uint8_t samples_left_for_bit = 0;
static volatile uint8_t afsk_running = 0;

/* 16-entry 4-bit sine table (values 0-15, centered at 8)
 * Standard symmetric sine lookup for AFSK
 */
static const uint8_t sine16[16] = {
     8, 11, 13, 14, 15, 14, 13, 11,
     8,  5,  2,  1,  0,  1,  2,  5
};

/* sample rate and tone parameters */
#define SAMPLE_RATE  9600U
#define BAUD         1200U
#define SAMPLES_PER_BIT  (SAMPLE_RATE / BAUD)  /* = 8 */

/* Phase accumulator (16-bit fixed point for precision) */
static volatile uint16_t phase_acc = 0;

/* Phase increments: one complete sine cycle = 4096 phase units (16 entries * 256)
 * Mark 1200 Hz: (1200 * 4096) / 9600 = 512
 * Space 2200 Hz: (2200 * 4096) / 9600 = 938.67 ≈ 939
 */
#define PHASE_INC_MARK   512
#define PHASE_INC_SPACE  939   /* Slightly more accurate than 938 */

static volatile uint16_t current_phase_inc = PHASE_INC_MARK;

/* Bit stuffing counter - must be reset before each frame */
static uint8_t consecutive_ones = 0;

/* External DAC write function */
extern void DAC_Write4(uint8_t v);

/* Enqueue single bit into FIFO (called from main context only) */
static int afsk_EnqueueBit(uint8_t bit)
{
    if (fifo_count >= AFSK_FIFO_SIZE) {
        return -1;  /* full */
    }
    afsk_fifo[fifo_head] = (bit & 1);
    fifo_head = (fifo_head + 1) % AFSK_FIFO_SIZE;
    fifo_count++;
    return 0;
}

/* Dequeue bit (called from ISR context only); returns -1 if empty */
static int afsk_DequeueBit(void)
{
    if (fifo_count == 0) {
        return -1;
    }
    uint8_t val = afsk_fifo[fifo_tail];
    fifo_tail = (fifo_tail + 1) % AFSK_FIFO_SIZE;
    fifo_count--;
    return val;
}

/* Initialize AFSK internals */
void afsk_Init(void)
{
    fifo_head = 0;
    fifo_tail = 0;
    fifo_count = 0;
    nrzi_tone_state = 1;       /* CRITICAL: Start at MARK (1200 Hz) */
    samples_left_for_bit = 0;
    afsk_running = 0;
    phase_acc = 0;
    current_phase_inc = PHASE_INC_MARK;
    consecutive_ones = 0;
}

/* Send a single byte as bits (LSB first), NO bit stuffing
 * Used for flag bytes (0x7E)
 */
static void send_byte_raw(uint8_t byte)
{
    for (uint8_t i = 0; i < 8; i++) {
        uint8_t bit = (byte >> i) & 1;
        while (afsk_EnqueueBit(bit) != 0) {
            /* FIFO full - shouldn't happen with 8K FIFO */
        }
    }
}

/* Send a single byte with bit stuffing (LSB first)
 * After 5 consecutive 1s, insert a 0 bit
 */
static void send_byte_stuffed(uint8_t byte)
{
    for (uint8_t i = 0; i < 8; i++) {
        uint8_t bit = (byte >> i) & 1;

        while (afsk_EnqueueBit(bit) != 0) {
            /* FIFO full - wait */
        }

        if (bit == 1) {
            consecutive_ones++;
            if (consecutive_ones == 5) {
                /* Stuff a zero bit to prevent false flag detection */
                while (afsk_EnqueueBit(0) != 0) {
                    /* wait */
                }
                consecutive_ones = 0;
            }
        } else {
            consecutive_ones = 0;
        }
    }
}

/* afsk_generate:
 * Takes raw frame data (WITHOUT flags) and generates the complete
 * AFSK bit stream with preamble, bit stuffing, and tail flags.
 *
 * Frame format expected: [address fields][control][PID][payload][FCS]
 * This function adds: [preamble flags][frame with stuffing][tail flags]
 */
void afsk_generate(const uint8_t *frame, uint16_t frame_len)
{
    if (!frame || frame_len == 0) return;

    /* ===== CRITICAL: RESET ALL STATE BEFORE EACH TRANSMISSION ===== */

    /* Reset FIFO pointers */
    fifo_head = 0;
    fifo_tail = 0;
    fifo_count = 0;

    /* Reset NRZI state to MARK (1200 Hz) - AX.25 idle state */
    nrzi_tone_state = 1;
    current_phase_inc = PHASE_INC_MARK;

    /* Reset phase accumulator for clean waveform start */
    phase_acc = 0;

    /* Reset sample counter */
    samples_left_for_bit = 0;

    /* Reset bit stuffing counter */
    consecutive_ones = 0;

    /* ===== BUILD THE BIT STREAM ===== */

    /* 1. PREAMBLE: Send flag bytes (0x7E) WITHOUT bit stuffing
     *    50 flags = 400 bits = 333ms at 1200 baud
     *    This gives receivers time to synchronize
     */
    for (int i = 0; i < 50; i++) {
        send_byte_raw(0x7E);
    }

    /* 2. FRAME DATA: Send WITH bit stuffing
     *    Skip any flags that might be in the input (defensive)
     */
    uint16_t start = 0;
    uint16_t end = frame_len;

    while (start < end && frame[start] == 0x7E) {
        start++;
    }
    while (end > start && frame[end - 1] == 0x7E) {
        end--;
    }

    /* Reset stuffing counter before data */
    consecutive_ones = 0;

    /* Send each data byte with bit stuffing */
    for (uint16_t i = start; i < end; i++) {
        send_byte_stuffed(frame[i]);
    }

    /* 3. TAIL: Send closing flag bytes WITHOUT bit stuffing
     *    3 flags ensures clean frame termination
     */
    for (int i = 0; i < 3; i++) {
        send_byte_raw(0x7E);
    }
}

/* Start AFSK transmission */
void afsk_start(void)
{
    afsk_running = 1;
}

/* Stop AFSK transmission */
void afsk_stop(void)
{
    afsk_running = 0;
    DAC_Write4(8);  /* Return to mid-level (DC bias point) */
}

/* afsk_timer_tick:
 * Called at SAMPLE_RATE (9600 Hz) from timer ISR.
 * Produces one 4-bit DAC sample per call.
 * Implements NRZI encoding: 0 bit = toggle tone, 1 bit = same tone
 */
void afsk_timer_tick(void)
{
    if (!afsk_running) {
        DAC_Write4(8);  /* Mid-level when idle */
        return;
    }

    /* Check if we need a new bit */
    if (samples_left_for_bit == 0) {
        int nextbit = afsk_DequeueBit();

        if (nextbit < 0) {
            /* No more bits - transmission complete */
            DAC_Write4(8);
            afsk_running = 0;
            return;
        }

        /* NRZI encoding:
         * - Bit 1: NO change in tone (stay on current frequency)
         * - Bit 0: TOGGLE tone (switch between MARK and SPACE)
         *
         * This means continuous 1s = continuous MARK tone
         * Flag 0x7E (01111110) produces the characteristic warble
         */
        if (nextbit == 0) {
            nrzi_tone_state = !nrzi_tone_state;
        }
        /* else: bit is 1, keep same tone */

        /* Update phase increment for the (possibly new) tone */
        current_phase_inc = nrzi_tone_state ? PHASE_INC_MARK : PHASE_INC_SPACE;

        /* Reset sample counter for this bit */
        samples_left_for_bit = SAMPLES_PER_BIT;
    }

    /* Generate sine wave sample using DDS (Direct Digital Synthesis)
     * phase_acc upper 4 bits (bits 8-11) index into 16-entry sine table
     */
    uint8_t table_index = (phase_acc >> 8) & 0x0F;
    DAC_Write4(sine16[table_index]);

    /* Advance phase accumulator
     * This maintains phase continuity when switching tones
     */
    phase_acc += current_phase_inc;

    samples_left_for_bit--;
}

/* Check if transmission is still in progress */
uint8_t afsk_isBusy(void)
{
    return afsk_running || (fifo_count > 0);
}

/* Get number of bits remaining in FIFO (for debugging) */
uint32_t afsk_getBitsRemaining(void)
{
    return fifo_count;
}
