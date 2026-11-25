/* Core/app_2_interrupt/app.c — Stage 2: RX by interrupt, ring buffer, lines and commands */
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "stm32f4xx_hal.h"
#include "app.h"
#include "system.h"
#include "cmd_parser.h"
#include "commands.h"

#define TX_TIMEOUT_MS   100U
#define RX_RING_SIZE    128U               /* power of two: "& (size - 1)" wraps the index */
#define LINE_MAX_LEN     63U
#define REPLY_SIZE       96U

static UART_HandleTypeDef huart2;

/* Ring buffer: the interrupt writes rx_head, app_loop() writes rx_tail */
static uint8_t           rx_ring[RX_RING_SIZE];
static volatile uint16_t rx_head = 0U;
static volatile uint16_t rx_tail = 0U;
static uint8_t           rx_byte;           /* the HAL writes each received byte here */

/* Errors are counted in the interrupt and reported from app_loop() */
static volatile uint32_t rx_error_count = 0U;
static volatile uint32_t rx_error_code  = 0U;
static volatile uint32_t rx_overflow    = 0U;
static uint32_t          rx_error_reported = 0U;

/* Line assembly */
static char     line[LINE_MAX_LEN + 1U];    /* +1 for the '\0' */
static uint8_t  line_len        = 0U;
static bool     line_too_long   = false;
static uint8_t  previous_byte   = 0U;

static void uart2_init(void);
static void uart2_start_rx(void);
static void uart2_send_text(const char *text);
static bool rx_ring_get(uint8_t *byte_to_get);
static void line_add_byte(uint8_t new_byte);
static void line_execute(void);

void app_init(void)
{
    uart2_init();
    uart2_send_text("\r\nStage 2: interrupt reception.\r\n> ");
    uart2_start_rx();
}

void app_loop(void)
{
    uint8_t byte_read;
    char    msg[48];

    /* 0. Report receive errors the interrupt counted */
    if (rx_error_reported != rx_error_count) {
        rx_error_reported = rx_error_count;
        (void)snprintf(msg, sizeof msg, "\r\n[uart error 0x%02lX, reception restarted]\r\n> ",
                       (unsigned long)rx_error_code);
        uart2_send_text(msg);
    }

    /* 1. Take bytes out of the ring and feed the line assembler */
    while (rx_ring_get(&byte_read)) {
        line_add_byte(byte_read);
    }
}

/* Clocks, pins, peripheral, NVIC: the same order as the heartbeat */
static void uart2_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    /* 1. Clocks */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USART2_CLK_ENABLE();

    /* 2. Pins */
    gpio.Pin       = GPIO_PIN_2 | GPIO_PIN_3;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_PULLUP;
    gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &gpio);

    /* 3. Peripheral */
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

    /* 4. NVIC: more urgent (2) than the heartbeat (3): a late byte is a lost byte */
    HAL_NVIC_SetPriority(USART2_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
}

static void uart2_start_rx(void)
{
    /* HAL_BUSY just means a reception is already running: harmless */
    (void)HAL_UART_Receive_IT(&huart2, &rx_byte, 1U);
}

static void uart2_send_text(const char *text)
{
    (void)HAL_UART_Transmit(&huart2, (const uint8_t *)text, (uint16_t)strlen(text), TX_TIMEOUT_MS);
}

/* ---- Ring buffer: the consumer side ---- */
static bool rx_ring_get(uint8_t *byte_to_get)
{
    if (rx_tail == rx_head) {
        return false;                       /* empty */
    }
    *byte_to_get = rx_ring[rx_tail];
    rx_tail = (uint16_t)((rx_tail + 1U) & (RX_RING_SIZE - 1U));
    return true;
}

/* ---- Lines ---- */
static void line_add_byte(uint8_t new_byte)
{
    if ((new_byte == '\r') || (new_byte == '\n')) {
        /* A '\n' right after a '\r' is the second half of "\r\n": already handled */
        if ((new_byte == '\n') && (previous_byte == '\r')) {
            previous_byte = new_byte;
            return;
        }
        previous_byte = new_byte;
        uart2_send_text("\r\n");
        line_execute();
        return;
    }

    previous_byte = new_byte;
    if ((new_byte < 0x20U) || (new_byte > 0x7EU)) {
        return;                             /* ignore control and non-ASCII bytes */
    }
    if (line_len < LINE_MAX_LEN) {
        line[line_len] = (char)new_byte;
        line_len++;
        (void)HAL_UART_Transmit(&huart2, &new_byte, 1U, TX_TIMEOUT_MS);   /* echo */
    } else {
        line_too_long = true;
    }
}

static void line_execute(void)
{
    cmd_t cmd;
    char  reply[REPLY_SIZE];

    line[line_len] = '\0';                  /* bytes from the UART do not come with a '\0' */

    if (line_too_long) {
        uart2_send_text("error: line too long\r\n");
    } else if (cmd_parse(line, &cmd)) {
        commands_execute(&cmd, reply, sizeof reply);
        uart2_send_text(reply);
        uart2_send_text("\r\n");
    }
    uart2_send_text("> ");

    line_len      = 0U;                     /* outside the if/else: runs in every case */
    line_too_long = false;
}

/* ---- Interrupt side ---- */
void USART2_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart2);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2) {
        uint16_t next = (uint16_t)((rx_head + 1U) & (RX_RING_SIZE - 1U));

        if (next != rx_tail) {              /* one slot stays empty: "full" differs from "empty" */
            rx_ring[rx_head] = rx_byte;
            rx_head = next;
        } else {
            rx_overflow++;                  /* ring full: drop the byte */
        }
        uart2_start_rx();                   /* one call receives one byte only */
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2) {
        rx_error_code = huart->ErrorCode;
        rx_error_count++;
        uart2_start_rx();                   /* an overrun aborts the reception: start it again */
    }
}
