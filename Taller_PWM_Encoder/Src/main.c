/**
 ******************************************************************************
 * @file           : main.c
 * @author         : Miguel A. Bedoya Gonzalez --> mibedoyag@unal.edu.co
 * @brief          : Taller PWM - Encoder KY040 (TIM2) controla el duty y el
 *                   pulsador (EXTI0) cambia la frecuencia del PWM (TIM1)
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 *
 *** Mapeo de Pines:
 ** PWM TIM1 - AF01:
 * - CH1 (PWM)  -> PA8
 *
 ** ENCODER TIM2 - AF01:
 * - CH1 (KY040: CLK) -> PA0
 * - CH2 (KY040: DT)  -> PA1
 *
 ** PULSADOR KY040 (SW), activo en bajo, con filtro RC externo a GND:
 * - SW_USE_TIM2_CH3 = 0 -> PB0 (EXTI0)               [por defecto]
 * - SW_USE_TIM2_CH3 = 1 -> PA2 (TIM2_CH3 - AF01, captura con filtro IC3F)
 *
 ** BLINKY TIM4 -> PH1 (500 ms)
 *
 *
 * ===================== FUNCIONAMIENTO ===================== *
 *
 * - TIM2 en modo encoder cuenta los flancos de CLK y DT (4 cuentas por paso).
 *   El contador se divide entre 4 y se satura entre 0 y 100, ese valor es el
 *   duty cycle en porcentaje.
 *
 * - TIM1 genera una senal PWM en PA8. Su frecuencia puede ser 500 Hz, 1 kHz
 *   o 2 kHz (tabla arr_table). El CCR1 se calcula siempre con el ARR actual,
 *   por eso el duty en % se conserva al cambiar de frecuencia.
 *
 * - Cada flanco de bajada del pulsador pasa a la siguiente frecuencia:
 *   500 Hz -> 1 kHz -> 2 kHz -> 500 Hz ...
 *   Se configura igual que el boton PC13 del taller de EXTI, pero en un pin
 *   cableado al SW del KY040. El rebote lo elimina un filtro RC externo, la ISR
 *   solo levanta una bandera y no espera.
 *
 * - TIM4 hace parpadear el PH1 cada 500 ms para saber que el programa corre.
 *
 * Reloj del sistema: HSI 16 MHz (sin PLL), los timers reciben 16 MHz.
 *
 ******************************************************************************
 */

#include <stdint.h>
#include <stm32f4xx.h>

/* Seleccion del pulsador: 0 = EXTI0 en PB0, 1 = captura del TIM2_CH3 en PA2 */
#define SW_USE_TIM2_CH3	0

/* Constantes */
#define NUM_FREQ		3		// Cantidad de frecuencias disponibles
#define DUTY_MAX		100		// Duty maximo en %
#define COUNTS_PER_STEP	4		// El modo TI12 cuenta 4 flancos por paso del encoder
#define ENCODER_MAX_CNT	(DUTY_MAX * COUNTS_PER_STEP)	// Cuenta maxima del TIM2 (400)

/* Variables */
volatile uint8_t button_event = 0;	// La ISR del pulsador la pone en 1, el main la atiende y la baja
uint8_t freq_index = 0;				// Indice de la frecuencia actual en arr_table
uint8_t duty_percent = 0;			// Duty actual en % (0 - 100)
const uint16_t arr_table[NUM_FREQ] = {1999, 999, 499};	// ARR para 500 Hz, 1 kHz y 2 kHz con tick de 1 us

/* Maquina de estados */
typedef enum
{
	STATE_READ_ENCODER = 0,
	STATE_UPDATE_PWM,
	STATE_CHECK_BUTTON
} State_t;

State_t current_state = STATE_READ_ENCODER;	// Siempre arranca leyendo el encoder

/* Definicion de funciones */
static void gpio_Init(void);			// GPIOH PH1 como salida (blinky)
static void tim4_Init(void);			// TIM4 con interrupcion cada 500 ms (blinky)
static void tim1_pwm_Init(void);		// GPIOA PA8 + TIM1 CH1 en modo PWM 1
static void tim2_encoder_Init(void);	// GPIOA PA0, PA1 + TIM2 en modo encoder
#if SW_USE_TIM2_CH3
static void sw_tim2ch3_Config(void);	// GPIOA PA2 + canal 3 del TIM2 como captura con filtro
#else
static void exti0_Init(void);			// GPIOB PB0 + EXTI0 (flanco de bajada)
#endif
static void pwm_SetFrequency(uint8_t index);	// Cambia el ARR del TIM1 segun arr_table
static void pwm_SetDuty(uint8_t percent);		// Calcula el CCR1 a partir del % y el ARR actual

/* Main */
int main(void)
{
	int32_t raw_count = 0;	// Cuenta del TIM2 vista con signo

	gpio_Init();
	tim1_pwm_Init();
	tim2_encoder_Init();	// Con SW_USE_TIM2_CH3 = 1 tambien configura el pulsador
#if !SW_USE_TIM2_CH3
	exti0_Init();
#endif
	tim4_Init();			// Ultimo: las interrupciones empiezan con todo listo

	while (1)
	{
		switch (current_state)
		{
		case STATE_READ_ENCODER:
			raw_count = (int32_t) TIM2->CNT;	// CNT es de 32 bit sin signo, al girar en CCW desde 0 pasa a 0xFFFFFFFF = -1 con signo

			if (raw_count < 0)	// Se paso por debajo de 0, se satura en 0
			{
				TIM2->CNT = 0;
				raw_count = 0;
			}
			else if (raw_count > ENCODER_MAX_CNT)	// Se paso de 100 pasos, se satura en 100
			{
				TIM2->CNT = ENCODER_MAX_CNT;
				raw_count = ENCODER_MAX_CNT;
			}

			duty_percent = (uint8_t) (raw_count / COUNTS_PER_STEP);	// 4 cuentas = 1 paso = 1 %
			current_state = STATE_UPDATE_PWM;
			break;

		case STATE_UPDATE_PWM:
			pwm_SetDuty(duty_percent);
			current_state = STATE_CHECK_BUTTON;
			break;

		case STATE_CHECK_BUTTON:
			if (button_event)
			{
				button_event = 0;	// Se baja la bandera para esperar otro pulso
				freq_index = (uint8_t) ((freq_index + 1) % NUM_FREQ);
				pwm_SetFrequency(freq_index);	// El CCR1 se recalcula en el siguiente ciclo de la FSM
			}
			current_state = STATE_READ_ENCODER;
			break;

		default:
			current_state = STATE_READ_ENCODER;
			break;
		}
	}
}

/*
 * gpio_Init
 * PH1 como salida push-pull, LED de la tarjeta (blinky)
 */
static void gpio_Init(void)
{
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOHEN;	// Reloj del GPIOH (AHB1)

	GPIOH->MODER &= ~GPIO_MODER_MODER1;
	GPIOH->MODER |= GPIO_MODER_MODER1_0;	// 01: salida
	GPIOH->OTYPER &= ~GPIO_OTYPER_OT1;		// Push-pull
	GPIOH->OSPEEDR &= ~GPIO_OSPEEDER_OSPEEDR1;	// Baja velocidad
	GPIOH->PUPDR &= ~GPIO_PUPDR_PUPDR1;		// Sin pull-up ni pull-down
	GPIOH->ODR &= ~GPIO_ODR_OD1;			// Inicia en bajo
}

/*
 * tim4_Init
 * Evento de actualizacion cada 500 ms
 * PSC = 15999 -> tick de 1 ms, ARR = 499 -> (499 + 1) x 1 ms = 500 ms
 */
static void tim4_Init(void)
{
	RCC->APB1ENR |= RCC_APB1ENR_TIM4EN;		// Reloj del TIM4 (APB1)

	TIM4->PSC = 15999;						// 16 MHz / 16000 = 1 kHz
	TIM4->ARR = 499;						// 500 ms
	TIM4->CNT = 0;

	TIM4->EGR |= TIM_EGR_UG;				// Carga el PSC (solo se aplica en un update)
	TIM4->SR &= ~TIM_SR_UIF;				// El UG levanta UIF, se limpia para no entrar a la ISR de inmediato

	TIM4->DIER |= TIM_DIER_UIE;				// Interrupcion por update
	__NVIC_EnableIRQ(TIM4_IRQn);

	TIM4->CR1 |= TIM_CR1_CEN;				// Se arranca de ultimo
}

/*
 * tim1_pwm_Init
 * PA8 -> TIM1_CH1 (AF1), PWM modo 1, 500 Hz, duty 0 %
 * Clock TIM1: 16 MHz (APB2 sin division). PSC = 15 -> tick de 1 us
 * Periodo = (ARR + 1) x 1 us = 2000 us -> 500 Hz
 */
static void tim1_pwm_Init(void)
{
	/* GPIOA PA8 como funcion alterna AF1 */
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;	// Reloj del GPIOA (AHB1)

	GPIOA->MODER &= ~GPIO_MODER_MODER8;
	GPIOA->MODER |= GPIO_MODER_MODER8_1;	// 10: funcion alterna

	GPIOA->OTYPER &= ~GPIO_OTYPER_OT8;		// Push-pull

	GPIOA->OSPEEDR &= ~GPIO_OSPEEDER_OSPEEDR8;
	GPIOA->OSPEEDR |= GPIO_OSPEEDER_OSPEEDR8_1;	// 10: alta velocidad

	GPIOA->PUPDR &= ~GPIO_PUPDR_PUPDR8;		// Sin pull-up ni pull-down, el timer maneja el nivel

	GPIOA->AFR[1] &= ~GPIO_AFRH_AFSEL8;
	GPIOA->AFR[1] |= GPIO_AFRH_AFSEL8_0;	// AFRH8 = 0001 -> AF1 (TIM1_CH1)

	/* Base de tiempo TIM1 */
	RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;		// Reloj del TIM1 (APB2)

	TIM1->PSC = 15;							// 16 MHz / (15 + 1) = 1 MHz -> 1 us
	TIM1->ARR = arr_table[0];				// 1999 -> 2000 us -> 500 Hz
	TIM1->CR1 &= ~TIM_CR1_DIR;				// Contador ascendente
	TIM1->CR1 |= TIM_CR1_ARPE;				// ARR con preload: el cambio de frecuencia se aplica al terminar el periodo

	/* Canal 1 en PWM modo 1 */
	TIM1->CCMR1 &= ~TIM_CCMR1_CC1S;			// 00: el canal 1 es salida
	TIM1->CCMR1 &= ~TIM_CCMR1_OC1M;
	TIM1->CCMR1 |= (TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2);	// 110: PWM modo 1 (alto mientras CNT < CCR1)
	TIM1->CCMR1 |= TIM_CCMR1_OC1PE;			// CCR1 con preload: el duty cambia solo al inicio de un periodo

	TIM1->CCR1 = 0;							// Duty inicial 0 %

	TIM1->CCER &= ~TIM_CCER_CC1P;			// Polaridad activa en alto
	TIM1->CCER |= TIM_CCER_CC1E;			// Habilita la salida del canal 1 hacia el pin

	TIM1->BDTR |= TIM_BDTR_MOE;				// TIM1 es timer avanzado: sin MOE las salidas quedan apagadas

	TIM1->EGR |= TIM_EGR_UG;				// Evento de actualizacion: carga PSC, ARR y CCR1 desde sus preload

	TIM1->CR1 |= TIM_CR1_CEN;				// Se arranca de ultimo
}

/*
 * tim2_encoder_Init
 * PA0 -> TIM2_CH1 (CLK), PA1 -> TIM2_CH2 (DT), ambos AF1 con Pull-Up
 * Modo encoder 3 (TI12): cuenta en los flancos de los dos canales
 * -> 4 cuentas por paso mecanico del KY040
 *
 * IC1F e IC2F: filtro digital de la entrada. Toma muestras a f_DTS/32 y solo
 * acepta un cambio si 8 muestras seguidas coinciden: 8 x 2 us = 16 us (f_DTS = 16 MHz)
 */
static void tim2_encoder_Init(void)
{
	/* GPIOA PA0 y PA1 como funcion alterna AF1 */
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;	// Reloj del GPIOA (AHB1)

	GPIOA->MODER &= ~(GPIO_MODER_MODER0 | GPIO_MODER_MODER1);
	GPIOA->MODER |= (GPIO_MODER_MODER0_1 | GPIO_MODER_MODER1_1);	// 10: funcion alterna

	GPIOA->PUPDR &= ~(GPIO_PUPDR_PUPDR0 | GPIO_PUPDR_PUPDR1);
	GPIOA->PUPDR |= (GPIO_PUPDR_PUPDR0_0 | GPIO_PUPDR_PUPDR1_0);	// 01: Pull-Up, los pines no quedan flotando

	GPIOA->AFR[0] &= ~(GPIO_AFRL_AFSEL0 | GPIO_AFRL_AFSEL1);
	GPIOA->AFR[0] |= (GPIO_AFRL_AFSEL0_0 | GPIO_AFRL_AFSEL1_0);	// AFRL0 = AFRL1 = 0001 -> AF1 (TIM2_CH1, TIM2_CH2)

	/* Base de tiempo TIM2 (32 bit) */
	RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;		// Reloj del TIM2 (APB1)

	TIM2->PSC = 0;							// El encoder no se divide
	TIM2->ARR = 0xFFFFFFFF;					// Rango completo de 32 bit
	TIM2->CNT = 0;

	/* Canales 1 y 2 como entradas */
	TIM2->CCMR1 &= ~(TIM_CCMR1_CC1S | TIM_CCMR1_CC2S);
	TIM2->CCMR1 |= (TIM_CCMR1_CC1S_0 | TIM_CCMR1_CC2S_0);	// 01: CC1 en TI1 y CC2 en TI2 (directo)

	TIM2->CCMR1 &= ~(TIM_CCMR1_IC1F | TIM_CCMR1_IC2F);
	TIM2->CCMR1 |= (TIM_CCMR1_IC1F | TIM_CCMR1_IC2F);		// Filtro 1111 (maximo): f_DTS/32 con N = 8

	TIM2->CCER &= ~(TIM_CCER_CC1P | TIM_CCER_CC1NP | TIM_CCER_CC2P | TIM_CCER_CC2NP);	// Sin inversion (si cuenta al reves, se activa CC1P)
	TIM2->CCER |= (TIM_CCER_CC1E | TIM_CCER_CC2E);			// Habilita las capturas de los dos canales

	/* Modo encoder */
	TIM2->SMCR &= ~TIM_SMCR_SMS;
	TIM2->SMCR |= (TIM_SMCR_SMS_0 | TIM_SMCR_SMS_1);		// 011: encoder mode 3 (cuenta con TI1 y TI2)

#if SW_USE_TIM2_CH3
	sw_tim2ch3_Config();					// El canal 3 se configura antes de arrancar el contador
#endif

	TIM2->CR1 |= TIM_CR1_CEN;				// Se arranca de ultimo
}

#if SW_USE_TIM2_CH3
/*
 * sw_tim2ch3_Config
 * Pulsador en PA2 -> TIM2_CH3 (AF1), captura en flanco de bajada con filtro IC3F
 * El canal 3 es independiente del par CH1/CH2 del encoder, no lo afecta.
 * Con CC3IE se genera una interrupcion (CC3IF) en cada flanco aceptado.
 */
static void sw_tim2ch3_Config(void)
{
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;	// Reloj del GPIOA (AHB1)

	GPIOA->MODER &= ~GPIO_MODER_MODER2;
	GPIOA->MODER |= GPIO_MODER_MODER2_1;	// 10: funcion alterna

	GPIOA->PUPDR &= ~GPIO_PUPDR_PUPDR2;
	GPIOA->PUPDR |= GPIO_PUPDR_PUPDR2_0;	// 01: Pull-Up, en reposo es 1

	GPIOA->AFR[0] &= ~GPIO_AFRL_AFSEL2;
	GPIOA->AFR[0] |= GPIO_AFRL_AFSEL2_0;	// AFRL2 = 0001 -> AF1 (TIM2_CH3)

	TIM2->CCMR2 &= ~TIM_CCMR2_CC3S;
	TIM2->CCMR2 |= TIM_CCMR2_CC3S_0;		// 01: CC3 es entrada en TI3

	TIM2->CCMR2 &= ~TIM_CCMR2_IC3F;
	TIM2->CCMR2 |= TIM_CCMR2_IC3F;			// Filtro 1111, el mismo de los canales del encoder

	TIM2->CCER &= ~TIM_CCER_CC3NP;
	TIM2->CCER |= TIM_CCER_CC3P;			// CC3P = 1, CC3NP = 0: flanco de bajada (se presiona)
	TIM2->CCER |= TIM_CCER_CC3E;			// Habilita la captura del canal 3

	TIM2->SR &= ~TIM_SR_CC3IF;				// Limpia la bandera antes de habilitar la interrupcion
	TIM2->DIER |= TIM_DIER_CC3IE;			// Interrupcion por captura del canal 3
	__NVIC_EnableIRQ(TIM2_IRQn);
}
#else
/*
 * exti0_Init
 * PB0 (SW del KY040) -> EXTI0, flanco de bajada. Es la misma secuencia del
 * boton PC13 (EXTI13), cambia el pin, la linea y el registro EXTICR.
 * PC13 ya tiene pull-up en la tarjeta; PB0 es un cable al KY040, por eso
 * se activa el Pull-Up interno.
 */
static void exti0_Init(void)
{
	/* GPIOB PB0 como entrada con Pull-Up */
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;	// Reloj del GPIOB (AHB1)

	GPIOB->MODER &= ~GPIO_MODER_MODER0;		// 00: entrada

	GPIOB->PUPDR &= ~GPIO_PUPDR_PUPDR0;
	GPIOB->PUPDR |= GPIO_PUPDR_PUPDR0_0;	// 01: Pull-Up, en reposo es 1 y al presionar baja a 0

	/* Conexion de PB0 a la linea EXTI0 */
	RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;	// El mux de EXTI esta en SYSCFG (APB2)

	SYSCFG->EXTICR[0] &= ~SYSCFG_EXTICR1_EXTI0;
	SYSCFG->EXTICR[0] |= SYSCFG_EXTICR1_EXTI0_PB;	// EXTI0 toma el pin 0 del puerto B

	EXTI->RTSR &= ~EXTI_RTSR_TR0;			// Sin disparo por flanco de subida
	EXTI->FTSR |= EXTI_FTSR_TR0;			// Disparo por flanco de bajada (el boton conecta PB0 a GND)

	EXTI->PR = EXTI_PR_PR0;					// Limpia la bandera pendiente (se limpia escribiendo 1)
	EXTI->IMR |= EXTI_IMR_MR0;				// Desenmascara la linea 0

	__NVIC_EnableIRQ(EXTI0_IRQn);			// Habilita la interrupcion de la linea 0 en el NVIC
}
#endif

/*
 * pwm_SetFrequency
 * ARR nuevo desde arr_table. Con ARPE = 1 se aplica al terminar el periodo actual
 */
static void pwm_SetFrequency(uint8_t index)
{
	if (index < NUM_FREQ)
	{
		TIM1->ARR = arr_table[index];
	}
}

/*
 * pwm_SetDuty
 * CCR1 = (ARR + 1) x duty / 100
 * 0 % -> CCR1 = 0 (siempre bajo), 100 % -> CCR1 = ARR + 1 (siempre alto)
 */
static void pwm_SetDuty(uint8_t percent)
{
	uint32_t period_ticks = 0;

	if (percent > DUTY_MAX)
	{
		percent = DUTY_MAX;
	}

	period_ticks = (uint32_t) arr_table[freq_index] + 1;
	TIM1->CCR1 = (period_ticks * percent) / DUTY_MAX;
}

/* Interrupciones */

/* Blinky: cada 500 ms alterna PH1 */
void TIM4_IRQHandler(void)
{
	if (TIM4->SR & TIM_SR_UIF)
	{
		TIM4->SR &= ~TIM_SR_UIF;			// Limpia la bandera
		GPIOH->ODR ^= GPIO_ODR_OD1;
	}
}

#if SW_USE_TIM2_CH3
/* Pulsador por captura del canal 3 del TIM2 */
void TIM2_IRQHandler(void)
{
	if (TIM2->SR & TIM_SR_CC3IF)
	{
		TIM2->SR &= ~TIM_SR_CC3IF;			// Limpia la bandera
		button_event = 1;
	}
}
#else
/* Pulsador por EXTI0 */
void EXTI0_IRQHandler(void)
{
	if (EXTI->PR & EXTI_PR_PR0)				// Verifica que la linea 0 genero la interrupcion
	{
		EXTI->PR = EXTI_PR_PR0;				// Limpia la bandera pendiente (escribiendo 1)
		button_event = 1;
	}
}
#endif
