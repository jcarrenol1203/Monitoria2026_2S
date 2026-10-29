/**
 ******************************************************************************
 * @file           : mainME.c
 * @brief          : Encoder rotatorio (TIM2) cambia el CCR1 o el ARR de una señal PWM (TIM3),
 *                   dos fotocompuertas (EXTI) eligen cuál de los dos, organizado como una
 *                   Máquina de Estados Finitos (FSM)
 ******************************************************************************
 *
 *** Mapeo de Pines:
 ** PWM TIM3 - AF02:
 * - CH1 (PWM)  -> PA6
 *
 ** ENCODER TIM2 - AF01:
 * - CH1 (CLK) -> PA0
 * - CH2 (DT)  -> PA1
 *
 ** FOTOCOMPUERTAS (EXTI, flanco de bajada):
 * - PA12 -> EXTI12: el encoder pasa a modificar el CCR1 (ancho del pulso)
 * - PB8  -> EXTI8 : el encoder pasa a modificar el ARR  (periodo)
 *
 ** BLINKY TIM4 -> PH1 (cambia cada 250 ms, siempre)
 *
 *
 * ===================== FUNCIONAMIENTO ===================== *
 *
 * - TIM2 en modo encoder 3 cuenta los flancos de CLK y DT: cada paso físico del encoder
 *   son 4 cuentas en TIM2->CNT. Cada paso suma o resta una cantidad fija al valor que
 *   se esté editando.
 *
 * - TIM3 genera la señal PWM en PA6. Con PSC = 15 cada tick dura 1 us:
 *   periodo = (ARR + 1) us, ancho del pulso = CCR1 us. Arranca con ARR = 1999 y
 *   CCR1 = 0: 500 Hz y duty de 0 %.
 *
 * - Los dos modos de edición son EXCLUYENTES porque la variable edit_mode guarda uno solo:
 *   MODE_CCR (inicial): cada paso del encoder suma o resta 10 al CCR1 (ancho del pulso).
 *   MODE_ARR          : cada paso del encoder suma o resta 20 al ARR (periodo).
 *   Las fotocompuertas eligen el modo (PA12 -> CCR, PB8 -> ARR) pasando por STATE_EDIT_CCR o
 *   STATE_EDIT_ARR, que solo cambian edit_mode. STATE_UPDATE_PWM consulta edit_mode y modifica
 *   el CCR1 o el ARR. El ARR se satura entre 99 y 65534, el CCR1 entre 0 y ARR + 1.
 *
 * - TIM4 interrumpe cada 250 ms, sin importar el modo. El LED PH1 cambia en cada interrupción.
 *
 * - Reloj del sistema: HSI 16 MHz (sin PLL), los timers reciben 16 MHz.
 *
 *
 * ===================== FSM ===================== *
 *
 * Patrón Switch-Case. La FSM decide QUÉ pasa (política), las funciones driver hacen el
 * CÓMO (hardware). Las ISR solo levantan banderas volatile, get_next_event() las
 * convierte en un EVENTO y el while(1) nunca se bloquea.
 *
 * STATE_IDLE es el centro: es el único estado que lee eventos. Cada evento lo lleva a un
 * estado que hace UNA tarea y vuelve a IDLE.
 *
 *   Estado actual     Evento              Estado siguiente   Acción [ENTER]
 *   ----------------  ------------------  -----------------  -----------------------------------------
 *   STATE_IDLE        EV_PHOTO_CCR        STATE_EDIT_CCR     -
 *   STATE_IDLE        EV_PHOTO_ARR        STATE_EDIT_ARR     -
 *   STATE_IDLE        EV_ENCODER_CHANGED  STATE_UPDATE_PWM   -
 *   STATE_IDLE        EV_BLINK_TICK       STATE_BLINK        -
 *   STATE_EDIT_CCR    (incondicional)     STATE_IDLE         edit_mode = MODE_CCR
 *   STATE_EDIT_ARR    (incondicional)     STATE_IDLE         edit_mode = MODE_ARR
 *   STATE_UPDATE_PWM  (incondicional)     STATE_IDLE         ccr_ApplySteps o arr_ApplySteps(encoder_TakeSteps()),
 *                                                            según edit_mode
 *   STATE_BLINK       (incondicional)     STATE_IDLE         led_Toggle()
 *   STATE_ERROR       (cualquiera)        STATE_IDLE         recuperación
 *
 ******************************************************************************
 */

//Includes

#include "stm32f411xe.h"
#include <stdint.h>
#include <stm32f4xx.h>

//Estados de la FSM (el QUÉ)
typedef enum {
	STATE_IDLE, //Reposo: espera un evento sin hacer nada más (no bloquea)
	STATE_EDIT_CCR, //Elige el modo CCR (lo pide la fotocompuerta de PA12)
	STATE_EDIT_ARR, //Elige el modo ARR (lo pide la fotocompuerta de PB8)
	STATE_UPDATE_PWM, //Atiende el giro del encoder: suma o resta al CCR1 o al ARR según edit_mode
	STATE_BLINK, //Atiende el tick del TIM4: cambia el estado del LED PH1
	STATE_ERROR, //Estado inválido (trampa de hardware): se recupera hacia STATE_IDLE
	NUM_STATES //Centinela: cantidad de estados, sirve para validar el rango
} State_t;

//Modos de edición (qué cambia el encoder)
typedef enum {
	MODE_CCR, //El encoder cambia el CCR1 (ancho del pulso)
	MODE_ARR //El encoder cambia el ARR (periodo)
} Mode_t;

//Eventos de la FSM (Según que cambian los estados)
typedef enum {
	EV_NONE, //No ha ocurrido nada
	EV_ENCODER_CHANGED, //El encoder avanzó o retrocedió al menos un paso
	EV_PHOTO_CCR, //Fotocompuerta de PA12 (flanco de bajada): elegir editar el CCR
	EV_PHOTO_ARR, //Fotocompuerta de PB8 (flanco de bajada): elegir editar el ARR
	EV_BLINK_TICK //Venció el periodo del blinky (250 ms)
} Event_t;

//Variables
State_t current_state = STATE_IDLE; //Estado actual de la FSM, siempre inicia en reposo
Mode_t edit_mode = MODE_CCR; //Modo elegido (MODE_CCR o MODE_ARR): una sola variable, por eso son excluyentes
uint8_t state_entry_flag = 1; //Vale 1 al llegar a un estado para que su acción [ENTER] corra exactamente una vez

volatile uint8_t photo_ccr_flag = 0; //Bandera que deja la ISR de PA12 (volatile: cambia fuera del flujo normal)
volatile uint8_t photo_arr_flag = 0; //Bandera que deja la ISR de PB8
volatile uint8_t blink_flag = 0; //Bandera que deja la ISR del TIM4

int32_t arr_value = 1999; //ARR que tiene el TIM3 (periodo = ARR + 1 ticks de 1 us): (1999 + 1) us -> 500 Hz. Con signo para comparar con los límites
int32_t ccr_value = 0; //CCR1 que tiene el TIM3 (ancho del pulso en ticks de 1 us), inicia en 0

//Cabecera de funciones

//Drivers de la FSM (el CÓMO)
Event_t get_next_event(void); //Lee (y limpia) las banderas de las ISR y la lectura del encoder, y las convierte en un evento
int32_t encoder_TakeSteps(void); //Devuelve los pasos acumulados (con signo) y los descuenta del TIM2
void ccr_ApplySteps(int32_t steps); //Suma steps x 10 al CCR1, con saturación
void arr_ApplySteps(int32_t steps); //Suma steps x 20 al ARR, con saturación
void led_Toggle(void); //Cambia el estado del LED PH1

//Inicialización del hardware
void init_GPIO(void);
void init_EXTI(void);
void init_timer4(void);
void init_timer3PWM(void);
void init_tim2_encoder(void);

//Main

int main(void){
    Event_t current_event = EV_NONE; //Evento que se evalúa en cada pasada del ciclo infinito

    init_GPIO();
    init_EXTI();
    init_timer3PWM();
    init_tim2_encoder();
    init_timer4(); //Último: las interrupciones del blinky empiezan con todo el hardware listo

    /* El ciclo infinito nunca termina: lee un evento, evalúa el estado y reacciona.
     * Regla de NO bloqueo: no hay delay(), cada pasada procesa a lo sumo UN evento y UNA transición. */
    while(1){

        //Safeguard: valida el estado antes de usarlo, si está fuera de rango se recupera con STATE_ERROR
        if (current_state >= NUM_STATES) {
            current_state = STATE_ERROR;
            state_entry_flag = 1;
        }

        switch (current_state) {

        case STATE_IDLE: //Estado de reposo
            //[ENTER] Aquí no hay nada que preparar
            if (state_entry_flag) {
                state_entry_flag = 0;
            }

            //[DO] Solo IDLE consume eventos: los otros estados duran una pasada y no lo necesitan
            current_event = get_next_event();

            //Transiciones
            if (current_event == EV_PHOTO_CCR) {
                current_state = STATE_EDIT_CCR;
                state_entry_flag = 1;
            }
            else if (current_event == EV_PHOTO_ARR) {
                current_state = STATE_EDIT_ARR;
                state_entry_flag = 1;
            }
            else if (current_event == EV_ENCODER_CHANGED) {
                current_state = STATE_UPDATE_PWM;
                state_entry_flag = 1;
            }
            else if (current_event == EV_BLINK_TICK) {
                current_state = STATE_BLINK;
                state_entry_flag = 1;
            }
            break;

        case STATE_EDIT_CCR: //Elige el modo CCR
            //[ENTER] Se guarda el modo: el siguiente giro del encoder cambiará el CCR1
            if (state_entry_flag) {
                edit_mode = MODE_CCR;
                state_entry_flag = 0;
            }
            //Transición incondicional: la tarea ya fue atendida, se regresa al reposo
            current_state = STATE_IDLE;
            state_entry_flag = 1;
            break;

        case STATE_EDIT_ARR: //Elige el modo ARR
            //[ENTER] Se guarda el modo: el siguiente giro del encoder cambiará el ARR
            if (state_entry_flag) {
                edit_mode = MODE_ARR;
                state_entry_flag = 0;
            }
            current_state = STATE_IDLE;
            state_entry_flag = 1;
            break;

        case STATE_UPDATE_PWM: //Atiende el giro del encoder
            //[ENTER] Cambia el CCR1 o el ARR según el modo elegido, exactamente una vez por cada evento del encoder
            if (state_entry_flag) {
                if (edit_mode == MODE_CCR) {
                    ccr_ApplySteps(encoder_TakeSteps());
                }
                else {
                    arr_ApplySteps(encoder_TakeSteps());
                }
                state_entry_flag = 0;
            }
            current_state = STATE_IDLE;
            state_entry_flag = 1;
            break;

        case STATE_BLINK: //Estado que atiende el tick del blinky
            //[ENTER] Cambia el LED, exactamente una vez por cada evento del blinky
            if (state_entry_flag) {
                led_Toggle();
                state_entry_flag = 0;
            }
            current_state = STATE_IDLE;
            state_entry_flag = 1;
            break;

        case STATE_ERROR: //STATE_ERROR comparte código con default porque la recuperación es la misma
        default: //Trampa: cualquier estado desconocido se recupera de forma segura hacia el reposo
            current_state = STATE_IDLE;
            state_entry_flag = 1;
            break;
        }
    }
    return 0;
}

/* Funciones driver de la FSM (el CÓMO) */

//Convierte las banderas de las ISR y el encoder en UN evento. Prioridad: fotocompuertas > encoder > blinky
Event_t get_next_event(void){
    if (photo_ccr_flag == 1) { //La ISR de PA12 dejó un evento pendiente
        photo_ccr_flag = 0; //Se baja la bandera: el evento se atiende una sola vez
        return EV_PHOTO_CCR;
    }
    if (photo_arr_flag == 1) { //La ISR de PB8 dejó un evento pendiente
        photo_arr_flag = 0;
        return EV_PHOTO_ARR;
    }

    int32_t counts = (int32_t) TIM2->CNT; //Con signo: al girar en CCW desde 0, CNT pasa a 0xFFFFFFFF, que es -1
    if ((counts >= 4) || (counts <= -4)) { //4 cuentas = 1 paso mecánico (modo encoder 3). Hay al menos un paso completo, CW o CCW
        return EV_ENCODER_CHANGED;
    }

    if (blink_flag == 1) { //La ISR del TIM4 dejó un evento pendiente
        blink_flag = 0;
        return EV_BLINK_TICK;
    }

    return EV_NONE;
}

//Devuelve los pasos completos acumulados (+ CW, - CCW) y los descuenta del TIM2
int32_t encoder_TakeSteps(void){
    int32_t steps = (int32_t) TIM2->CNT / 4; //4 cuentas = 1 paso.
    TIM2->CNT = 0; //Se reinicia CNT
    return steps;
}

//Suma los pasos al CCR1 y lo limita entre 0 y ARR + 1
void ccr_ApplySteps(int32_t steps){
    ccr_value += steps * 10; //10 us por paso. Cambia el ancho del pulso

    if (ccr_value < 0) ccr_value = 0;
    if (ccr_value > arr_value + 1) ccr_value = arr_value + 1;

    TIM3->CCR1 = ccr_value; //Con OC1PE = 1 entra al terminar el periodo actual
}

//Suma los pasos al ARR y lo limita entre 99 y 65534, recortando el CCR1 si queda mayor que ARR + 1
void arr_ApplySteps(int32_t steps){
    arr_value += steps * 20; //20 us por paso. Cambia el periodo

    if (arr_value < 99) arr_value = 99; //(99 + 1) x 1 us = 100 us -> 10 kHz (con ARR = 0 el contador queda bloqueado)
    if (arr_value > 65534) arr_value = 65534; //TIM3 es de 16 bit y ARR + 1 (el CCR1 máximo) debe caber en 16 bit
    if (ccr_value > arr_value + 1) ccr_value = arr_value + 1; //Un periodo más corto puede dejar el CCR1 fuera de rango

    TIM3->ARR = arr_value; //Con ARPE = 1 y OC1PE = 1 entran al terminar el periodo actual
    TIM3->CCR1 = ccr_value;
}

//Cambia el estado del LED PH1
void led_Toggle(void){
    GPIOH->ODR ^= GPIO_ODR_OD1; //XOR con 1: invierte el bit sin tocar los otros pines
}

/* Funciones de inicialización del hardware */

void init_GPIO(void){

    /*
    PH1 (Blinky)
    */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOHEN;   //Se enciende la señal de reloj
    GPIOH->MODER &= ~GPIO_MODER_MODE1;   //Se limpian los bits
    GPIOH->MODER |= GPIO_MODER_MODE1_0; //Se configura en (01) salida de proposito general
    GPIOH->OTYPER &= ~GPIO_OTYPER_OT1; //Salida push-pull
    GPIOH->OSPEEDR &= ~GPIO_OSPEEDER_OSPEEDR1; //Velocidad baja
    GPIOH->PUPDR &= ~GPIO_PUPDR_PUPD1; //No pull up/down
    GPIOH->ODR |= GPIO_ODR_OD1; //Inicia encendido


    /*
    EXTIs (Entradas)
    */

    //PB8 (Fotocompuerta 1)
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN; //Señal de reloj puerto B
    GPIOB->MODER &= ~GPIO_MODER_MODE8; //Limpiamos (00) modo entrada
    GPIOB->PUPDR &= ~GPIO_PUPDR_PUPD8; //No pull up/down

    //PA12 (Fotocompuerta 2)
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN; //Señal de reloj puerto A
    GPIOA->MODER &= ~GPIO_MODER_MODE12; //Limpiamos (00) modo entrada
    GPIOA->PUPDR &= ~GPIO_PUPDR_PUPD12; //No pull up/down

    /*
    PWM (PA6 AF02 (TIM3 -> CH1))
    */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN; //Señal de reloj puerto A
    GPIOA->MODER &= ~GPIO_MODER_MODE6; //Limpiamos (00)
    GPIOA->MODER |= GPIO_MODER_MODE6_1; //(10) Función alternativa
    GPIOA->AFR[0] &= ~GPIO_AFRL_AFRL6; //Limpiamos
    GPIOA->AFR[0] |= GPIO_AFRL_AFRL6_1; //(0010 AF02)

    /*
    Encoder (CLK -> PA0 AF01 (TIM2->CH1); DT -> PA1 AF01 (TIM2->CH2))
    */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN; //Señal de reloj puerto A
    GPIOA->MODER &= ~(GPIO_MODER_MODE0 | GPIO_MODER_MODE1); //Limpiamos (00)
    GPIOA->MODER |= (GPIO_MODER_MODE0_1 | GPIO_MODER_MODE1_1); //(10) Función alternativa
    GPIOA->AFR[0] &= ~(GPIO_AFRL_AFRL0 | GPIO_AFRL_AFRL1); //Limpiamos
    GPIOA->AFR[0] |= (GPIO_AFRL_AFRL0_0 | GPIO_AFRL_AFRL1_0); //(0001 AF01 en ambos casos)
}

void init_EXTI(void){
    /*
    PB8 (Fotocompuerta 1)
    */
	RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN; //Reloj del SYSCFG, el multiplexor de EXTI vive ahí
	SYSCFG->EXTICR[2] |= SYSCFG_EXTICR3_EXTI8_PB; //La línea EXTI8 toma el pin 8 del puerto B
	EXTI->FTSR |= EXTI_FTSR_TR8; //Disparo por flanco de bajada
	EXTI->IMR |= EXTI_IMR_MR8; //Desenmascara la línea 8
	__NVIC_EnableIRQ(EXTI9_5_IRQn); //Habilita la interrupción en el NVIC

    /*
    PA12 (Fotocompuerta 2)
    */
	SYSCFG->EXTICR[3] |= SYSCFG_EXTICR4_EXTI12_PA; //La línea EXTI12 toma el pin 12 del puerto A
	EXTI->FTSR |= EXTI_FTSR_TR12; //Disparo por flanco de bajada
	EXTI->IMR |= EXTI_IMR_MR12; //Desenmascara la línea 12
	__NVIC_EnableIRQ(EXTI15_10_IRQn); //Habilita la interrupción en el NVIC
}

void init_timer4(void){
    RCC->APB1ENR |= RCC_APB1ENR_TIM4EN; //Reloj del TIM4

	TIM4->PSC = 16000-1; //16 MHz / 16000 = 1 kHz
	TIM4->ARR = 250-1; //250ms, fijo: el blinky no depende del modo
	TIM4->CR1 |= TIM_CR1_ARPE; //ARR con preload
	TIM4->EGR |= TIM_EGR_UG; //Fuerza una actualización para cargar PSC y ARR antes de arrancar
	TIM4->SR &= ~TIM_SR_UIF; //El UG levanta UIF: se baja para que no cuente como un tick falso
	TIM4->DIER |= TIM_DIER_UIE; //Habilita la interrupción por actualización (primer interruptor)
	__NVIC_EnableIRQ(TIM4_IRQn); //Habilita la interrupción en el NVIC (segundo interruptor)

	TIM4->CR1 |= TIM_CR1_CEN; //Arranca el contador, siempre de último
}

void init_timer3PWM(void){

    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN; //Reloj del TIM3

    TIM3->PSC = 16-1; //Tick de 1 us
	TIM3->ARR = arr_value; //Periodo: (1999 + 1) us
	TIM3->CCR1 = ccr_value; //Ancho del pulso
	TIM3->CR1 |= TIM_CR1_ARPE; //ARR con preload: el cambio de periodo no corta pulsos

    TIM3->CCMR1 |= (TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2); //110: PWM modo 1
    TIM3->CCMR1 |= TIM_CCMR1_OC1PE; //CCR1 con preload
    TIM3->CCER |= TIM_CCER_CC1E; //Habilita la salida del canal 1 al pin
    TIM3->EGR |= TIM_EGR_UG; //Fuerza una actualización para cargar PSC, ARR y CCR1 antes de arrancar

    TIM3->CR1 |= TIM_CR1_CEN; //Arranca el contador, siempre de último
}


void init_tim2_encoder(void){
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN; //Reloj del TIM2

    TIM2->ARR = 0xFFFFFFFF; //TIM2 es de 32 bit: no se desborda en uso normal
    TIM2->CCMR1 |= (TIM_CCMR1_CC1S_0 | TIM_CCMR1_CC2S_0); //01: CC1 y CC2 mapeados a TI1 y TI2
    TIM2->CCMR1 |= (TIM_CCMR1_IC1F | TIM_CCMR1_IC2F); //Filtro máximo: rechaza el rebote del encoder
    TIM2->CCER |= (TIM_CCER_CC1E | TIM_CCER_CC2E); //Habilita la captura de los dos canales

    TIM2->SMCR |= (TIM_SMCR_SMS_0 | TIM_SMCR_SMS_1); //011: modo encoder 3, cuenta los flancos de TI1 y TI2
    TIM2->CR1 |= TIM_CR1_CEN; //Arranca el contador
}

/* Funciones ISR (solo registran el evento y regresan, sin lógica de la FSM) */

//Cada vez que vence el TIM4 (cada 250 ms) deja la bandera del blinky
void TIM4_IRQHandler(void)
{
	TIM4->SR &= ~TIM_SR_UIF; //Baja la bandera (escribiendo 0) para que la interrupción se pueda repetir
	blink_flag = 1; //La FSM lo convierte en EV_BLINK_TICK
}

//Fotocompuerta de PB8 (EXTI8 comparte vector con las líneas 5 a 9)
void EXTI9_5_IRQHandler(void)
{
    if(EXTI->PR & EXTI_PR_PR8) {
        EXTI->PR = EXTI_PR_PR8; //Se baja escribiendo 1 (en EXTI->PR se limpia con 1, no con 0)
        photo_arr_flag = 1; //La FSM lo convierte en EV_PHOTO_ARR
    }
}

//Fotocompuerta de PA12 (EXTI12 comparte vector con las líneas 10 a 15)
void EXTI15_10_IRQHandler(void){
    if(EXTI->PR & EXTI_PR_PR12) {
        EXTI->PR = EXTI_PR_PR12; //Se baja escribiendo 1
        photo_ccr_flag = 1; //La FSM lo convierte en EV_PHOTO_CCR
    }
}
