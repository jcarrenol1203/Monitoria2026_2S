/* Core/common/pwm.h — one PWM output: TIM3 channel 1 on PA6 (Arduino D12), 1 kHz */
#ifndef PWM_H
#define PWM_H

#include <stdbool.h>
#include <stdint.h>

void     pwm_init(void);                     /* starts the output at 0 % */
bool     pwm_set_duty(uint32_t percent);     /* 0..100; false if not started */
uint32_t pwm_get_duty(void);

#endif
