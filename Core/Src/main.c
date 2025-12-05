/* main.c
 * RS485 -> APRS (AX.25) -> AFSK1200 -> DRA818U
 * VERSION 2 - Fixed PA15 JTAG issue, optimized DAC, added APRS DTI
 */

#include "main.h"
#include "afsk.h"
#include "ax25.h"

#include <string.h>
#include <stdio.h>
#include <stdint.h>

/* Hardware handles */
UART_HandleTypeDef huart1; /* RS-485 half duplex (USART1) */
UART_HandleTypeDef huart2; /* Debug (USART2) */
UART_HandleTypeDef huart6; /* DRA818U (USART6) */
TIM_HandleTypeDef  htim3;  /* sample timer */

/* APRS config */
static const char SRC_CALL[]  = "VU3LTQ";
static const uint8_t SRC_SSID = 5;
static const char DST_CALL[]  = "VU2CWN";
static const uint8_t DST_SSID = 0;
static const char PATH1_CALL[] = "WIDE1";
static const uint8_t PATH1_SSID = 1;
static const char PATH2_CALL[] = "WIDE2";
static const uint8_t PATH2_SSID = 1;

/* buffers */
#define AX25_BUF_SIZE 4096
static uint8_t ax25_buffer[AX25_BUF_SIZE];
static uint16_t ax25_len = 0;
static char rs485_msg[LINE_BUF_SIZE];
static uint16_t rs485_len = 0;

/* DAC pin masks - precomputed for fast atomic writes */
static uint32_t dac_set_masks[16];
static uint32_t dac_reset_masks[16];

/* forward declarations */
void SystemClock_Config(void);
void GPIO_Init(void);
void USART2_Init(void);
void USART1_Init(void);
void USART6_Init(void);
void TIM3_Init(void);
void DAC_PrecomputeMasks(void);

static void Debug_Print(const char *s);
static void RS485_SetReceive(void);
static void DRA_Send(const char *s);
static void DRA_Init(void);
void Debug_PrintClocks(void);

/* External function to check if AFSK is still transmitting */
extern uint8_t afsk_isBusy(void);
extern uint32_t afsk_getBitsRemaining(void);

/* Optimized DAC write function using precomputed BSRR masks
 * This sets all 4 bits atomically in a single register write
 *
 * NOTE: This assumes all DAC pins are on GPIOA. If they're on different
 * ports, use the slower individual write version.
 */
void DAC_Write4(uint8_t v)
{
#if 1
    /* FAST VERSION: Atomic write using BSRR (requires all pins on same port) */
    /* BSRR lower 16 bits = set, upper 16 bits = reset */
    GPIOA->BSRR = dac_set_masks[v & 0x0F] | dac_reset_masks[v & 0x0F];
#else
    /* SLOW VERSION: Individual GPIO writes (use if pins on different ports) */
    HAL_GPIO_WritePin(LSB_GPIO_Port, LSB_Pin,   (v>>0) & 1 ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(BIT_1_GPIO_Port, BIT_1_Pin, (v>>1) & 1 ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(BIT_2_GPIO_Port, BIT_2_Pin, (v>>2) & 1 ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(MSB_GPIO_Port, MSB_Pin,   (v>>3) & 1 ? GPIO_PIN_SET : GPIO_PIN_RESET);
#endif
}

/* Precompute BSRR masks for all 16 possible DAC values */
void DAC_PrecomputeMasks(void)
{
    for (uint8_t v = 0; v < 16; v++) {
        uint32_t set_mask = 0;
        uint32_t reset_mask = 0;

        /* LSB (bit 0) */
        if (v & 0x01) set_mask |= LSB_Pin; else reset_mask |= (LSB_Pin << 16);
        /* BIT_1 (bit 1) */
        if (v & 0x02) set_mask |= BIT_1_Pin; else reset_mask |= (BIT_1_Pin << 16);
        /* BIT_2 (bit 2) */
        if (v & 0x04) set_mask |= BIT_2_Pin; else reset_mask |= (BIT_2_Pin << 16);
        /* MSB (bit 3) */
        if (v & 0x08) set_mask |= MSB_Pin; else reset_mask |= (MSB_Pin << 16);

        dac_set_masks[v] = set_mask;
        dac_reset_masks[v] = reset_mask;
    }
}

/* HAL timer callback - calls afsk tick */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM3) {
        afsk_timer_tick();
    }
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    GPIO_Init();
    DAC_PrecomputeMasks();  /* Precompute DAC masks for fast writes */

    USART2_Init(); /* debug */
    USART6_Init(); /* DRA */
    USART1_Init(); /* RS485 half duplex */
    TIM3_Init();   /* sample timer */

    /* init afsk */
    afsk_Init();

    Debug_Print("\r\n=== BeliefSat OrbitRadio-5 APRS MODEM v2 ===\r\n");

    /* Print clock info for debugging */
    Debug_PrintClocks();

    DRA_Init();
    RS485_SetReceive();

    Debug_Print("RS485 listening...\r\n");

    /* main loop: read RS485 line-by-line, form APRS message, encode, generate, transmit */
    for (;;)
    {
        uint8_t b;
        if (HAL_UART_Receive(&huart1, &b, 1, HAL_MAX_DELAY) == HAL_OK)
        {
            HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);

            if (b == '\r') continue;
            if (b == '\n' || rs485_len >= (LINE_BUF_SIZE - 2))
            {
                rs485_msg[rs485_len] = '\0';
                Debug_Print("RS485: ");
                Debug_Print(rs485_msg);
                Debug_Print("\r\n");

                /* Build APRS payload with Data Type Identifier
                 * '>' = Status message (most appropriate for telemetry)
                 * Format: >status text
                 */
                char payload[256];
                snprintf(payload, sizeof(payload), ">%s | Somaiya OrbitRadio-5 73", rs485_msg);

                /* prepare AX.25 frame */
                ax25_len = 0;
                ax25_encode(ax25_buffer, &ax25_len,
                            SRC_CALL, SRC_SSID,
                            DST_CALL, DST_SSID,
                            PATH1_CALL, PATH1_SSID,
                            PATH2_CALL, PATH2_SSID,
                            payload);

                char dbg[80];
                snprintf(dbg, sizeof(dbg), "AX.25 frame: %u bytes (payload: %zu chars)\r\n",
                         ax25_len, strlen(payload));
                Debug_Print(dbg);

                /* Pre-TX delay - system stabilization */
                HAL_Delay(200);

                /* Enable PTT */
                HAL_GPIO_WritePin(PTT_UHF_GPIO_Port, PTT_UHF_Pin, GPIO_PIN_SET);
                Debug_Print("PTT ON\r\n");

                /* TX Delay (TXD) - wait for radio to key up
                 * DRA818U typically needs 300-500ms
                 */
                HAL_Delay(500);

                /* Generate AFSK bit stream from AX.25 frame */
                afsk_generate(ax25_buffer, ax25_len);

                /* Debug: show bit count */
                snprintf(dbg, sizeof(dbg), "AFSK bits queued: %lu\r\n", afsk_getBitsRemaining());
                Debug_Print(dbg);

                /* Start transmission */
                afsk_start();
                Debug_Print("TX started...\r\n");

                /* Wait for transmission to complete */
                uint32_t start_time = HAL_GetTick();
                uint32_t timeout = start_time + 15000;  /* 15 second max */

                while (afsk_isBusy()) {
                    if (HAL_GetTick() > timeout) {
                        Debug_Print("TX timeout!\r\n");
                        break;
                    }
                }

                uint32_t tx_time = HAL_GetTick() - start_time;
                snprintf(dbg, sizeof(dbg), "TX complete: %lu ms\r\n", tx_time);
                Debug_Print(dbg);

                /* Post-TX delay before releasing PTT */
                HAL_Delay(100);

                /* Stop AFSK and release PTT */
                afsk_stop();
                HAL_GPIO_WritePin(PTT_UHF_GPIO_Port, PTT_UHF_Pin, GPIO_PIN_RESET);
                Debug_Print("PTT OFF\r\n\r\n");

                /* Clear receive buffer */
                rs485_len = 0;
                memset(rs485_msg, 0, sizeof(rs485_msg));
            }
            else
            {
                rs485_msg[rs485_len++] = (char)b;
            }
        }
    }
}

/* System Clock config: use HSI 16 MHz, no PLL */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState = RCC_PLL_NONE;
    HAL_RCC_OscConfig(&osc);

    clk.ClockType = RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV1;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;

    HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_0);
}

void Debug_PrintClocks(void)
{
    char buf[100];
    uint32_t sysclk = HAL_RCC_GetSysClockFreq();
    uint32_t hclk = HAL_RCC_GetHCLKFreq();
    uint32_t pclk1 = HAL_RCC_GetPCLK1Freq();

    uint32_t tim_clk = pclk1;
    if ((RCC->CFGR & RCC_CFGR_PPRE1) != RCC_CFGR_PPRE1_DIV1) {
        tim_clk = pclk1 * 2;
    }

    snprintf(buf, sizeof(buf), "SYSCLK: %lu Hz\r\n", sysclk);
    Debug_Print(buf);
    snprintf(buf, sizeof(buf), "HCLK: %lu Hz\r\n", hclk);
    Debug_Print(buf);
    snprintf(buf, sizeof(buf), "PCLK1: %lu Hz, TIM3 clk: %lu Hz\r\n", pclk1, tim_clk);
    Debug_Print(buf);

    uint32_t period = TIM3->ARR + 1;
    uint32_t actual_rate = tim_clk / period;
    snprintf(buf, sizeof(buf), "TIM3 ARR: %lu, Sample rate: %lu Hz\r\n", TIM3->ARR, actual_rate);
    Debug_Print(buf);
}

/* GPIO init - WITH PA15 JTAG RELEASE */
void GPIO_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* NOTE: PA15 is JTDI by default on STM32F4
     * On STM32F4, simply configuring PA15 as GPIO output will release it from JTAG
     * SWD (PA13/PA14) remains available for debugging
     * No special AFIO remapping needed (that's STM32F1 only)
     */

    GPIO_InitTypeDef g = {0};

    /* RS485 RE/DE (PC0, PC2) */
    g.Pin = RS485_RE_Pin | RS485_DE_Pin;
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(RS485_RE_GPIO_Port, &g);
    HAL_GPIO_WritePin(RS485_RE_GPIO_Port, RS485_RE_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(RS485_DE_GPIO_Port, RS485_DE_Pin, GPIO_PIN_RESET);

    /* DAC bits - Configure with HIGH speed for clean waveforms
     * IMPORTANT: Verify these pins in main.h:
     * - LSB_Pin   = PA? (bit 0, smallest weight)
     * - BIT_1_Pin = PA? (bit 1)
     * - BIT_2_Pin = PA? (bit 2)
     * - MSB_Pin   = PA? (bit 3, largest weight)
     *
     * If using PA15, it should be automatically released from JTAG
     * when configured as GPIO output on STM32F4
     */
    g.Pin = LSB_Pin | BIT_1_Pin | BIT_2_Pin | MSB_Pin;
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;  /* Fastest for clean transitions */
    HAL_GPIO_Init(LSB_GPIO_Port, &g);

    /* Initialize DAC to mid-level (8) */
    HAL_GPIO_WritePin(LSB_GPIO_Port, LSB_Pin, GPIO_PIN_RESET);      /* bit 0 = 0 */
    HAL_GPIO_WritePin(BIT_1_GPIO_Port, BIT_1_Pin, GPIO_PIN_RESET);  /* bit 1 = 0 */
    HAL_GPIO_WritePin(BIT_2_GPIO_Port, BIT_2_Pin, GPIO_PIN_RESET);  /* bit 2 = 0 */
    HAL_GPIO_WritePin(MSB_GPIO_Port, MSB_Pin, GPIO_PIN_SET);        /* bit 3 = 1 */
    /* This sets DAC to 8 (0b1000) = mid-level */

    /* PTT (PC9) */
    g.Pin = PTT_UHF_Pin;
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(PTT_UHF_GPIO_Port, &g);
    HAL_GPIO_WritePin(PTT_UHF_GPIO_Port, PTT_UHF_Pin, GPIO_PIN_RESET);

    /* LD2 */
    g.Pin = LD2_Pin;
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LD2_GPIO_Port, &g);
    HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);
}

/* USART2 debug init (PA2 TX, PA3 RX) */
void USART2_Init(void)
{
    __HAL_RCC_USART2_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};
    g.Pin = USART_TX_Pin | USART_RX_Pin;
    g.Mode = GPIO_MODE_AF_PP;
    g.Pull = GPIO_PULLUP;
    g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    g.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(USART_TX_GPIO_Port, &g);

    huart2.Instance = USART2;
    huart2.Init.BaudRate = 115200;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    HAL_UART_Init(&huart2);
}

/* USART1 half-duplex (PA9) */
void USART1_Init(void)
{
    __HAL_RCC_USART1_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};
    g.Pin = USART1_RXTX_Pin;
    g.Mode = GPIO_MODE_AF_PP;
    g.Pull = GPIO_PULLUP;
    g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    g.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(USART1_RXTX_GPIO_Port, &g);

    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    HAL_HalfDuplex_Init(&huart1);
}

/* USART6 for DRA818U (PC6 TX, PC7 RX) */
void USART6_Init(void)
{
    __HAL_RCC_USART6_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};
    g.Pin = USART6_TX_Pin | USART6_RX_Pin;
    g.Mode = GPIO_MODE_AF_PP;
    g.Pull = GPIO_PULLUP;
    g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    g.Alternate = GPIO_AF8_USART6;
    HAL_GPIO_Init(USART6_TX_GPIO_Port, &g);

    huart6.Instance = USART6;
    huart6.Init.BaudRate = 9600;
    huart6.Init.WordLength = UART_WORDLENGTH_8B;
    huart6.Init.StopBits = UART_STOPBITS_1;
    huart6.Init.Parity = UART_PARITY_NONE;
    huart6.Init.Mode = UART_MODE_TX_RX;
    huart6.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    HAL_UART_Init(&huart6);
}

/* TIM3 init: 9600 Hz sample rate for AFSK1200 */
void TIM3_Init(void)
{
    __HAL_RCC_TIM3_CLK_ENABLE();

    htim3.Instance = TIM3;

    uint32_t pclk1 = HAL_RCC_GetPCLK1Freq();
    uint32_t tim_clk = pclk1;
    if ((RCC->CFGR & RCC_CFGR_PPRE1) != RCC_CFGR_PPRE1_DIV1) {
        tim_clk = pclk1 * 2;
    }

    /* Calculate period for 9600 Hz with rounding */
    uint32_t period = (tim_clk + 4800) / 9600;
    if (period < 1) period = 1;

    htim3.Init.Prescaler = 0;
    htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim3.Init.Period = period - 1;
    htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    HAL_TIM_Base_Init(&htim3);

    __HAL_TIM_CLEAR_FLAG(&htim3, TIM_FLAG_UPDATE);

    HAL_TIM_Base_Start_IT(&htim3);
    HAL_NVIC_SetPriority(TIM3_IRQn, 0, 0);  /* Highest priority */
    HAL_NVIC_EnableIRQ(TIM3_IRQn);
}

/* RS485 receive mode */
static void RS485_SetReceive(void)
{
    HAL_GPIO_WritePin(RS485_RE_GPIO_Port, RS485_RE_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(RS485_DE_GPIO_Port, RS485_DE_Pin, GPIO_PIN_RESET);
}

/* Debug print */
static void Debug_Print(const char *s)
{
    HAL_UART_Transmit(&huart2, (uint8_t*)s, strlen(s), HAL_MAX_DELAY);
}

/* DRA818U helpers */
void DRA_Send(const char *s)
{
    HAL_UART_Transmit(&huart6, (uint8_t*)s, strlen(s), HAL_MAX_DELAY);
    HAL_UART_Transmit(&huart6, (uint8_t*)"\r\n", 2, HAL_MAX_DELAY);
}

void DRA_Init(void)
{
    Debug_Print("Configuring DRA818U...\r\n");
    HAL_Delay(500);

    DRA_Send("AT+DMOCONNECT");
    HAL_Delay(300);

    /* 435.2480 MHz, no CTCSS, squelch 0 */
    DRA_Send("AT+DMOSETGROUP=0,435.2480,435.2480,0000,0,0000");
    HAL_Delay(300);

    DRA_Send("AT+DMOSETVOLUME=8");
    HAL_Delay(200);

    Debug_Print("DRA818U @ 435.2480 MHz ready\r\n");
}

/* Error handler */
void Error_Handler(void)
{
    while (1) {
        HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
        HAL_Delay(200);
    }
}
