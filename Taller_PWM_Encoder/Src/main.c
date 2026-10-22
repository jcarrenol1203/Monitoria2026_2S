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
#define CCR_INIT		((ARR_INIT + 1) / 2) //CCR1 inicial: la mitad del periodo -> 1000 us de 2000 us -> duty de 50 %
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
uint32_t arr_value = ARR_INIT; //ARR que tiene el TIM1 (periodo = ARR + 1 ticks de 1 us)
uint32_t ccr_value = CCR_INIT; //CCR1 que tiene el TIM1 (ancho del pulso en ticks de 1 us), inicia en la mitad del periodo: duty de 50 %

/* Definición de funciones */

//Drivers de la FSM (el CÓMO)
static Event_t get_next_event(void); //Lee (y limpia) las banderas de las ISR y la lectura del encoder, y las convierte en un evento
static uint8_t encoder_HasSteps(void); //Devuelve 1 si el TIM2 acumuló al menos un paso completo (no modifica nada)
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

/*
 * get_next_event
 * Convierte las banderas que dejan las ISR (y el avance del encoder) en UN evento.
 * Prioridad: pulsador > encoder > blinky. Cada fuente se evalúa con su propio if.
 * Las banderas que no se atienden en esta llamada quedan pendientes para la siguiente pasada.
 */
static Event_t get_next_event(void)
{
	Event_t ev = EV_NONE; //Si nada ocurrió, se devuelve EV_NONE

	__disable_irq(); //Sección crítica: leer y limpiar una bandera no puede ser interrumpido, o el evento podría perderse
	if (button_flag == 1) //La ISR del pulsador dejó un evento pendiente
	{
		button_flag = 0; //Se baja la bandera: ya se atendió el evento
		ev = EV_BUTTON_PRESS; //Evento más prioritario
	}

	if ((ev == EV_NONE) && encoder_HasSteps()) //El encoder no genera interrupción: el TIM2 cuenta solo y aquí se consulta si ya hay un paso completo
	{
		ev = EV_ENCODER_CHANGED; //Los pasos se toman en el [ENTER] de STATE_UPDATE_PWM
	}

	if ((ev == EV_NONE) && (blink_flag == 1)) //Sin otro evento: la ISR del TIM4 dejó el evento del blinky pendiente
	{
		blink_flag = 0; //Se baja la bandera: ya se atendió el evento
		ev = EV_BLINK_TICK; //Evento menos prioritario
	}
	__enable_irq(); //Fin de la sección crítica: las ISR pueden volver a ejecutarse

	return ev; //Devuelve el evento a la FSM
}

/*
 * encoder_HasSteps
 * Devuelve 1 si el TIM2 acumuló al menos un paso completo (4 cuentas) hacia cualquiera de los dos lados.
 * Solo consulta, no modifica el contador.
 */
static uint8_t encoder_HasSteps(void)
{
	int32_t raw_count = (int32_t) TIM2->CNT; //CNT es de 32 bit sin signo: al girar en CCW desde 0 pasa a 0xFFFFFFFF, que con signo es -1

	return (uint8_t) ((raw_count >= COUNTS_PER_STEP) || (raw_count <= -COUNTS_PER_STEP)); //1 si hay un paso o más, CW (positivo) o CCW (negativo)
}

/*
 * encoder_TakeSteps
 * Devuelve los pasos completos acumulados (positivo = CW, negativo = CCW) y los descuenta del TIM2.
 * Las cuentas que no completan un paso (-3 a 3) se quedan en CNT para sumarse al siguiente giro.
 */
static int32_t encoder_TakeSteps(void)
{
	int32_t raw_count = (int32_t) TIM2->CNT; //Cuentas acumuladas con signo
	int32_t steps = raw_count / COUNTS_PER_STEP; //La división entera trunca hacia 0: 7 cuentas = 1 paso, -7 cuentas = -1 paso

	TIM2->CNT = (uint32_t) (raw_count - (steps * COUNTS_PER_STEP)); //Se descuentan solo las cuentas que se usaron, queda el residuo

	return steps; //Pasos con signo
}

/*
 * pwm_ApplySteps
 * MODE_CCR: CCR1 += steps x CCR_STEP, saturado entre 0 y ARR + 1.
 * MODE_ARR: ARR  += steps x ARR_STEP, saturado entre ARR_MIN y ARR_MAX.
 * Si el ARR baja por debajo del CCR1, el CCR1 se recorta a ARR + 1 (salida siempre en alto).
 * Con ARPE = 1 y OC1PE = 1 los valores nuevos entran al terminar el periodo actual.
 */
static void pwm_ApplySteps(int32_t steps)
{
	int32_t new_arr = (int32_t) arr_value; //Se calcula con signo para poder detectar que se pasó de los límites
	int32_t new_ccr = (int32_t) ccr_value; //Igual que el ARR

	if (edit_mode == MODE_CCR) //El encoder edita el ancho del pulso
	{
		new_ccr += steps * CCR_STEP; //Cada paso suma o resta CCR_STEP ticks
	}
	else //El encoder edita el periodo
	{
		new_arr += steps * ARR_STEP; //Cada paso suma o resta ARR_STEP ticks
	}

	if (new_arr < ARR_MIN) //Validación: el ARR nunca baja del mínimo
	{
		new_arr = ARR_MIN; //Se satura en el mínimo
	}
	else if (new_arr > ARR_MAX) //Validación: el ARR nunca pasa del máximo
	{
		new_arr = ARR_MAX; //Se satura en el máximo
	}

	if (new_ccr < 0) //El CCR1 nunca es negativo
	{
		new_ccr = 0; //Se satura en 0: salida siempre en bajo
	}
	else if (new_ccr > (new_arr + 1)) //El CCR1 no puede pasar de ARR + 1 (el contador cuenta de 0 a ARR)
	{
		new_ccr = new_arr + 1; //Se satura en ARR + 1: salida siempre en alto
	}

	arr_value = (uint32_t) new_arr; //Guarda los valores ya validados
	ccr_value = (uint32_t) new_ccr;

	TIM1->ARR = arr_value; //Escribe el periodo nuevo: (ARR + 1) x 1 us
	TIM1->CCR1 = ccr_value; //Escribe el ancho del pulso nuevo: CCR1 x 1 us
}

/*
 * mode_Toggle
 * Cambia el valor que edita el encoder y muestra el modo con la velocidad del blinky:
 * MODE_CCR -> 4 Hz (cambia cada 125 ms), MODE_ARR -> 2 Hz (cambia cada 250 ms).
 */
static void mode_Toggle(void)
{
	if (edit_mode == MODE_CCR) //Estaba editando el CCR
	{
		edit_mode = MODE_ARR; //Pasa a editar el ARR
		blink_half_period_ms = BLINK_ARR_MS; //Blinky a 2 Hz
	}
	else //Estaba editando el ARR
	{
		edit_mode = MODE_CCR; //Pasa a editar el CCR
		blink_half_period_ms = BLINK_CCR_MS; //Blinky a 4 Hz
	}
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
 * Evento de actualización cada 1 ms: base de tiempo del blinky
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
 * PA8 -> TIM1_CH1 (AF1), PWM modo 1, 500 Hz, CCR1 = 1000 (duty de 50 %)
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
	GPIOA->OSPEEDR |= GPIO_OSPEEDER_OSPEEDR8_1; //10: alta velocidad, flancos más limpios en el osciloscopio

	GPIOA->PUPDR &= ~GPIO_PUPDR_PUPDR8; //00: sin pull-up ni pull-down, el timer maneja el nivel

	GPIOA->AFR[1] &= ~GPIO_AFRH_AFSEL8; //Limpia los 4 bits de función alterna del pin 8 (los pines 8-15 están en AFR[1] o AFRH)
	GPIOA->AFR[1] |= GPIO_AFRH_AFSEL8_0; //0001: AF1, conecta PA8 con TIM1_CH1

	/* Base de tiempo TIM1 */
	RCC->APB2ENR |= RCC_APB2ENR_TIM1EN; //Activa la señal de reloj del TIM1 (bus APB2, los TIM2 a TIM5 están en APB1)

	TIM1->PSC = 15; //16 MHz / (15 + 1) = 1 MHz -> cada tick dura 1 us
	TIM1->ARR = ARR_INIT; //1999: el contador cuenta de 0 a 1999 -> 2000 us -> 500 Hz
	TIM1->CR1 &= ~TIM_CR1_DIR; //0: el contador cuenta hacia arriba
	TIM1->CR1 |= TIM_CR1_ARPE; //ARR con preload: el cambio de periodo se aplica al terminar el periodo, sin pulsos cortados

	/* Canal 1 en PWM modo 1 */
	TIM1->CCMR1 &= ~TIM_CCMR1_CC1S; //00: el canal 1 es una salida (no una captura)
	TIM1->CCMR1 &= ~TIM_CCMR1_OC1M; //Limpia los 3 bits del modo de salida
	TIM1->CCMR1 |= (TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2); //110: PWM modo 1, alto mientras CNT < CCR1
	TIM1->CCMR1 |= TIM_CCMR1_OC1PE; //CCR1 con preload: el ancho nuevo entra solo al inicio de un periodo

	TIM1->CCR1 = CCR_INIT; //Ancho inicial 1000 us: duty de 50 % al arrancar

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

	TIM2->CR1 |= TIM_CR1_CEN; //Arranca el contador, siempre de último
}

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

/* Funciones ISR (solo registran el evento y regresan, sin lógica de la FSM) */

/* Base de tiempo: cada 1 ms cuenta el milisegundo y cada blink_half_period_ms deja la bandera del blinky */
void TIM4_IRQHandler(void)
{
	static uint16_t blink_count = 0; //Milisegundos desde el último cambio del LED (static: conserva su valor entre interrupciones)

	if (TIM4->SR & TIM_SR_UIF) //Confirma que la interrupción fue por actualización
	{
		TIM4->SR &= ~TIM_SR_UIF; //Baja la bandera (escribiendo 0) para que la interrupción se pueda volver a generar
		blink_count++; //Un milisegundo más
		if (blink_count >= blink_half_period_ms) //Se cumplió el medio periodo: 125 ms (4 Hz) o 250 ms (2 Hz) según el modo
		{
			blink_count = 0; //Reinicia la cuenta para el próximo medio periodo
			blink_flag = 1; //Deja el evento: la FSM lo convierte en EV_BLINK_TICK
		}
	}
}

/* Pulsador por EXTI0 */
void EXTI0_IRQHandler(void)
{
	if (EXTI->PR & EXTI_PR_PR0) //Confirma que la línea 0 generó la interrupción
	{
		EXTI->PR = EXTI_PR_PR0; //Baja la bandera escribiendo 1 (en EXTI->PR se limpia con 1, no con 0)
		button_flag = 1; //Deja el evento: la FSM lo convierte en EV_BUTTON_PRESS
	}
}
