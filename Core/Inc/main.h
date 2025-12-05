/* ============================================================================
 *  BeliefSat APRS Modem - main.h
 *  STM32F446RE
 *  RS-485 + AFSK1200 + AX.25 + DRA818U (435.2480 MHz)
 * ============================================================================
 */

#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"

/* ===================== RS-485 (ADM2582E) ===================== */
/* RE = PC0  (LOW  = Receive)
 * DE = PC2  (LOW  = Receive / HIGH = Transmit)
 */
#define RS485_RE_Pin          GPIO_PIN_0
#define RS485_RE_GPIO_Port    GPIOC

#define RS485_DE_Pin          GPIO_PIN_2
#define RS485_DE_GPIO_Port    GPIOC

/* ===================== 4-bit DAC (AFSK Output) ===================== */
/* LSB = PA15   (bit0)
 * BIT_1 = PA1  (bit1)
 * BIT_2 = PA4  (bit2)
 * MSB = PA6    (bit3)
 */
#define LSB_Pin               GPIO_PIN_15
#define LSB_GPIO_Port         GPIOA

#define BIT_1_Pin             GPIO_PIN_1
#define BIT_1_GPIO_Port       GPIOA

#define BIT_2_Pin             GPIO_PIN_4
#define BIT_2_GPIO_Port       GPIOA

#define MSB_Pin               GPIO_PIN_6
#define MSB_GPIO_Port         GPIOA

/* ===================== PTT (DRA818U) ===================== */
/* PC9 = PTT_OUT → (NOT Gate) → DRA818 PTT
 * MCU HIGH = TX enabled
 */
#define PTT_UHF_Pin           GPIO_PIN_9
#define PTT_UHF_GPIO_Port     GPIOC

/* ===================== Debug LED ===================== */
#define LD2_Pin               GPIO_PIN_5
#define LD2_GPIO_Port         GPIOA

/* ===================== USART2 (Debug UART) ===================== */
/* PA2 = TX, PA3 = RX */
#define USART_TX_Pin          GPIO_PIN_2
#define USART_TX_GPIO_Port    GPIOA

#define USART_RX_Pin          GPIO_PIN_3
#define USART_RX_GPIO_Port    GPIOA

/* ===================== USART1 (RS-485 HALF-DUPLEX RX) ===================== */
/* PA9 = USART1 TX (used as half-duplex RX/TX) */
#define USART1_RXTX_Pin       GPIO_PIN_9
#define USART1_RXTX_GPIO_Port GPIOA

/* ===================== USART6 (DRA818U) ===================== */
/* PC6 = TX, PC7 = RX */
#define USART6_TX_Pin         GPIO_PIN_6
#define USART6_TX_GPIO_Port   GPIOC

#define USART6_RX_Pin         GPIO_PIN_7
#define USART6_RX_GPIO_Port   GPIOC

/* ===================== Other Pins ===================== */
#define SWO_Pin               GPIO_PIN_3
#define SWO_GPIO_Port         GPIOB

/* ===================== Buffer Size ===================== */
#define LINE_BUF_SIZE         256

/* ===================== Prototypes ===================== */
void Error_Handler(void);

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
