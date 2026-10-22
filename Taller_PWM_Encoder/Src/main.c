/**
 ******************************************************************************
 * @file           : main.c
 * @author         : Miguel A. Bedoya Gonzalez --> mibedoyag@unal.edu.co
 * @brief          : Taller PWM - Encoder KY040 (TIM2) controla el duty y el
 *                   pulsador (EXTI0) cambia la frecuencia del PWM (TIM1),
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
 ** PULSADOR KY040 (SW), activo en bajo, antirrebote por software (sin capacitor):
 * - SW_USE_TIM2_CH3 = 0 -> PB0 (EXTI0)                        [por defecto]
 * - SW_USE_TIM2_CH3 = 1 -> PA2 (TIM2_CH3 - AF01, captura con filtro IC3F)
 *
 ** BLINKY TIM4 -> PH1 (500 ms)
 *
 *
 * ===================== FUNCIONAMIENTO ===================== *
 *
 * - TIM2 en modo encoder cuenta los flancos de CLK y DT (4 cuentas por paso).
 *   La cuenta se divide entre 4 y se satura entre 0 y 100: es el duty en %.
 *
 * - TIM1 genera la señal PWM en PA8. Su frecuencia es 500 Hz, 1 kHz o 2 kHz
 *   (tabla arr_table). El CCR1 se calcula siempre con el ARR actual, por eso el
 *   duty en % se conserva al cambiar de frecuencia.
 *
 * - Cada flanco de bajada del pulsador pasa a la siguiente frecuencia:
 *   500 Hz -> 1 kHz -> 2 kHz -> 500 Hz ...
 *
 * - TIM4 es la base de tiempo: interrumpe cada 1 ms, cuenta milisegundos (ms_ticks)
 *   y cada 500 ms hace parpadear PH1 para saber que el programa corre.
 *
 * - Antirrebote del pulsador por software: un contacto mecánico rebota de 1 a 10 ms
 *   y genera varios flancos por una sola pulsación. get_next_event() acepta una
 *   pulsación solo si pasaron DEBOUNCE_MS (50 ms) desde la última aceptada.
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
 *   STATE_IDLE         EV_ENCODER_CHANGED   STATE_UPDATE_DUTY   -
 *   STATE_IDLE         EV_BUTTON_PRESS      STATE_CHANGE_FREQ   -
 *   STATE_IDLE         EV_BLINK_TICK        STATE_BLINK         -
 *   STATE_UPDATE_DUTY  (incondicional)      STATE_IDLE          pwm_SetDuty(duty_percent)
 *   STATE_CHANGE_FREQ  (incondicional)      STATE_IDLE          siguiente frecuencia + duty
 *   STATE_BLINK        (incondicional)      STATE_IDLE          led_Toggle()
 *   STATE_ERROR        (cualquiera)         STATE_IDLE          recuperación
 *
 ******************************************************************************
 */

#include <stdint.h> //Tipos de ancho fijo (uint8_t, int32_t...) para que el tamaño de cada variable sea conocido
#include <stm32f4xx.h> //CMSIS del STM32F4: define los registros (RCC, GPIOx, TIMx, EXTI...) y sus máscaras de bits

/* Selección del pulsador: 0 = EXTI0 en PB0, 1 = captura del TIM2_CH3 en PA2 */
#define SW_USE_TIM2_CH3	0 //Con 0 se compila la versión con EXTI, con 1 la versión con captura del canal 3

/* Lectura del nivel del pulsador: un pico de ruido dura microsegundos, una pulsación real mantiene el pin en bajo */
#if SW_USE_TIM2_CH3
#define SW_PIN_IS_LOW()	((GPIOA->IDR & GPIO_IDR_ID2) == 0) //Versión captura: el pulsador está en PA2
#else
#define SW_PIN_IS_LOW()	((GPIOB->IDR & GPIO_IDR_ID0) == 0) //Versión EXTI: el pulsador está en PB0
#endif

/* Constantes */
#define DEBOUNCE_MS		50 //Tiempo mínimo entre dos pulsaciones válidas: mayor que el rebote (1-10 ms), menor que una pulsación humana (>100 ms)
#define BLINK_PERIOD_MS	500 //Periodo del blinky de PH1 en milisegundos (se cuenta con ms_ticks)
#define NUM_FREQ		3 //Cantidad de frecuencias disponibles para el PWM (500 Hz, 1 kHz, 2 kHz)
#define DUTY_MAX		100 //Duty máximo en porcentaje
#define COUNTS_PER_STEP	4 //El modo TI12 cuenta 4 flancos por cada paso mecánico del encoder
#define ENCODER_MAX_CNT	(DUTY_MAX * COUNTS_PER_STEP) //Cuenta máxima del TIM2: 100 pasos x 4 cuentas = 400

/* Máquina de estados finitos (FSM) */

/* Paso 1 (FSM): mapear el vocabulario a memoria. Estados y eventos son enum (type safety)
 * y NUM_STATES / NUM_EVENTS son centinelas para validar rangos. */

//ESTADOS: la condición en la que está el sistema en un instante dado
typedef enum { //Lista de estados posibles, cada nombre vale un entero (0, 1, 2...)
	STATE_IDLE, //Reposo: espera un evento sin hacer nada más (no bloquea)
	STATE_UPDATE_DUTY, //Atiende el giro del encoder: actualiza el duty del PWM
	STATE_CHANGE_FREQ, //Atiende el pulsador: pasa a la siguiente frecuencia del PWM
	STATE_BLINK, //Atiende el tick de 500 ms: cambia el estado del LED PH1
	STATE_ERROR, //Estado inválido (trampa de hardware): se recupera hacia STATE_IDLE
	NUM_STATES //Centinela: cantidad de estados, sirve para validar el rango
} State_t; //Nombre del tipo: current_state solo puede recibir valores de este enum

//EVENTOS: los disparadores (ISR o lecturas) que provocan transiciones entre estados
typedef enum { //Lista de eventos posibles, cada nombre vale un entero (0, 1, 2...)
	EV_NONE, //No ha ocurrido nada
	EV_ENCODER_CHANGED, //El valor del encoder (en %) cambió respecto al duty actual
	EV_BUTTON_PRESS, //El pulsador del KY040 fue presionado (flanco de bajada ya filtrado contra rebotes)
	EV_BLINK_TICK, //Venció el tiempo del TIM4 (500 ms)
	NUM_EVENTS //Centinela: cantidad de eventos, sirve para validar el rango
} Event_t; //Nombre del tipo: current_event solo puede recibir valores de este enum

/* Variables */

//Estado actual de la FSM, static: solo se accede desde este módulo (scope mínimo) y solo se modifica en main()
static State_t current_state = STATE_IDLE; //Siempre inicia en reposo
//Bandera de ENTRY: vale 1 al llegar a un estado para que su acción [ENTER] se ejecute exactamente una vez
static uint8_t state_entry_flag = 1; //Inicia en 1 para que el primer estado también ejecute su [ENTER]

volatile uint8_t button_flag = 0; //Bandera que deja la ISR del pulsador (volatile: cambia fuera del flujo normal del programa)
volatile uint8_t blink_flag = 0; //Bandera que deja la ISR del TIM4 (volatile por la misma razón)
volatile uint32_t ms_ticks = 0; //Milisegundos desde el arranque: los incrementa la ISR del TIM4 cada 1 ms (volatile: cambia fuera del flujo normal)
static uint32_t last_button_ms = 0 - DEBOUNCE_MS; //Instante de la última pulsación aceptada; el valor inicial hace que la primera pulsación siempre pase (resta sin signo)

uint8_t freq_index = 0; //Índice de la frecuencia actual dentro de arr_table
uint8_t duty_percent = 0; //Duty actual del PWM en porcentaje (0 - 100)
const uint16_t arr_table[NUM_FREQ] = {1999, 999, 499}; //ARR para 500 Hz, 1 kHz y 2 kHz con tick de 1 us, const: vive en la Flash

/* Definición de funciones */

//Drivers de la FSM (el CÓMO)
static Event_t get_next_event(void); //Lee (y limpia) las banderas de las ISR y la lectura del encoder, y las convierte en un evento
static uint8_t encoder_ReadPercent(void); //Lee el TIM2, satura la cuenta entre 0 y 400 y la devuelve como porcentaje (0 - 100)
static void pwm_SetFrequency(uint8_t index); //Cambia el ARR del TIM1 según arr_table
static void pwm_SetDuty(uint8_t percent); //Calcula el CCR1 a partir del porcentaje y del ARR actual
static void led_Toggle(void); //Cambia el estado del LED PH1

//Inicialización del hardware
static void gpio_Init(void); //GPIOH PH1 como salida (blinky)
static void tim4_Init(void); //TIM4 con interrupción cada 1 ms (base de tiempo para el antirrebote y el blinky de 500 ms)
static void tim1_pwm_Init(void); //GPIOA PA8 + TIM1 CH1 en modo PWM 1
static void tim2_encoder_Init(void); //GPIOA PA0, PA1 + TIM2 en modo encoder
#if SW_USE_TIM2_CH3 //Compilación condicional: solo existe la versión elegida con SW_USE_TIM2_CH3
static void sw_tim2ch3_Config(void); //GPIOA PA2 + canal 3 del TIM2 como captura con filtro
#else
static void exti0_Init(void); //GPIOB PB0 + EXTI0 (flanco de bajada)
#endif

/* Main */
int main(void) //Punto de entrada: el startup lo llama después de copiar .data y limpiar .bss
{
	Event_t current_event = EV_NONE; //Evento que se evalúa en cada pasada del ciclo infinito

	gpio_Init(); //Configura PH1 (blinky)
	tim1_pwm_Init(); //Configura el PWM en PA8
	tim2_encoder_Init(); //Configura el encoder (con SW_USE_TIM2_CH3 = 1 también configura el pulsador)
#if !SW_USE_TIM2_CH3 //Solo se llama si el pulsador va por EXTI
	exti0_Init(); //Configura el pulsador por EXTI0 en PB0
#endif
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
				current_state = STATE_UPDATE_DUTY; //[EXIT] No hay acción de salida en este estado
				state_entry_flag = 1; //Se marca para que el estado nuevo ejecute su [ENTER]
			}
			else if (current_event == EV_BUTTON_PRESS) //Se presionó el pulsador
			{
				current_state = STATE_CHANGE_FREQ; //Pasa a atender el cambio de frecuencia
				state_entry_flag = 1; //Se marca para que el estado nuevo ejecute su [ENTER]
			}
			else if (current_event == EV_BLINK_TICK) //Pasaron 500 ms del TIM4
			{
				current_state = STATE_BLINK; //Pasa a cambiar el LED
				state_entry_flag = 1; //Se marca para que el estado nuevo ejecute su [ENTER]
			}
			break; //Sale del switch, la pasada termina y el while(1) vuelve a empezar

		case STATE_UPDATE_DUTY: //Estado que atiende el giro del encoder
			//[ENTER] Acción de entrada (Moore): actualiza el duty, exactamente una vez por cada evento del encoder
			if (state_entry_flag) //Solo la primera pasada después de llegar a este estado
			{
				duty_percent = encoder_ReadPercent(); //Lee el valor actual del encoder (0 - 100 %) y lo guarda como duty
				pwm_SetDuty(duty_percent); //Escribe el CCR1 correspondiente en el TIM1
				state_entry_flag = 0; //Baja la bandera: la acción ya se ejecutó
			}
			//Transición incondicional: la tarea ya fue atendida, se regresa al reposo
			current_state = STATE_IDLE; //Vuelve a esperar eventos
			state_entry_flag = 1; //IDLE ejecutará su [ENTER]
			break; //Fin del estado

		case STATE_CHANGE_FREQ: //Estado que atiende el pulsador
			//[ENTER] Acción de entrada (Moore): pasa a la siguiente frecuencia, exactamente una vez por cada pulsación
			if (state_entry_flag) //Solo la primera pasada después de llegar a este estado
			{
				freq_index = (uint8_t) ((freq_index + 1) % NUM_FREQ); //Siguiente frecuencia: 0 -> 1 -> 2 -> 0 (el módulo da la vuelta)
				pwm_SetFrequency(freq_index); //Escribe el ARR nuevo en el TIM1
				pwm_SetDuty(duty_percent); //Recalcula el CCR1 con el ARR nuevo para conservar el duty en %
				state_entry_flag = 0; //Baja la bandera: la acción ya se ejecutó
			}
			//Transición incondicional: la tarea ya fue atendida, se regresa al reposo
			current_state = STATE_IDLE; //Vuelve a esperar eventos
			state_entry_flag = 1; //IDLE ejecutará su [ENTER]
			break; //Fin del estado

		case STATE_BLINK: //Estado que atiende el tick del TIM4
			//[ENTER] Acción de entrada (Moore): cambia el LED, exactamente una vez por cada evento del TIM4
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

/*
 * get_next_event
 * Convierte las banderas que dejan las ISR (y el cambio del encoder) en UN evento.
 * Aplica el antirrebote del pulsador (DEBOUNCE_MS) y descarta el ruido leyendo el nivel del pin.
 * Prioridad: pulsador > encoder > blinky, pero un pulsador rechazado (rebote o ruido) NO bloquea a los demás:
 * si el ruido en el pin levantara la bandera todo el tiempo, el encoder y el blinky nunca se atenderían.
 * Las banderas que no se atienden en esta llamada quedan pendientes para la siguiente pasada.
 */
static Event_t get_next_event(void)
{
	Event_t ev = EV_NONE; //Si nada ocurrió, se devuelve EV_NONE
	uint8_t percent = 0; //Valor del encoder en % leído en esta pasada

	percent = encoder_ReadPercent(); //El encoder no genera interrupción: el TIM2 cuenta solo y aquí se lee su valor

	__disable_irq(); //Sección crítica: leer y limpiar una bandera no puede ser interrumpido, o el evento podría perderse
	if (button_flag == 1) //La ISR del pulsador dejó un evento pendiente (puede ser una pulsación real o un rebote)
	{
		button_flag = 0; //Se baja la bandera: ya se atendió el evento
		if (((ms_ticks - last_button_ms) >= DEBOUNCE_MS) && SW_PIN_IS_LOW()) //Antirrebote: resta sin signo (segura al desbordar) de ms desde la última pulsación aceptada, y el pin debe seguir en bajo
		{
			last_button_ms = ms_ticks; //Pulsación válida: se guarda el instante para medir la próxima ventana de 50 ms
			ev = EV_BUTTON_PRESS; //Evento más prioritario
		} //Si pasaron menos de 50 ms (rebote) o el pin ya volvió a alto (ruido) se descarta: ev sigue en EV_NONE
	}

	if ((ev == EV_NONE) && (percent != duty_percent)) //Sin pulsador válido: el encoder marca un valor distinto al duty que está aplicado
	{
		ev = EV_ENCODER_CHANGED; //El duty se actualiza en el [ENTER] de STATE_UPDATE_DUTY
	}

	if ((ev == EV_NONE) && (blink_flag == 1)) //Sin otro evento: la ISR del TIM4 dejó el evento de 500 ms pendiente
	{
		blink_flag = 0; //Se baja la bandera: ya se atendió el evento
		ev = EV_BLINK_TICK; //Evento menos prioritario
	}
	__enable_irq(); //Fin de la sección crítica: las ISR pueden volver a ejecutarse

	return ev; //Devuelve el evento a la FSM
}

/*
 * encoder_ReadPercent
 * Lee el TIM2 (modo encoder), lo satura entre 0 y 400 cuentas y devuelve el duty en %.
 */
static uint8_t encoder_ReadPercent(void)
{
	int32_t raw_count = (int32_t) TIM2->CNT; //CNT es de 32 bit sin signo: al girar en CCW desde 0 pasa a 0xFFFFFFFF, que con signo es -1

	if (raw_count < 0) //Se giró por debajo de 0
	{
		TIM2->CNT = 0; //Se satura en 0 y se reescribe el contador para que no se "acumule" fuera de rango
		raw_count = 0; //La cuenta usada en el cálculo también queda en 0
	}
	else if (raw_count > ENCODER_MAX_CNT) //Se giró por encima de 100 pasos
	{
		TIM2->CNT = ENCODER_MAX_CNT; //Se satura en 400 y se reescribe el contador por la misma razón
		raw_count = ENCODER_MAX_CNT; //La cuenta usada en el cálculo también queda en 400
	}

	return (uint8_t) (raw_count / COUNTS_PER_STEP); //4 cuentas = 1 paso = 1 %
}

/*
 * pwm_SetFrequency
 * ARR nuevo desde arr_table. Con ARPE = 1 se aplica al terminar el periodo actual.
 */
static void pwm_SetFrequency(uint8_t index)
{
	if (index < NUM_FREQ) //Validación: nunca se lee fuera de arr_table
	{
		TIM1->ARR = arr_table[index]; //Escribe el nuevo periodo: (ARR + 1) x 1 us
	}
}

/*
 * pwm_SetDuty
 * CCR1 = (ARR + 1) x duty / 100
 * 0 % -> CCR1 = 0 (siempre bajo), 100 % -> CCR1 = ARR + 1 (siempre alto)
 */
static void pwm_SetDuty(uint8_t percent)
{
	uint32_t period_ticks = 0; //Cantidad de ticks de 1 us que dura un periodo

	if (percent > DUTY_MAX) //Validación: el duty nunca pasa de 100 %
	{
		percent = DUTY_MAX; //Se satura en el máximo
	}

	period_ticks = (uint32_t) arr_table[freq_index] + 1; //El periodo es ARR + 1 ticks porque el contador cuenta desde 0 hasta ARR
	TIM1->CCR1 = (period_ticks * percent) / DUTY_MAX; //Regla de tres: ancho del pulso en ticks para ese porcentaje
}

/*
 * led_Toggle
 * Cambia el estado del LED de la tarjeta (PH1).
 */
static void led_Toggle(void)
{
	GPIOH->ODR ^= GPIO_ODR_OD1; //XOR con 1: invierte el bit sin tocar los otros pines del puerto
}

/* Funciones de inicialización del hardware */

/*
 * gpio_Init
 * PH1 como salida push-pull, LED de la tarjeta (blinky)
 */
static void gpio_Init(void)
{
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOHEN; //Activa la señal de reloj del GPIOH (bus AHB1), sin reloj el puerto ignora las escrituras

	GPIOH->MODER &= ~GPIO_MODER_MODER1; //Limpia los 2 bits de modo del pin 1 antes de asignar
	GPIOH->MODER |= GPIO_MODER_MODER1_0; //01: salida de propósito general
	GPIOH->OTYPER &= ~GPIO_OTYPER_OT1; //0: push-pull (el pin maneja alto y bajo)
	GPIOH->OSPEEDR &= ~GPIO_OSPEEDER_OSPEEDR1; //00: baja velocidad, suficiente para un LED
	GPIOH->PUPDR &= ~GPIO_PUPDR_PUPDR1; //00: sin pull-up ni pull-down, el pin es salida
	GPIOH->ODR &= ~GPIO_ODR_OD1; //El LED inicia apagado
}

/*
 * tim4_Init
 * Evento de actualización cada 1 ms: base de tiempo del sistema (ms_ticks)
 * PSC = 15 -> tick de 1 us, ARR = 999 -> (999 + 1) x 1 us = 1 ms
 * ARR nunca puede ser 0: con ARR = 0 el contador queda bloqueado y no hay interrupciones
 */
static void tim4_Init(void)
{
	RCC->APB1ENR |= RCC_APB1ENR_TIM4EN; //Activa la señal de reloj del TIM4 (bus APB1)

	TIM4->PSC = 15; //16 MHz / (15 + 1) = 1 MHz -> cada tick dura 1 us
	TIM4->ARR = 999; //El contador cuenta de 0 a 999 -> 1000 us = 1 ms por interrupción (ARR = 0 bloquearía el contador)
	TIM4->CNT = 0; //Reinicia el contador en 0

	TIM4->EGR |= TIM_EGR_UG; //Fuerza un evento de actualización: el PSC solo se carga a su registro activo en un update
	TIM4->SR &= ~TIM_SR_UIF; //El UG anterior levanta UIF, se limpia para que no entre una ISR falsa al habilitar el NVIC

	TIM4->DIER |= TIM_DIER_UIE; //Habilita la interrupción por actualización (primer interruptor)
	__NVIC_EnableIRQ(TIM4_IRQn); //Habilita la interrupción del TIM4 en el NVIC (segundo interruptor)

	TIM4->CR1 |= TIM_CR1_CEN; //Arranca el contador, siempre de último con todo ya configurado
}

/*
 * tim1_pwm_Init
 * PA8 -> TIM1_CH1 (AF1), PWM modo 1, 500 Hz, duty 0 %
 * Clock TIM1: 16 MHz (APB2 sin división). PSC = 15 -> tick de 1 us
 * Periodo = (ARR + 1) x 1 us = 2000 us -> 500 Hz
 */
static void tim1_pwm_Init(void)
{
	/* GPIOA PA8 como función alterna AF1 */
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN; //Activa la señal de reloj del GPIOA (bus AHB1)

	GPIOA->MODER &= ~GPIO_MODER_MODER8; //Limpia los 2 bits de modo del pin 8
	GPIOA->MODER |= GPIO_MODER_MODER8_1; //10: función alterna, el pin lo controla un periférico (el TIM1)

	GPIOA->OTYPER &= ~GPIO_OTYPER_OT8; //0: push-pull, el timer fuerza alto y bajo

	GPIOA->OSPEEDR &= ~GPIO_OSPEEDER_OSPEEDR8; //Limpia los 2 bits de velocidad del pin 8
	GPIOA->OSPEEDR |= GPIO_OSPEEDER_OSPEEDR8_1; //10: alta velocidad, flancos más limpios hasta 2 kHz en el osciloscopio

	GPIOA->PUPDR &= ~GPIO_PUPDR_PUPDR8; //00: sin pull-up ni pull-down, el timer maneja el nivel

	GPIOA->AFR[1] &= ~GPIO_AFRH_AFSEL8; //Limpia los 4 bits de función alterna del pin 8 (los pines 8-15 están en AFR[1] o AFRH)
	GPIOA->AFR[1] |= GPIO_AFRH_AFSEL8_0; //0001: AF1, conecta PA8 con TIM1_CH1

	/* Base de tiempo TIM1 */
	RCC->APB2ENR |= RCC_APB2ENR_TIM1EN; //Activa la señal de reloj del TIM1 (bus APB2, los TIM2 a TIM5 están en APB1)

	TIM1->PSC = 15; //16 MHz / (15 + 1) = 1 MHz -> cada tick dura 1 us
	TIM1->ARR = arr_table[0]; //1999: el contador cuenta de 0 a 1999 -> 2000 us -> 500 Hz
	TIM1->CR1 &= ~TIM_CR1_DIR; //0: el contador cuenta hacia arriba
	TIM1->CR1 |= TIM_CR1_ARPE; //ARR con preload: el cambio de frecuencia se aplica al terminar el periodo, sin pulsos cortados

	/* Canal 1 en PWM modo 1 */
	TIM1->CCMR1 &= ~TIM_CCMR1_CC1S; //00: el canal 1 es una salida (no una captura)
	TIM1->CCMR1 &= ~TIM_CCMR1_OC1M; //Limpia los 3 bits del modo de salida
	TIM1->CCMR1 |= (TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2); //110: PWM modo 1, alto mientras CNT < CCR1
	TIM1->CCMR1 |= TIM_CCMR1_OC1PE; //CCR1 con preload: el duty nuevo entra solo al inicio de un periodo

	TIM1->CCR1 = 0; //Duty inicial 0 %: la salida inicia en bajo

	TIM1->CCER &= ~TIM_CCER_CC1P; //0: polaridad activa en alto
	TIM1->CCER |= TIM_CCER_CC1E; //Habilita la salida del canal 1 hacia el pin

	TIM1->BDTR |= TIM_BDTR_MOE; //TIM1 es un timer avanzado: sin MOE (Main Output Enable) las salidas quedan apagadas aunque todo lo demás esté bien

	TIM1->EGR |= TIM_EGR_UG; //Fuerza un update: copia PSC, ARR y CCR1 desde sus preload a los registros activos

	TIM1->CR1 |= TIM_CR1_CEN; //Arranca el contador, siempre de último
}

/*
 * tim2_encoder_Init
 * PA0 -> TIM2_CH1 (CLK), PA1 -> TIM2_CH2 (DT), ambos AF1 con Pull-Up
 * Modo encoder 3 (TI12): cuenta en los flancos de los dos canales
 * -> 4 cuentas por paso mecánico del KY040
 *
 * IC1F e IC2F: filtro digital de la entrada. Toma muestras a f_DTS/32 y solo
 * acepta un cambio si 8 muestras seguidas coinciden: 8 x 2 us = 16 us (f_DTS = 16 MHz)
 */
static void tim2_encoder_Init(void)
{
	/* GPIOA PA0 y PA1 como función alterna AF1 */
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN; //Activa la señal de reloj del GPIOA (bus AHB1)

	GPIOA->MODER &= ~(GPIO_MODER_MODER0 | GPIO_MODER_MODER1); //Limpia los bits de modo de PA0 y PA1
	GPIOA->MODER |= (GPIO_MODER_MODER0_1 | GPIO_MODER_MODER1_1); //10: función alterna, los pines los lee el TIM2

	GPIOA->PUPDR &= ~(GPIO_PUPDR_PUPDR0 | GPIO_PUPDR_PUPDR1); //Limpia los bits de pull de PA0 y PA1
	GPIOA->PUPDR |= (GPIO_PUPDR_PUPDR0_0 | GPIO_PUPDR_PUPDR1_0); //01: Pull-Up, el KY040 cierra a GND y sin pull-up la entrada flotaría entre pasos

	GPIOA->AFR[0] &= ~(GPIO_AFRL_AFSEL0 | GPIO_AFRL_AFSEL1); //Limpia los bits de función alterna de PA0 y PA1 (pines 0-7 están en AFR[0] o AFRL)
	GPIOA->AFR[0] |= (GPIO_AFRL_AFSEL0_0 | GPIO_AFRL_AFSEL1_0); //0001: AF1, conecta PA0 con TIM2_CH1 y PA1 con TIM2_CH2

	/* Base de tiempo TIM2 (32 bit) */
	RCC->APB1ENR |= RCC_APB1ENR_TIM2EN; //Activa la señal de reloj del TIM2 (bus APB1)

	TIM2->PSC = 0; //Sin división: se quiere contar cada flanco del encoder
	TIM2->ARR = 0xFFFFFFFF; //Rango completo de 32 bit: el contador no se desborda en uso normal
	TIM2->CNT = 0; //El encoder inicia en 0

	/* Canales 1 y 2 como entradas */
	TIM2->CCMR1 &= ~(TIM_CCMR1_CC1S | TIM_CCMR1_CC2S); //Limpia la selección de entrada de ambos canales
	TIM2->CCMR1 |= (TIM_CCMR1_CC1S_0 | TIM_CCMR1_CC2S_0); //01: CC1 lee TI1 y CC2 lee TI2 (conexión directa)

	TIM2->CCMR1 &= ~(TIM_CCMR1_IC1F | TIM_CCMR1_IC2F); //Limpia los 4 bits de filtro de cada canal
	TIM2->CCMR1 |= (TIM_CCMR1_IC1F | TIM_CCMR1_IC2F); //1111: filtro máximo, f_DTS/32 con N = 8 (rechaza glitches de menos de 16 us)

	TIM2->CCER &= ~(TIM_CCER_CC1P | TIM_CCER_CC1NP | TIM_CCER_CC2P | TIM_CCER_CC2NP); //Sin inversión de polaridad (si cuenta al revés se activa CC1P)
	TIM2->CCER |= (TIM_CCER_CC1E | TIM_CCER_CC2E); //Habilita la captura de los dos canales

	/* Modo encoder */
	TIM2->SMCR &= ~TIM_SMCR_SMS; //Limpia los 3 bits del modo esclavo
	TIM2->SMCR |= (TIM_SMCR_SMS_0 | TIM_SMCR_SMS_1); //011: encoder mode 3, cuenta con los flancos de TI1 y TI2 (4 cuentas por paso)

#if SW_USE_TIM2_CH3
	sw_tim2ch3_Config(); //El canal 3 se configura antes de arrancar el contador
#endif

	TIM2->CR1 |= TIM_CR1_CEN; //Arranca el contador, siempre de último
}

#if SW_USE_TIM2_CH3
/*
 * sw_tim2ch3_Config
 * Pulsador en PA2 -> TIM2_CH3 (AF1), captura en flanco de bajada con filtro IC3F
 * El canal 3 es independiente del par CH1/CH2 del encoder, no lo afecta.
 * Con CC3IE se genera una interrupción (CC3IF) en cada flanco aceptado.
 */
static void sw_tim2ch3_Config(void)
{
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN; //Activa la señal de reloj del GPIOA (bus AHB1)

	GPIOA->MODER &= ~GPIO_MODER_MODER2; //Limpia los bits de modo de PA2
	GPIOA->MODER |= GPIO_MODER_MODER2_1; //10: función alterna, el pin lo lee el TIM2

	GPIOA->PUPDR &= ~GPIO_PUPDR_PUPDR2; //Limpia los bits de pull de PA2
	GPIOA->PUPDR |= GPIO_PUPDR_PUPDR2_0; //01: Pull-Up, en reposo el pin lee 1

	GPIOA->AFR[0] &= ~GPIO_AFRL_AFSEL2; //Limpia los bits de función alterna de PA2
	GPIOA->AFR[0] |= GPIO_AFRL_AFSEL2_0; //0001: AF1, conecta PA2 con TIM2_CH3

	TIM2->CCMR2 &= ~TIM_CCMR2_CC3S; //Limpia la selección de entrada del canal 3
	TIM2->CCMR2 |= TIM_CCMR2_CC3S_0; //01: CC3 es una entrada que lee TI3

	TIM2->CCMR2 &= ~TIM_CCMR2_IC3F; //Limpia los 4 bits de filtro del canal 3
	TIM2->CCMR2 |= TIM_CCMR2_IC3F; //1111: el mismo filtro de los canales del encoder (16 us)

	TIM2->CCER &= ~TIM_CCER_CC3NP; //CC3NP = 0
	TIM2->CCER |= TIM_CCER_CC3P; //CC3P = 1 y CC3NP = 0: captura en flanco de bajada (cuando se presiona)
	TIM2->CCER |= TIM_CCER_CC3E; //Habilita la captura del canal 3

	TIM2->SR &= ~TIM_SR_CC3IF; //Limpia la bandera antes de habilitar la interrupción
	TIM2->DIER |= TIM_DIER_CC3IE; //Habilita la interrupción por captura del canal 3 (primer interruptor)
	__NVIC_EnableIRQ(TIM2_IRQn); //Habilita la interrupción del TIM2 en el NVIC (segundo interruptor)
}
#else
/*
 * exti0_Init
 * PB0 (SW del KY040) -> EXTI0, flanco de bajada. Es la misma secuencia del
 * botón PC13 (EXTI13), cambia el pin, la línea y el registro EXTICR.
 * PC13 ya tiene pull-up en la tarjeta; PB0 es un cable al KY040, por eso
 * se activa el Pull-Up interno.
 */
static void exti0_Init(void)
{
	/* GPIOB PB0 como entrada con Pull-Up */
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN; //Activa la señal de reloj del GPIOB (bus AHB1)

	GPIOB->MODER &= ~GPIO_MODER_MODER0; //00: entrada

	GPIOB->PUPDR &= ~GPIO_PUPDR_PUPDR0; //Limpia los bits de pull de PB0
	//GPIOB->PUPDR |= GPIO_PUPDR_PUPDR0_0; //01: Pull-Up, en reposo el pin lee 1 y al presionar baja a 0

	/* Conexión de PB0 a la línea EXTI0 */
	RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN; //Activa el reloj del SYSCFG (bus APB2), el multiplexor de EXTI vive ahí

	SYSCFG->EXTICR[0] &= ~SYSCFG_EXTICR1_EXTI0; //Limpia la selección de puerto de la línea 0
	SYSCFG->EXTICR[0] |= SYSCFG_EXTICR1_EXTI0_PB; //0001: la línea EXTI0 toma el pin 0 del puerto B

	EXTI->RTSR &= ~EXTI_RTSR_TR0; //Sin disparo por flanco de subida (al soltar)
	EXTI->FTSR |= EXTI_FTSR_TR0; //Disparo por flanco de bajada (el pulsador conecta PB0 a GND)

	EXTI->PR = EXTI_PR_PR0; //Limpia una posible bandera pendiente (se limpia escribiendo 1)
	EXTI->IMR |= EXTI_IMR_MR0; //Desenmascara la línea 0: deja que llegue la interrupción al NVIC

	__NVIC_EnableIRQ(EXTI0_IRQn); //Habilita la interrupción de la línea 0 en el NVIC
}
#endif

/* Funciones ISR (solo registran el evento y regresan, sin lógica de la FSM) */

/* Base de tiempo: cada 1 ms cuenta el milisegundo y cada 500 ms deja la bandera del blinky */
void TIM4_IRQHandler(void)
{
	if (TIM4->SR & TIM_SR_UIF) //Confirma que la interrupción fue por actualización
	{
		TIM4->SR &= ~TIM_SR_UIF; //Baja la bandera (escribiendo 0) para que la interrupción se pueda volver a generar
		ms_ticks++; //Un milisegundo más desde el arranque (el antirrebote mide tiempo con este contador)
		if ((ms_ticks % BLINK_PERIOD_MS) == 0) //Cada 500 ms exactos
		{
			blink_flag = 1; //Deja el evento: la FSM lo convierte en EV_BLINK_TICK
		}
	}
}

#if SW_USE_TIM2_CH3
/* Pulsador por captura del canal 3 del TIM2 */
void TIM2_IRQHandler(void)
{
	if (TIM2->SR & TIM_SR_CC3IF) //Confirma que la interrupción fue por captura del canal 3
	{
		TIM2->SR &= ~TIM_SR_CC3IF; //Baja la bandera (escribiendo 0) para que se pueda volver a generar
		button_flag = 1; //Deja el evento: la FSM lo convierte en EV_BUTTON_PRESS
	}
}
#else
/* Pulsador por EXTI0 */
void EXTI0_IRQHandler(void)
{
	if (EXTI->PR & EXTI_PR_PR0) //Confirma que la línea 0 generó la interrupción
	{
		EXTI->PR = EXTI_PR_PR0; //Baja la bandera escribiendo 1 (en EXTI->PR se limpia con 1, no con 0)
		button_flag = 1; //Deja el evento: la FSM lo convierte en EV_BUTTON_PRESS
	}
}
#endif
