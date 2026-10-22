/**
 ******************************************************************************
 * @file           : main.c
 * @author         : Miguel A. Bedoya Gonzalez --> mibedoyag@unal.edu.co
 * @brief          : Taller PWM - Encoder KY040 (TIM2) cambia el ARR o el CCR1 del
 *                   PWM (TIM1) y el pulsador (EXTI0) elige cuál de los dos,
 *                   organizado como una Máquina de Estados Finitos (FSM)
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
 ** PULSADOR KY040 (SW), activo en bajo, sin antirrebote:
 * - SW -> PB0 (EXTI0)
 *
 ** BLINKY TIM4 -> PH1 (4 Hz en modo CCR, 2 Hz en modo ARR)
 *
 *
 * ===================== FUNCIONAMIENTO ===================== *
 *
 * - TIM2 en modo encoder cuenta los flancos de CLK y DT (4 cuentas por paso).
 *   Cada paso del encoder sube o baja en una cantidad fija el valor que se esté
 *   editando.
 *
 * - TIM1 genera la señal PWM en PA8. Con PSC = 15 cada tick dura 1 us:
 *   periodo = (ARR + 1) us, ancho del pulso = CCR1 us. Arranca con ARR = 1999 y
 *   CCR1 = 1000: 500 Hz con duty de 50 %.
 *
 * - Hay dos modos de edición y cada flanco de bajada del pulsador cambia de uno a otro:
 *   MODE_CCR (inicial): cada paso del encoder suma o resta 10 al CCR1 (ancho del pulso).
 *   MODE_ARR          : cada paso del encoder suma o resta 20 al ARR (periodo).
 *   El ARR se satura entre 99 y 65534, el CCR1 entre 0 y ARR + 1.
 *
 * - TIM4 es la base de tiempo: interrumpe cada 1 ms y cuenta milisegundos. El LED PH1 muestra
 *   el modo: parpadea a 4 Hz (cambia cada 125 ms) en MODE_CCR y a 2 Hz (cambia cada 250 ms) en MODE_ARR.
 *
 * - Reloj del sistema: HSI 16 MHz (sin PLL), los timers reciben 16 MHz.
 *
 *
 * ===================== FSM (presentación 05-FSM) ===================== *
 *
 * Patrón 1: Switch-Case. La FSM decide QUÉ pasa (política), las funciones
 * driver hacen el CÓMO (hardware). Las ISR solo levantan banderas volatile,
 * get_next_event() las convierte en un EVENTO y el while(1) nunca se bloquea.
 *
 *   Estado actual      Evento               Estado siguiente    Acción [ENTER]
 *   -----------------  -------------------  ------------------  --------------------------
 *   STATE_IDLE         EV_ENCODER_CHANGED   STATE_UPDATE_PWM    -
 *   STATE_IDLE         EV_BUTTON_PRESS      STATE_CHANGE_MODE   -
 *   STATE_IDLE         EV_BLINK_TICK        STATE_BLINK         -
 *   STATE_UPDATE_PWM   (incondicional)      STATE_IDLE          pwm_ApplySteps(encoder_TakeSteps())
 *   STATE_CHANGE_MODE  (incondicional)      STATE_IDLE          mode_Toggle()
 *   STATE_BLINK        (incondicional)      STATE_IDLE          led_Toggle()
 *   STATE_ERROR        (cualquiera)         STATE_IDLE          recuperación
 *
 ******************************************************************************
 */

#include <stdint.h> //Tipos de ancho fijo (uint8_t, int32_t...) para que el tamaño de cada variable sea conocido
#include <stm32f4xx.h> //CMSIS del STM32F4: define los registros (RCC, GPIOx, TIMx, EXTI...) y sus máscaras de bits

/* Constantes */
#define COUNTS_PER_STEP	4 //El modo TI12 cuenta 4 flancos por cada paso mecánico del encoder
#define ARR_INIT		1999 //ARR inicial: (1999 + 1) x 1 us = 2000 us -> 500 Hz
#define CCR_INIT		0 //CCR1 inicial = 0
#define ARR_STEP		20 //Lo que suma o resta al ARR cada paso del encoder en MODE_ARR
#define ARR_MIN			99 //ARR mínimo: (99 + 1) x 1 us = 100 us -> 10 kHz (con ARR = 0 el contador queda bloqueado)
#define ARR_MAX			65534 //ARR máximo: periodo de unos 65 ms. TIM1 es de 16 bit y ARR + 1 (el CCR1 máximo) debe caber en 16 bit: con 65535 daría 65536, que se trunca a 0
#define CCR_STEP		10 //Lo que suma o resta al CCR1 cada paso del encoder en MODE_CCR
#define BLINK_CCR_MS	125 //PH1 cambia cada 125 ms en MODE_CCR: periodo de 250 ms -> 4 Hz
#define BLINK_ARR_MS	250 //PH1 cambia cada 250 ms en MODE_ARR: periodo de 500 ms -> 2 Hz

/* Máquina de estados finitos (FSM) */

/* Paso 1 (FSM): mapear el vocabulario a memoria. Estados y eventos son enum (type safety)
 * y NUM_STATES / NUM_EVENTS son centinelas para validar rangos. */

//MODOS DE EDICIÓN: qué valor del PWM modifica el encoder
typedef enum { //Lista de modos posibles
	MODE_CCR, //El encoder cambia el CCR1 (ancho del pulso), blinky a 4 Hz
	MODE_ARR //El encoder cambia el ARR (periodo), blinky a 2 Hz
} Mode_t; //Nombre del tipo: edit_mode solo puede recibir valores de este enum

//ESTADOS: la condición en la que está el sistema en un instante dado
typedef enum { //Lista de estados posibles, cada nombre vale un entero (0, 1, 2...)
	STATE_IDLE, //Reposo: espera un evento sin hacer nada más (no bloquea)
	STATE_UPDATE_PWM, //Atiende el giro del encoder: suma o resta al ARR o al CCR1 del PWM según el modo
	STATE_CHANGE_MODE, //Atiende el pulsador: cambia entre editar el CCR y editar el ARR
	STATE_BLINK, //Atiende el tick del TIM4: cambia el estado del LED PH1
	STATE_ERROR, //Estado inválido (trampa de hardware): se recupera hacia STATE_IDLE
	NUM_STATES //Centinela: cantidad de estados, sirve para validar el rango
} State_t; //Nombre del tipo: current_state solo puede recibir valores de este enum

//EVENTOS: los disparadores (ISR o lecturas) que provocan transiciones entre estados
typedef enum { //Lista de eventos posibles, cada nombre vale un entero (0, 1, 2...)
	EV_NONE, //No ha ocurrido nada
	EV_ENCODER_CHANGED, //El encoder avanzó o retrocedió al menos un paso
	EV_BUTTON_PRESS, //El pulsador del KY040 fue presionado (flanco de bajada)
	EV_BLINK_TICK, //Venció el medio periodo del blinky (125 ms en MODE_CCR, 250 ms en MODE_ARR)
	NUM_EVENTS //Centinela: cantidad de eventos, sirve para validar el rango
} Event_t; //Nombre del tipo: current_event solo puede recibir valores de este enum

/* Variables */

//Estado actual de la FSM, static: solo se accede desde este módulo (scope mínimo) y solo se modifica en main()
static State_t current_state = STATE_IDLE; //Siempre inicia en reposo
//Bandera de ENTRY: vale 1 al llegar a un estado para que su acción [ENTER] se ejecute exactamente una vez
static uint8_t state_entry_flag = 1; //Inicia en 1 para que el primer estado también ejecute su [ENTER]

volatile uint8_t button_flag = 0; //Bandera que deja la ISR del pulsador (volatile: cambia fuera del flujo normal del programa)
volatile uint8_t blink_flag = 0; //Bandera que deja la ISR del TIM4 (volatile por la misma razón)
volatile uint16_t blink_half_period_ms = BLINK_CCR_MS; //Milisegundos entre dos cambios del LED: la ISR del TIM4 lo lee y el main lo cambia (volatile)

static Mode_t edit_mode = MODE_CCR; //Modo de edición actual, inicia editando el CCR
int32_t arr_value = ARR_INIT; //ARR que tiene el TIM1 (periodo = ARR + 1 ticks de 1 us), con signo para poder comparar con los límites
int32_t ccr_value = CCR_INIT; //CCR1 que tiene el TIM1 (ancho del pulso en ticks de 1 us), inicia en la mitad del periodo: duty de 50 %

/* Definición de funciones */

//Drivers de la FSM (el CÓMO)
static Event_t get_next_event(void); //Lee (y limpia) las banderas de las ISR y la lectura del encoder, y las convierte en un evento
static int32_t encoder_TakeSteps(void); //Devuelve los pasos acumulados (con signo) y los descuenta del TIM2
static void pwm_ApplySteps(int32_t steps); //Suma steps x ARR_STEP al ARR o steps x CCR_STEP al CCR1 según el modo, con saturación
static void mode_Toggle(void); //Cambia entre MODE_CCR y MODE_ARR y ajusta la velocidad del blinky
static void led_Toggle(void); //Cambia el estado del LED PH1

//Inicialización del hardware
static void gpio_Init(void); //GPIOH PH1 como salida (blinky)
static void tim4_Init(void); //TIM4 con interrupción cada 1 ms (base de tiempo del blinky)
static void tim1_pwm_Init(void); //GPIOA PA8 + TIM1 CH1 en modo PWM 1
static void tim2_encoder_Init(void); //GPIOA PA0, PA1 + TIM2 en modo encoder
static void exti0_Init(void); //GPIOB PB0 + EXTI0 (flanco de bajada)

/* Main */
int main(void) //Punto de entrada: el startup lo llama después de copiar .data y limpiar .bss
{
	Event_t current_event = EV_NONE; //Evento que se evalúa en cada pasada del ciclo infinito

	gpio_Init(); //Configura PH1 (blinky)
	tim1_pwm_Init(); //Configura el PWM en PA8
	tim2_encoder_Init(); //Configura el encoder
	exti0_Init(); //Configura el pulsador por EXTI0 en PB0
	tim4_Init(); //Último: las interrupciones del blinky empiezan con todo el hardware listo

	/* Patrón 1 (FSM): Switch-Case. El ciclo infinito nunca termina: lee un evento, evalúa el estado y reacciona.
	 * Regla de NO bloqueo: no hay delay(), cada pasada procesa a lo sumo UN evento y UNA transición. */
	while (1) //El firmware nunca termina, gira todo el tiempo evaluando eventos
	{
		if (current_state == STATE_IDLE) //Solo en reposo se consumen eventos: los otros estados duran una pasada y no usan el evento
		{
			current_event = get_next_event(); //Consulta (sin bloquear) si hay un evento pendiente
		}
		else //En un estado de atención no se toca ninguna bandera
		{
			current_event = EV_NONE; //Si se consumiera aquí, el evento se perdería sin que nadie lo atienda
		}

		//Safeguard: valida estado y evento antes de usarlos, si alguno está fuera de rango se recupera con STATE_ERROR
		if ((current_state >= NUM_STATES) || (current_event >= NUM_EVENTS)) //Un valor fuera de rango sólo pasa por corrupción de memoria
		{
			current_state = STATE_ERROR; //Se fuerza el estado de error para recuperarse de forma segura
			state_entry_flag = 1; //El estado de error también ejecuta su [ENTER]
		}

		switch (current_state) //Despacha al código del estado actual
		{

		case STATE_IDLE: //Estado de reposo
			//[ENTER] Corre una sola vez al llegar al estado (aquí no hay nada que preparar)
			if (state_entry_flag) //Solo es 1 en la primera pasada después de llegar al estado
			{
				state_entry_flag = 0; //Se baja para que [ENTER] no vuelva a ejecutarse mientras se siga en este estado
			}

			//[DO] Corre libremente en cada pasada: en reposo no hace nada, solo espera un evento sin bloquear

			//Transiciones: cada evento lleva a un estado distinto
			if (current_event == EV_ENCODER_CHANGED) //Se giró el encoder
			{
				current_state = STATE_UPDATE_PWM; //[EXIT] No hay acción de salida en este estado
				state_entry_flag = 1; //Se marca para que el estado nuevo ejecute su [ENTER]
			}
			else if (current_event == EV_BUTTON_PRESS) //Se presionó el pulsador
			{
				current_state = STATE_CHANGE_MODE; //Pasa a atender el cambio de modo
				state_entry_flag = 1; //Se marca para que el estado nuevo ejecute su [ENTER]
			}
			else if (current_event == EV_BLINK_TICK) //Venció el medio periodo del blinky
			{
				current_state = STATE_BLINK; //Pasa a cambiar el LED
				state_entry_flag = 1; //Se marca para que el estado nuevo ejecute su [ENTER]
			}
			break; //Sale del switch, la pasada termina y el while(1) vuelve a empezar

		case STATE_UPDATE_PWM: //Estado que atiende el giro del encoder
			//[ENTER] Acción de entrada (Moore): cambia el ARR o el CCR1, exactamente una vez por cada evento del encoder
			if (state_entry_flag) //Solo la primera pasada después de llegar a este estado
			{
				pwm_ApplySteps(encoder_TakeSteps()); //Toma los pasos del encoder y los aplica al valor que indica edit_mode
				state_entry_flag = 0; //Baja la bandera: la acción ya se ejecutó
			}
			//Transición incondicional: la tarea ya fue atendida, se regresa al reposo
			current_state = STATE_IDLE; //Vuelve a esperar eventos
			state_entry_flag = 1; //IDLE ejecutará su [ENTER]
			break; //Fin del estado

		case STATE_CHANGE_MODE: //Estado que atiende el pulsador
			//[ENTER] Acción de entrada (Moore): cambia de modo, exactamente una vez por cada pulsación
			if (state_entry_flag) //Solo la primera pasada después de llegar a este estado
			{
				mode_Toggle(); //MODE_CCR <-> MODE_ARR y velocidad del blinky (4 Hz o 2 Hz)
				state_entry_flag = 0; //Baja la bandera: la acción ya se ejecutó
			}
			//Transición incondicional: la tarea ya fue atendida, se regresa al reposo
			current_state = STATE_IDLE; //Vuelve a esperar eventos
			state_entry_flag = 1; //IDLE ejecutará su [ENTER]
			break; //Fin del estado

		case STATE_BLINK: //Estado que atiende el tick del blinky
			//[ENTER] Acción de entrada (Moore): cambia el LED, exactamente una vez por cada evento del blinky
			if (state_entry_flag) //Solo la primera pasada después de llegar a este estado
			{
				led_Toggle(); //Invierte el estado de PH1
				state_entry_flag = 0; //Baja la bandera: la acción ya se ejecutó
			}
			//Transición incondicional: la tarea ya fue atendida, se regresa al reposo
			current_state = STATE_IDLE; //Vuelve a esperar eventos
			state_entry_flag = 1; //IDLE ejecutará su [ENTER]
			break; //Fin del estado

		case STATE_ERROR: //STATE_ERROR comparte código con default porque la recuperación es la misma
		default: //Trampa (default:): cualquier estado desconocido se recupera de forma segura hacia el reposo
			current_state = STATE_IDLE; //Reinicia la FSM en un estado conocido
			state_entry_flag = 1; //IDLE ejecutará su [ENTER]
			break; //Fin del estado
		}
	}
}

/* Funciones driver de la FSM (el CÓMO) */

//Convierte las banderas de las ISR y el encoder en UN evento. Prioridad: pulsador > encoder > blinky
static Event_t get_next_event(void)
{
	if (button_flag == 1) //La ISR del pulsador dejó un evento pendiente
	{
		button_flag = 0; //Se baja la bandera: el evento se atiende una sola vez
		return EV_BUTTON_PRESS;
	}

	int32_t counts = (int32_t) TIM2->CNT; //Con signo: al girar en CCW desde 0, CNT pasa a 0xFFFFFFFF, que es -1
	if ((counts >= COUNTS_PER_STEP) || (counts <= -COUNTS_PER_STEP)) //Hay al menos un paso completo, CW o CCW
	{
		return EV_ENCODER_CHANGED;
	}

	if (blink_flag == 1) //La ISR del TIM4 dejó un evento pendiente
	{
		blink_flag = 0;
		return EV_BLINK_TICK;
	}

	return EV_NONE;
}

//Devuelve los pasos completos acumulados (+ CW, - CCW) y los descuenta del TIM2
static int32_t encoder_TakeSteps(void)
{
	int32_t steps = (int32_t) TIM2->CNT / COUNTS_PER_STEP; //División entera: 7 cuentas = 1 paso, -7 cuentas = -1 paso
	TIM2->CNT -= steps * COUNTS_PER_STEP; //Se descuentan solo las cuentas usadas, el residuo queda para el siguiente giro
	return steps;
}

//Suma los pasos al ARR o al CCR1 según el modo, y los limita
static void pwm_ApplySteps(int32_t steps)
{
	if (edit_mode == MODE_CCR)
	{
		ccr_value += steps * CCR_STEP; //Cambia el ancho del pulso
	}
	else
	{
		arr_value += steps * ARR_STEP; //Cambia el periodo
	}

	if (arr_value < ARR_MIN) arr_value = ARR_MIN; //El ARR se queda entre ARR_MIN y ARR_MAX
	if (arr_value > ARR_MAX) arr_value = ARR_MAX;
	if (ccr_value < 0) ccr_value = 0; //El CCR1 se queda entre 0 y ARR + 1
	if (ccr_value > arr_value + 1) ccr_value = arr_value + 1;

	TIM1->ARR = arr_value; //Con ARPE = 1 y OC1PE = 1 entran al terminar el periodo actual
	TIM1->CCR1 = ccr_value;
}

//Cambia el valor que edita el encoder y la velocidad del blinky (CCR: 4 Hz, ARR: 2 Hz)
static void mode_Toggle(void)
{
	if (edit_mode == MODE_CCR)
	{
		edit_mode = MODE_ARR;
		blink_half_period_ms = BLINK_ARR_MS;
	}
	else
	{
		edit_mode = MODE_CCR;
		blink_half_period_ms = BLINK_CCR_MS;
	}
}

//Cambia el estado del LED PH1
static void led_Toggle(void)
{
	GPIOH->ODR ^= GPIO_ODR_OD1; //XOR con 1: invierte el bit sin tocar los otros pines
}

/* Funciones de inicialización del hardware (se asume el estado de reset de cada registro) */

//PH1 como salida (LED del blinky)
static void gpio_Init(void)
{
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOHEN; //Reloj del GPIOH
	GPIOH->MODER |= GPIO_MODER_MODER1_0; //01: salida
}

//Interrupción cada 1 ms: PSC = 15 -> tick de 1 us, ARR = 999 -> 1000 us
static void tim4_Init(void)
{
	RCC->APB1ENR |= RCC_APB1ENR_TIM4EN; //Reloj del TIM4

	TIM4->PSC = 15; //16 MHz / 16 = 1 MHz
	TIM4->ARR = 999; //Cuenta de 0 a 999 (ARR = 0 bloquearía el contador)

	TIM4->DIER |= TIM_DIER_UIE; //Habilita la interrupción por actualización (primer interruptor)
	__NVIC_EnableIRQ(TIM4_IRQn); //Habilita la interrupción en el NVIC (segundo interruptor)

	TIM4->CR1 |= TIM_CR1_CEN; //Arranca el contador, siempre de último
}

//PA8 -> TIM1_CH1 (AF1), PWM modo 1, 500 Hz, duty 50 %
static void tim1_pwm_Init(void)
{
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN; //Reloj del GPIOA
	GPIOA->MODER |= GPIO_MODER_MODER8_1; //10: función alterna
	GPIOA->AFR[1] |= GPIO_AFRH_AFSEL8_0; //AF1 (los pines 8 a 15 están en AFR[1])

	RCC->APB2ENR |= RCC_APB2ENR_TIM1EN; //Reloj del TIM1 (bus APB2)

	TIM1->PSC = 15; //Tick de 1 us
	TIM1->ARR = ARR_INIT; //Periodo: (1999 + 1) us
	TIM1->CCR1 = CCR_INIT; //Ancho del pulso
	TIM1->CR1 |= TIM_CR1_ARPE; //ARR con preload: el cambio de periodo no corta pulsos

	TIM1->CCMR1 |= (TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2); //110: PWM modo 1, alto mientras CNT < CCR1
	TIM1->CCMR1 |= TIM_CCMR1_OC1PE; //CCR1 con preload
	TIM1->CCER |= TIM_CCER_CC1E; //Habilita la salida del canal 1 al pin
	TIM1->BDTR |= TIM_BDTR_MOE; //TIM1 es avanzado: sin MOE las salidas no funcionan

	TIM1->EGR |= TIM_EGR_UG; //Carga PSC, ARR y CCR1 a los registros activos
	TIM1->CR1 |= TIM_CR1_CEN; //Arranca el contador, siempre de último
}

//PA0 -> TIM2_CH1 (CLK), PA1 -> TIM2_CH2 (DT), AF1 con pull-up. Modo encoder: 4 cuentas por paso
static void tim2_encoder_Init(void)
{
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN; //Reloj del GPIOA
	GPIOA->MODER |= (GPIO_MODER_MODER0_1 | GPIO_MODER_MODER1_1); //10: función alterna
	GPIOA->PUPDR |= (GPIO_PUPDR_PUPDR0_0 | GPIO_PUPDR_PUPDR1_0); //01: pull-up
	GPIOA->AFR[0] |= (GPIO_AFRL_AFSEL0_0 | GPIO_AFRL_AFSEL1_0); //AF1

	RCC->APB1ENR |= RCC_APB1ENR_TIM2EN; //Reloj del TIM2

	TIM2->ARR = 0xFFFFFFFF; //TIM2 es de 32 bit: no se desborda en uso normal
	TIM2->CCMR1 |= (TIM_CCMR1_CC1S_0 | TIM_CCMR1_CC2S_0); //01: CH1 lee TI1 y CH2 lee TI2
	TIM2->CCMR1 |= (TIM_CCMR1_IC1F | TIM_CCMR1_IC2F); //Filtro digital máximo: rechaza el rebote del encoder
	TIM2->CCER |= (TIM_CCER_CC1E | TIM_CCER_CC2E); //Habilita la captura de los dos canales
	TIM2->SMCR |= (TIM_SMCR_SMS_0 | TIM_SMCR_SMS_1); //011: modo encoder 3, cuenta los flancos de TI1 y TI2

	TIM2->CR1 |= TIM_CR1_CEN; //Arranca el contador
}

//PB0 (SW del KY040) -> EXTI0, flanco de bajada
static void exti0_Init(void)
{
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN; //Reloj del GPIOB (PB0 queda como entrada, estado de reset)
	GPIOB->PUPDR |= GPIO_PUPDR_PUPDR0_0; //01: pull-up, en reposo lee 1 y al presionar baja a 0

	RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN; //Reloj del SYSCFG, el multiplexor de EXTI vive ahí
	SYSCFG->EXTICR[0] |= SYSCFG_EXTICR1_EXTI0_PB; //La línea EXTI0 toma el pin 0 del puerto B

	EXTI->FTSR |= EXTI_FTSR_TR0; //Disparo por flanco de bajada
	EXTI->IMR |= EXTI_IMR_MR0; //Desenmascara la línea 0
	__NVIC_EnableIRQ(EXTI0_IRQn); //Habilita la interrupción en el NVIC
}

/* Funciones ISR (solo registran el evento y regresan, sin lógica de la FSM) */

//Cada 1 ms cuenta el milisegundo y cada blink_half_period_ms deja la bandera del blinky
void TIM4_IRQHandler(void)
{
	static uint16_t blink_count = 0; //static: conserva su valor entre interrupciones

	TIM4->SR &= ~TIM_SR_UIF; //Baja la bandera (escribiendo 0) para que la interrupción se pueda repetir
	blink_count++;
	if (blink_count >= blink_half_period_ms)
	{
		blink_count = 0;
		blink_flag = 1; //La FSM lo convierte en EV_BLINK_TICK
	}
}

//Pulsador por EXTI0
void EXTI0_IRQHandler(void)
{
	EXTI->PR = EXTI_PR_PR0; //Se baja escribiendo 1 (en EXTI->PR se limpia con 1, no con 0)
	button_flag = 1; //La FSM lo convierte en EV_BUTTON_PRESS
}

