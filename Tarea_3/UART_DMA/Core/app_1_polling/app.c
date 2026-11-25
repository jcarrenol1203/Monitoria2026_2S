/* Core/app_1_polling/app.c — Stage 1: blocking HAL_UART_Transmit / HAL_UART_Receive */
#include <stdio.h>
#include <string.h>
#include "stm32f4xx_hal.h"
#include "app.h"
#include "system.h"

#define TX_TIMEOUT_MS   100U
#define RX_TIMEOUT_MS    10U     /* HAL_MAX_DELAY would wait forever (see "Break it") */
#define TICK_PERIOD_MS 1000U

static UART_HandleTypeDef huart2;     /* static: only this file can touch it */

static void uart2_init(void);
static void uart2_send_text(const char *text);

void app_init(void)
{
    uart2_init();
    uart2_send_text("\r\nStage 1: polling. Type something.\r\n");
}

void app_loop(void)
{
    static uint32_t last_tick_ms = 0U;
    static uint32_t tick_count   = 0U;
    uint8_t key;
    char    msg[48];

    /* 1. One tick line per second */
    if ((HAL_GetTick() - last_tick_ms) >= TICK_PERIOD_MS) {
        last_tick_ms += TICK_PERIOD_MS;
        tick_count++;
        (void)snprintf(msg, sizeof msg, "tick %lu at %lu ms\r\n",
                       (unsigned long)tick_count, (unsigned long)HAL_GetTick());
        uart2_send_text(msg);
    }

    /* 2. Wait up to RX_TIMEOUT_MS for one key; HAL_TIMEOUT means nobody typed */
    if (HAL_UART_Receive(&huart2, &key, 1U, RX_TIMEOUT_MS) == HAL_OK) {
        char shown = ((key >= 0x20U) && (key <= 0x7EU)) ? (char)key : '.';
        (void)snprintf(msg, sizeof msg, "rx: '%c' 0x%02X\r\n", shown, (unsigned)key);
        uart2_send_text(msg);
    }
}

/* Same order as heartbeat_init(): clocks, pins, peripheral */
static void uart2_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    /* 1. Clocks */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USART2_CLK_ENABLE();

    /* 2. Pins: PA2 = TX, PA3 = RX, both alternate function 7 */
    gpio.Pin       = GPIO_PIN_2 | GPIO_PIN_3;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_PULLUP;
    gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &gpio);

    /* 3. Peripheral: 115200 8N1, no flow control */
    huart2.Instance          = USART2;
    huart2.Init.BaudRate     = 115200U;
    huart2.Init.WordLength   = UART_WORDLENGTH_8B;
    huart2.Init.StopBits     = UART_STOPBITS_1;
    huart2.Init.Parity       = UART_PARITY_NONE;
    huart2.Init.Mode         = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart2) != HAL_OK) {
        Error_Handler();
    }
}

static void uart2_send_text(const char *text)
{
    (void)HAL_UART_Transmit(&huart2, (const uint8_t *)text, (uint16_t)strlen(text), TX_TIMEOUT_MS);
}
