/* Core/app_3_dma_idle/app.c — Stage 3: DMA reception ended by the IDLE line */
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "stm32f4xx_hal.h"
#include "app.h"
#include "system.h"

#define TX_TIMEOUT_MS   100U
#define RX_DMA_SIZE      32U

static UART_HandleTypeDef huart2;
static DMA_HandleTypeDef  hdma_usart2_rx;   /* static at file level: the HAL keeps its address */

static uint8_t           rx_dma_buf[RX_DMA_SIZE];
static volatile uint16_t rx_event_size  = 0U;
static volatile uint32_t rx_event_type  = 0U;
static volatile bool     rx_event_ready = false;

static volatile uint32_t rx_error_count = 0U;
static volatile uint32_t rx_error_code  = 0U;
static uint32_t          rx_error_reported = 0U;

static void uart2_init(void);
static void uart2_start_rx(void);
static void uart2_send_text(const char *text);
static void event_name(uint32_t type, char *name);
static void print_block(uint32_t type, uint16_t size);

void app_init(void)
{
    uart2_init();
    uart2_send_text("\r\nStage 3: DMA reception with IDLE line.\r\n");
    uart2_start_rx();
}

void app_loop(void)
{
    uint16_t size_aux;
    uint32_t type_aux;
    bool     got_event = false;
    char     msg[56];

    if (rx_error_reported != rx_error_count) {
        rx_error_reported = rx_error_count;
        (void)snprintf(msg, sizeof msg, "[uart error 0x%02lX, reception restarted]\r\n",
                       (unsigned long)rx_error_code);
        uart2_send_text(msg);
    }

    /* Claim the event with interrupts off for a few instructions, so a new
       event cannot change the size between reading it and reading the type */
    __disable_irq();
    if (rx_event_ready) {
        size_aux       = rx_event_size;
        type_aux       = rx_event_type;
        rx_event_ready = false;
        got_event      = true;
    }
    __enable_irq();

    if (got_event) {
        print_block(type_aux, size_aux);
    }
}

/* The handle settings only: clocks, pins, DMA and NVIC live in HAL_UART_MspInit() */
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
    if (HAL_UART_Init(&huart2) != HAL_OK) {   /* calls HAL_UART_MspInit() */
        Error_Handler();
    }
}

/* Called by HAL_UART_Init() just before it writes the USART registers */
void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART2) {
        return;
    }

    GPIO_InitTypeDef gpio = {0};

    /* 1. Clocks: the DMA clock must run before HAL_DMA_Init() */
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

    /* 3. DMA1, stream 5, channel 4 = USART2_RX */
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

    /* 4. Interrupts: USART2 (IDLE, errors) and the DMA stream (buffer full) */
    HAL_NVIC_SetPriority(USART2_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
    HAL_NVIC_SetPriority(DMA1_Stream5_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);
}

static void uart2_start_rx(void)
{
    if (HAL_UARTEx_ReceiveToIdle_DMA(&huart2, rx_dma_buf, RX_DMA_SIZE) == HAL_OK) {
        /* The HAL switches HT on at every start: we want whole blocks only */
        __HAL_DMA_DISABLE_IT(&hdma_usart2_rx, DMA_IT_HT);
    }
}

static void uart2_send_text(const char *text)
{
    (void)HAL_UART_Transmit(&huart2, (const uint8_t *)text, (uint16_t)strlen(text), TX_TIMEOUT_MS);
}

/* Writes the event name into the caller's array */
static void event_name(uint32_t type, char *name)
{
    switch (type) {
    case HAL_UART_RXEVENT_IDLE: (void)strcpy(name, "IDLE"); break;
    case HAL_UART_RXEVENT_TC:   (void)strcpy(name, "TC");   break;
    case HAL_UART_RXEVENT_HT:   (void)strcpy(name, "HT");   break;
    default:                    (void)strcpy(name, "?");    break;
    }
}

static void print_block(uint32_t type, uint16_t size)
{
    char name[8];
    char msg[RX_DMA_SIZE + 24U];
    uint16_t used;

    event_name(type, name);
    used = (uint16_t)snprintf(msg, sizeof msg, "[%s %2u bytes] '", name, (unsigned)size);
    for (uint16_t i = 0U; (i < size) && (i < RX_DMA_SIZE); i++) {
        uint8_t c = rx_dma_buf[i];
        msg[used++] = ((c >= 0x20U) && (c <= 0x7EU)) ? (char)c : '.';
    }
    msg[used++] = '\'';
    msg[used++] = '\r';
    msg[used++] = '\n';
    msg[used]   = '\0';
    uart2_send_text(msg);
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

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance == USART2) {
        rx_event_size  = Size;
        rx_event_type  = (uint32_t)HAL_UARTEx_GetRxEventType(huart);
        rx_event_ready = true;
        uart2_start_rx();                   /* IDLE and TC stop the reception: start another */
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2) {
        rx_error_code = huart->ErrorCode;
        rx_error_count++;
        uart2_start_rx();                   /* in DMA mode every error stops the reception */
    }
}
