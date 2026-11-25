/* Core/app_4_pingpong/app.c — Stage 4: ping-pong reception, DMA transmission, commands */
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "stm32f4xx_hal.h"
#include "app.h"
#include "system.h"
#include "pwm.h"
#include "cmd_parser.h"
#include "commands.h"

#define RX_BLOCK_SIZE    32U
#define TX_BUF_SIZE     512U
#define LINE_MAX_LEN     63U
#define REPLY_SIZE       96U

/* ---- Reception: two blocks, one owner each ---- */
typedef struct {
    uint8_t           data[RX_BLOCK_SIZE];
    volatile uint16_t size;
    volatile bool     ready;        /* false: the DMA side owns it; true: app_loop() owns it */
} rx_block_t;

static rx_block_t        rx_block[2];
static volatile uint8_t  dma_index = 0U;        /* the block the DMA is filling */
static volatile bool     rx_paused = false;     /* both blocks busy: reception stopped */
static volatile uint32_t rx_pause_count = 0U;
static uint32_t          rx_pause_reported = 0U;
static uint8_t           read_index = 0U;       /* the next block app_loop() will read */

static volatile uint32_t rx_error_count = 0U;
static volatile uint32_t rx_error_code  = 0U;
static uint32_t          rx_error_reported = 0U;

/* ---- Transmission: tx_fill collects text, tx_dma_buf is what the DMA reads ---- */
static uint8_t       tx_fill[TX_BUF_SIZE];
static uint16_t      tx_fill_len = 0U;
static uint8_t       tx_dma_buf[TX_BUF_SIZE];
static volatile bool tx_busy = false;

/* ---- Lines ---- */
static char     line[LINE_MAX_LEN + 1U];
static uint8_t  line_len      = 0U;
static bool     line_too_long = false;
static uint8_t  previous_byte = 0U;

static UART_HandleTypeDef huart2;
static DMA_HandleTypeDef  hdma_usart2_rx;
static DMA_HandleTypeDef  hdma_usart2_tx;

static void uart2_init(void);
static void uart2_start_rx(void);
static void uart2_send_text(const char *text);
static void uart2_send_bytes(const uint8_t *data, uint16_t length);
static void uart2_flush(void);
static void line_add_byte(uint8_t new_byte);
static void line_execute(void);

void app_init(void)
{
    uart2_init();
    pwm_init();                                /* the PWM output, at 0 % */
    uart2_send_text("\r\nStage 4: ping-pong reception.\r\n> ");
    uart2_start_rx();
}

void app_loop(void)
{
    char msg[64];

    /* 0. Report what the interrupts counted */
    if (rx_error_reported != rx_error_count) {
        rx_error_reported = rx_error_count;
        (void)snprintf(msg, sizeof msg, "\r\n[uart error 0x%02lX, reception restarted]\r\n> ",
                       (unsigned long)rx_error_code);
        uart2_send_text(msg);
    }
    if (rx_pause_reported != rx_pause_count) {
        rx_pause_reported = rx_pause_count;
        uart2_send_text("\r\n[reception paused: both blocks busy, data lost]\r\n> ");
    }

    /* 1. Process the block we own, byte by byte, through the line assembler */
    if (rx_block[read_index].ready) {
        bool restart_aux;

        for (uint16_t i = 0U; i < rx_block[read_index].size; i++) {
            line_add_byte(rx_block[read_index].data[i]);
        }

        /* 2. Give the block back and look at rx_paused together, interrupts off */
        __disable_irq();
        rx_block[read_index].ready = false;
        restart_aux = rx_paused;
        if (restart_aux) {
            rx_paused = false;
            dma_index = read_index;            /* the block we just freed */
        }
        __enable_irq();

        if (restart_aux) {
            uart2_start_rx();
        }
        read_index = (uint8_t)(1U - read_index);
    }

    /* 3. Start sending the text that is waiting */
    uart2_flush();
}

static void uart2_init(void)
{
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

void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART2) {
        return;
    }

    GPIO_InitTypeDef gpio = {0};

    /* 1. Clocks */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_DMA1_CLK_ENABLE();

    /* 2. Pins */
    gpio.Pin       = GPIO_PIN_2 | GPIO_PIN_3;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_PULLUP;
    gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &gpio);

    /* 3a. DMA1 stream 5, channel 4 = USART2_RX */
    hdma_usart2_rx.Instance                 = DMA1_Stream5;
    hdma_usart2_rx.Init.Channel             = DMA_CHANNEL_4;
    hdma_usart2_rx.Init.Direction           = DMA_PERIPH_TO_MEMORY;
    hdma_usart2_rx.Init.PeriphInc           = DMA_PINC_DISABLE;
    hdma_usart2_rx.Init.MemInc              = DMA_MINC_ENABLE;
    hdma_usart2_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart2_rx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
    hdma_usart2_rx.Init.Mode                = DMA_NORMAL;
    hdma_usart2_rx.Init.Priority            = DMA_PRIORITY_LOW;
    hdma_usart2_rx.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&hdma_usart2_rx) != HAL_OK) {
        Error_Handler();
    }
    __HAL_LINKDMA(huart, hdmarx, hdma_usart2_rx);

    /* 3b. DMA1 stream 6, channel 4 = USART2_TX */
    hdma_usart2_tx.Instance                 = DMA1_Stream6;
    hdma_usart2_tx.Init.Channel             = DMA_CHANNEL_4;
    hdma_usart2_tx.Init.Direction           = DMA_MEMORY_TO_PERIPH;
    hdma_usart2_tx.Init.PeriphInc           = DMA_PINC_DISABLE;
    hdma_usart2_tx.Init.MemInc              = DMA_MINC_ENABLE;
    hdma_usart2_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart2_tx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
    hdma_usart2_tx.Init.Mode                = DMA_NORMAL;
    hdma_usart2_tx.Init.Priority            = DMA_PRIORITY_LOW;
    hdma_usart2_tx.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&hdma_usart2_tx) != HAL_OK) {
        Error_Handler();
    }
    __HAL_LINKDMA(huart, hdmatx, hdma_usart2_tx);

    /* 4. Interrupts */
    HAL_NVIC_SetPriority(USART2_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
    HAL_NVIC_SetPriority(DMA1_Stream5_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);
    HAL_NVIC_SetPriority(DMA1_Stream6_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream6_IRQn);
}

static void uart2_start_rx(void)
{
    if (HAL_UARTEx_ReceiveToIdle_DMA(&huart2, rx_block[dma_index].data, RX_BLOCK_SIZE) == HAL_OK) {
        __HAL_DMA_DISABLE_IT(&hdma_usart2_rx, DMA_IT_HT);   /* whole blocks only */
    }
}

/* ---- Transmission ---- */
static void uart2_send_text(const char *text)
{
    uart2_send_bytes((const uint8_t *)text, (uint16_t)strlen(text));
}

/* Appends to tx_fill and returns at once. Text that does not fit is dropped whole. */
static void uart2_send_bytes(const uint8_t *data, uint16_t length)
{
    if ((tx_fill_len + length) > TX_BUF_SIZE) {
        return;
    }
    (void)memcpy(&tx_fill[tx_fill_len], data, length);
    tx_fill_len = (uint16_t)(tx_fill_len + length);
}

/* If the DMA is free and text is waiting: copy it to tx_dma_buf and start sending */
static void uart2_flush(void)
{
    if (tx_busy || (tx_fill_len == 0U)) {
        return;
    }
    (void)memcpy(tx_dma_buf, tx_fill, tx_fill_len);
    tx_busy = true;                            /* set before the DMA starts */
    if (HAL_UART_Transmit_DMA(&huart2, tx_dma_buf, tx_fill_len) != HAL_OK) {
        tx_busy = false;
    }
    tx_fill_len = 0U;
}

/* ---- Lines (the same as Stage 2, but the echo goes through uart2_send_bytes) ---- */
static void line_add_byte(uint8_t new_byte)
{
    if ((new_byte == '\r') || (new_byte == '\n')) {
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
        return;
    }
    if (line_len < LINE_MAX_LEN) {
        line[line_len] = (char)new_byte;
        line_len++;
        uart2_send_bytes(&new_byte, 1U);
    } else {
        line_too_long = true;
    }
}

static void line_execute(void)
{
    cmd_t cmd;
    char  reply[REPLY_SIZE];

    line[line_len] = '\0';

    if (line_too_long) {
        uart2_send_text("error: line too long\r\n");
    } else if (cmd_parse(line, &cmd)) {
        commands_execute(&cmd, reply, sizeof reply);
        uart2_send_text(reply);
        uart2_send_text("\r\n");
    }
    uart2_send_text("> ");

    line_len      = 0U;
    line_too_long = false;
}

/* ---- Interrupt side ---- */
void USART2_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart2);
}

void DMA1_Stream5_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_usart2_rx);
}

void DMA1_Stream6_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_usart2_tx);
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance != USART2) {
        return;
    }
    if (HAL_UARTEx_GetRxEventType(huart) == HAL_UART_RXEVENT_HT) {
        return;                                /* half transfer does not end a block */
    }

    rx_block[dma_index].size  = Size;
    rx_block[dma_index].ready = true;          /* the block now belongs to app_loop() */

    uint8_t next = (uint8_t)(1U - dma_index);
    if (!rx_block[next].ready) {
        dma_index = next;
        uart2_start_rx();
    } else {
        rx_paused = true;                      /* no free block: pause, never overwrite */
        rx_pause_count++;
    }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2) {
        tx_busy = false;                       /* the last byte has left */
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART2) {
        return;
    }
    rx_error_code = huart->ErrorCode;
    rx_error_count++;
    if (huart->gState == HAL_UART_STATE_READY) {
        tx_busy = false;                       /* a transmission error also ends here */
    }
    if (!rx_paused) {
        uart2_start_rx();                      /* every error stops the reception */
    }
}
