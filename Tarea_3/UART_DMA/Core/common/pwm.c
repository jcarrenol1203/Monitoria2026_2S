/* Core/common/pwm.c — TIM3 channel 1 on PA6 (AF2), 1 kHz */
#include "stm32f4xx_hal.h"
#include "pwm.h"
#include "system.h"

#define PWM_TIM        TIM3
#define PWM_CHANNEL    TIM_CHANNEL_1
#define PWM_TICK_HZ    1000000U     /* counter at 1 MHz: 1 tick = 1 us */
#define PWM_PERIOD     1000U        /* 1000 ticks = 1 ms = 1 kHz */

static TIM_HandleTypeDef htim_pwm;
static bool     pwm_started = false;
static uint32_t pwm_duty    = 0U;

void pwm_init(void)
{
    GPIO_InitTypeDef gpio = {0};
    TIM_OC_InitTypeDef oc = {0};
    uint32_t pclk1 = HAL_RCC_GetPCLK1Freq();
    uint32_t timer_clock = ((RCC->CFGR & RCC_CFGR_PPRE1) == RCC_CFGR_PPRE1_DIV1) ? pclk1 : (2U * pclk1);

    /* 1. Clocks */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_TIM3_CLK_ENABLE();

    /* 2. Pin PA6 as TIM3_CH1 (AF2) */
    gpio.Pin       = GPIO_PIN_6;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_LOW;
    gpio.Alternate = GPIO_AF2_TIM3;
    HAL_GPIO_Init(GPIOA, &gpio);

    /* 3. Timer base: 1 MHz, counts 0..999 */
    htim_pwm.Instance               = PWM_TIM;
    htim_pwm.Init.Prescaler         = (timer_clock / PWM_TICK_HZ) - 1U;
    htim_pwm.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim_pwm.Init.Period            = PWM_PERIOD - 1U;
    htim_pwm.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim_pwm.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    if (HAL_TIM_PWM_Init(&htim_pwm) != HAL_OK) {
        Error_Handler();
    }

    /* 4. Channel 1: PWM mode 1, high while CNT < compare */
    oc.OCMode     = TIM_OCMODE_PWM1;
    oc.Pulse      = 0U;                 /* 0 % */
    oc.OCPolarity = TIM_OCPOLARITY_HIGH;
    oc.OCFastMode = TIM_OCFAST_DISABLE;
    if (HAL_TIM_PWM_ConfigChannel(&htim_pwm, &oc, PWM_CHANNEL) != HAL_OK) {
        Error_Handler();
    }

    if (HAL_TIM_PWM_Start(&htim_pwm, PWM_CHANNEL) != HAL_OK) {
        Error_Handler();
    }
    pwm_duty    = 0U;
    pwm_started = true;
}

bool pwm_set_duty(uint32_t percent)
{
    if (!pwm_started || (percent > 100U)) {
        return false;
    }
    pwm_duty = percent;
    /* percent x 10 ticks out of 1000: 25 % -> 250. Preload applies it at the end of the period. */
    __HAL_TIM_SET_COMPARE(&htim_pwm, PWM_CHANNEL, percent * (PWM_PERIOD / 100U));
    return true;
}

uint32_t pwm_get_duty(void)
{
    return pwm_duty;
}
