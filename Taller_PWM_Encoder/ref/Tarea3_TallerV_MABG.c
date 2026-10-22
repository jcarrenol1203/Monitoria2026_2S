/**
 ******************************************************************************
 * @file           : Tarea3_TallerV_MABG.c
 * @author         : Miguel A. Bedoya Gonzalez --> mibedoyag@unal.edu.co
 * @brief          : Tarea 3 - LED RGB con señales PWM dirigidas por Encoder, Pot. (ADC) y USART
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
 *
 * - CH1 (Green) -> PA8
 * - CH2 (Blue)  -> PA9
 * - CH3 (Red)   -> PA10
 *
 ** ENCODER TIM2 - AF01:
 * - CH1 (encoder: CLK) -> PA0
 * - CH2 (Encoder_ DT)  -> PA1
 *
 ** ADC TIM3 (TRGO):
 * - ADC -> PA6
 *
 ** BLINKY TIM4 -> PH1
 *
 ** USART2 - AF01:
 * - Tx -> PA2
 * - Rx -> PA3
 *
 ** MCO2 - AF00 -> PC9
 *
 ** Parámetros de Configuración para Terminal Serial:
 * - Baudrate : 19200
 * - Bits de Datos : 8 --\
 * - Paridad : No      -- |--> 8N1
 * - Bit de Parada : 1 --/
 *
 ** Caracteres de Recepción o Envío desde la Terminal Serial:
 * - '+' : Incrementa en 1% el Duty Cycle pwm_blue (CH2).
 * - '-' : Disminuye en 1% el Duty Cycle pwm_blue (CH2).
 * - '0' : Lleva al 0% el Duty Cycle pwm_blue (CH2).
 * - '5' : Lleva al 50% del Duty Cycle pwm_blue (CH2).
 *
 *
 * ===================== FUNCIONAMIENTO ===================== *
 *
 * El  siguiente programa implementa el control de un LED RGB utilizando tres periféricos
 * independientes del STM32F411RE:
 *
 *  - Un encoder rotativo, configurado mediante el TIM2 en modo Encoder, para
 *    controlar el duty cycle  del PWM del canal 3 -> pwm_red.
 *
 *  - Un potenciómetro, leído mediante el ADC1 disparado periódicamente por el
 *    evento TRGO generado por el TIM3, para controlar el duty cycle del PWM del
 *    canal 1 -> pwm_green.
 *
 *  - Una comunicación serial UART2, mediante interrupciones de recepción,
 *    para modificar el duty cycle del PWM del canal 2 -> pwm_blue a partir de comandos
 *    enviados desde un terminal serial, los cuales se especificaron más arriba debajo
 *    del mapeo de pines.
 *
 * Las tres señales PWM son generadas por el TIM1 utilizando sus canales
 * CH1, CH2 y CH3 con una frecuencia de 2 kHz y que inician todos en 0.
 * Adicionalmente, el TIM4 genera una interrupción periódica utilizada
 * únicamente para el blinky.
 *
 * Durante la inicialización se configuran todos los periféricos, se habilitan
 * las interrupciones correspondientes y se envía un mensaje inicial por UART
 * con las instrucciones de operación para la recepción Rx de comandos.
 * Posteriormente, el programa permanece ejecutándose dentro del while, donde
 * procesa los eventos generados por cada periférico, actualiza las variables
 * asociadas, modifica los duty cycle de las señales PWM y transmite por UART
 * el estado actual del sistema únicamente cuando se detecta un cambio significativo
 * en alguno de los dispositivos de entrada (Encoder, ADC o Rx del UART).
 *
 ******************************************************************************
 */

#include "stm32f4xx_hal.h"
#include "stdio.h"
#include "string.h"
#include "stdlib.h"

/* TIM1 handle — must be global so stm32f4xx_it.c can access it */
TIM_HandleTypeDef htim1;

/* TIM2 handle — must be global so stm32f4xx_it.c can access it */
TIM_HandleTypeDef htim2;

/* TIM3 handle — must be global so stm32f4xx_it.c can access it */
TIM_HandleTypeDef htim3;

/* TIM4 handle — must be global so stm32f4xx_it.c can access it */
TIM_HandleTypeDef htim4;

/* ADC1 handle — must be global so stm32f4xx_it.c can access it */
ADC_HandleTypeDef hadc1 = {0};

/* USART2 handle — must be global so stm32f4xx_it.c can access it */
UART_HandleTypeDef huart2 = {0};

uint8_t msg_buffer[256] = {0}; //Arerglo donde estará el mensaje dinámico a transmitir USART2 Tx - max 256 caracteres
uint8_t startupMsg[] =   //Mensaje inicial, solo se envía una vez al iniciar el main ()
"\r\n"
"========================================\r\n"
" Tarea 3 - Taller V\r\n"
"========================================\r\n"
"Comandos UART:\r\n"
"  + : Incrementa 1 click\r\n"
"  - : Decrementa 1 click\r\n"
"  0 : Reinicia el contador a 0 clicks \r\n"
"  5 : Incrementa 50 clicks\r\n"
"========================================\r\n\r\n"; //Mensaje Inicial del UART Tx


volatile uint8_t adc_chaged = 0; //Variable volatil que cambia cuando hay un cambio en el valor de ADC para pasar a hacer Tx
volatile uint16_t raw_adc = 0; //Variable volatil donde se almacena el valor de la conversión ADC
volatile uint8_t adc_done = 0; //Variable volatil que se modifica y lleva a la ejecución del Callback del ADC
float adc_value_mv = 0.0f; //Variable donde se guarda y convierte el valor en mV a partir del raw_adc ADC
uint16_t adc_old = 0; //Varibale con la que se compara el valor del ADC para así saber si se debe transmitir Tx dado el cambio

volatile uint8_t encoder_changed = 0; //Variable volatil que cambia cuando hay un cambio en el valor del encoder para pasar a hacer Tx
volatile int16_t encoder_steps = 0; //Variable dedicada a almacenar los pasos dados y actuales del encoder -> CW ++ y CCW --
volatile uint8_t encoder_dir = 0; //Variable que indicará la dirección del encoder (0 = CW, 1 = CCW)
char *dir_string = 0; //Variable tipo string (caracter) que indica CW si es 0 y CCW si es 1 para enviar en Tx USART2
uint16_t encoder_old = 0; //Varibale con la que se compara el valor del encoder para así saber si se debe transmitir Tx dado el cambio

uint8_t usart_clicks = 0; //Variable dondé estará el número actual de click de acuerdo al comando que reciba de la consola (Rx)
uint8_t rx_data = 0; //Variable donde se almacena el caracter recibido de la consola para su interpretación
volatile uint8_t usart_done = 0; //Variable volatil que se modifica y lleva a la ejecución de la lógica del callback por fuera
volatile uint8_t uart_changed = 0; //Variable volatil que cambia cuando hay una recepción Rx que modifique el PWM para pasar a hacer Tx

volatile uint16_t pwm_red = 0; //Variable que tendra el duty del PWM CH1 (PA10)
volatile uint16_t pwm_green = 0; //Variable que tendra el duty del PWM CH2 (PA8)
volatile uint16_t pwm_blue = 0; //Variable que tendra el duty del PWM CH3 (PA9)

//FSM ejecución while
typedef enum
{
	STATE_READ_ENCODER = 0,
	STATE_PROCESS_ADC,
	STATE_PROCESS_USART,
	STATE_SEND_MESSAGE
} AppState_t;

AppState_t currentState = STATE_READ_ENCODER; //Al iniciar, siempre estará iniciandose la lectura del encoder para actualizarse

/* Private function prototypes */
static void SystemClock_Config(void);
static void gpio_Init(void); //Función que inicializa y configura el puerto PH1 del blinky
static void tim1_pwm_Init(void); //Función asociada a la inicialización y configuración del TIM1 (PWM -> CH1, CH2, CH3)
static void tim2_encoder_Init(void); //Función asociada a la inicialización y configuración del TIM2 (Encoder -> CH1 (PA0), CH2 (PA1))
static void tim4_Init(void); //Función asociada a la inicialización y configuración del TIM4(Blinky)
static void tim3_adc_Init(void); //Función asociada a la inicialización y configuración del TIM3 (Disparo del ADC con TRGO (PA6))
static void usart2_Init(void); //Función asociada a la inicialización y configuración del USART2 (Rx, Tx)
static void adc_Init(void); //Función asociada a la inicialización y configuración del ADC (PA6 ADC1_CH6)
static void mco2_Init(void); //Función asociada a la configuración del PC9 para el MCO2 con lectura del clock 16 MHz

int main(void)
{
    HAL_Init();           /* initialize HAL: SysTick, cache, priority grouping */
    SystemClock_Config(); /* configure clock tree: HSI at 16 MHz               */
    gpio_Init();          /* configure PH1 as push-pull output                  */
    tim4_Init();          /* configure TIM4: update event every 250 ms          */

    usart2_Init();
    adc_Init();
    tim1_pwm_Init();
    tim2_encoder_Init();
    tim3_adc_Init();
    mco2_Init ();


    HAL_UART_Transmit(&huart2, (uint8_t *)startupMsg,strlen((char *)startupMsg), 500);  //Mensaje Inicial de Funcionamiento de la Recepción Rx via Tx

    while (1)
    {
    	switch(currentState){

    	case STATE_READ_ENCODER:
    		encoder_steps = __HAL_TIM_GET_COUNTER(&htim2) / 4; //Leyendo el valor del contador del TIM2 que maneja el encoder, se divide entre 4 ya que el modo TI12 cuenta todos los flancos (4 por paso de ambos canales 2 por canal (asc y desc)
    		pwm_red = encoder_steps * 5; //Ya que el encoder va de 0 a 100 y el periodo de la señal PWM es del 500 us, se hace la conversión para que el duty vaya de 0 a 500 aproximadamente

    		if (encoder_steps != encoder_old){
    			encoder_old = encoder_steps; //Se igualan para que encoder_old vaya tenieno el valor actual del encoder
    			encoder_changed = 1; //Si hay un cambio en el valor del encoder cambia la bandera para actulizar Tx
    		}

    		if (pwm_red > 499) {
				pwm_red = 499;
			}
    		__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, pwm_red); //Asignando el nuevo CCR que tiene pwm_red al Canal 3

    		if (__HAL_TIM_IS_TIM_COUNTING_DOWN(&htim2)) { //Leyendo la dirección y asignando el valor en encoder_dir y dir_string para enviar por Tx USART2
				encoder_dir = 1;
				dir_string = "CCW";
			} else {
				encoder_dir = 0;
				dir_string = "CW";
			}

    		currentState = STATE_PROCESS_ADC;

    		break;

    	case STATE_PROCESS_ADC:
			if (adc_done == 1) {
				adc_value_mv = (float) ((3300.0f / 4095.0f) * raw_adc); //Transformando el valor raw adc en un valor de mV

				if (raw_adc < 15){  //Zona muerta para evitar leve parpadeo en el minimo del potenciometro (No llega a 0 Ohm)
					pwm_green = 0;
				}
				else{
					pwm_green = (raw_adc * 499) / 4095; //Realizan la conversión del raw_adc en duty para el pwm_green
				}

				__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, pwm_green); //Asignando el valor del duty (CCR) del pwm_green al PWM del Canal 1.

				if (abs(raw_adc - adc_old) > 50){  //Se compara la diferencia para evitar salto por ruido que disparen la transmision
					adc_old = raw_adc;  //Se igualan ambos valores para que adc_old se pueda seguir comparando
					adc_chaged = 1; //Si hay un cambio significativo en el valor del adc, cambia la bandera para actualizar Tx
				}

				adc_done = 0; //Limpiando la bandera para que vuelva a entrar
			}

			currentState = STATE_PROCESS_USART;

			break;

    	case STATE_PROCESS_USART:
			if (usart_done == 1) {
				if (rx_data == '+') { //Identificando el carater "+" para que incremente usart_clicks en 1
					if (usart_clicks < 100) {
						usart_clicks++;
						uart_changed = 1; //Si hay un cambio en el valor del clicks recibidos por Rx, cambia la bandera para actualizar Tx

					}
				}

				if (rx_data == '-') { //Identificando el carater "-" para que disminuya usart_clicks en 1
					if (usart_clicks > 0) {
						usart_clicks--;
						uart_changed = 1; //Si hay un cambio en el valor del clicks recibidos por Rx, cambia la bandera para actualizar Tx

					}
				}

				if (rx_data == '0') { //Identificando el carater "0" para que lleve la variable usart_clicks a 0
					usart_clicks = 0;
					uart_changed = 1; //Si hay un cambio en el valor del clicks recibidos por Rx, cambia la bandera para actualizar Tx

				}

				if (rx_data == '5') { //Identificando el carater "5" para que incremente usart_clicks en 50
					usart_clicks += 50;
					uart_changed = 1; //Si hay un cambio en el valor del clicks recibidos por Rx, cambia la bandera para actualizar Tx


					if (usart_clicks >= 100) {
						usart_clicks = 100;
					}
				}

				pwm_blue = usart_clicks * 5; //Ya que usart_clicks va de 0 a 100 y el periodo de la señal PWM es del 500 us, se hace la conversión para que el duty vaya de 0 a 500 aproximadamente
				__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, pwm_blue); //Asignando el valor de PWM del canal 2 que está en pwm_blue (CCR)
				HAL_UART_Receive_IT(&huart2, &rx_data, 1); //Se vuelve a activar la recepcion de datos ya que cuando recibe algo esta se desactiva y toca volverla a ejercutarla para que espere un nuevo comando

				usart_done = 0; //Limpiando la bandera para que vuelva a entrar
			}

			currentState = STATE_SEND_MESSAGE;

			break;

    	case STATE_SEND_MESSAGE:
			if (encoder_changed || adc_chaged || uart_changed) {  //Si hay algún cambio en alguna de estas variables, se ejecuta el if
				sprintf((char*) msg_buffer,
						"ADC value = %u raw\n\r" "ADC value = %.0f mV\n\r" "Encoder dir: %s, value = %d\n\r" "UART value = %u clicks\n\r\n\r\n\r",
						raw_adc, adc_value_mv, dir_string, encoder_steps,
						usart_clicks);
				HAL_UART_Transmit(&huart2, msg_buffer,
						strlen((char*) msg_buffer) - 1, 100);
				encoder_changed = 0;  //Vuelve la bandera a 0 para esperar si se produce otro cambio para transmitir
				adc_chaged = 0;  //Vuelve la bandera a 0 para esperar si se produce otro cambio para transmitir
				uart_changed = 0;  //Vuelve la bandera a 0 para esperar si se produce otro cambio para transmitir
			}

			currentState = STATE_READ_ENCODER; //Reinia la FSM para que se mantenda en bucle ejecutandose un caso tras el otro

			break;
    	}


    }
}

/*
 * SystemClock_Config
 * Uses HSI internal oscillator at 16 MHz
 * No PLL — simplest possible clock configuration
 */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /* HSI is already on at reset — confirm and use it */
    RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState            = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_NONE; //Desactiva el PLL, si se quiere llevar el MCU a la máxima freq. debe activarse (RCC_PLL_ON;) y luego se debe configurar los divisores y el multiplicador para llegar a la freq. final

    /* Si se quiere configurar el PLL, primero se debe elegir la fuente que tomará el PLL para la conversión, este caso
     * sería HSI, a continuación se configura el primer divisor (PLLM) (Ejm para 100 MHz: PLLM = 16 -> 16MHz / 16 = 1MHz),
     * luego de este iría el multiplicador (PLLN) (PLLN = 400 -> 1MHz * 400 = 400 MHz), se sigue entonces con el siguiente
     * y último divisor para el PLLCLK (PLLP) (RCC_PLLP_DIV4 = 4 -> 400MHz / 4 = 100 MHz y este será el PLLCLK. Hay un
     * divisor más (PLLQ) que no afecta el CPU como tal si no para el USB o el SDIO por ejemplo (PLLQ = 7 ->
     * 400 MHz / 7 = 57 MHz)*/

    HAL_RCC_OscConfig(&RCC_OscInitStruct); //Cargano la configuración en los registros FSR del MCU

    /* Select HSI as SYSCLK — all bus dividers set to 1 */
    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_SYSCLK | //Todos estos relojes tendrán esta configuracion, reloj principal SYSCLK
                                       RCC_CLOCKTYPE_HCLK   | //reloj AHB HCLK
                                       RCC_CLOCKTYPE_PCLK1  | //reloj APB1 PCLK1
                                       RCC_CLOCKTYPE_PCLK2;   //reloj APB2 PCLK2
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_HSI;  // EL reloj principal del sistema (SYSCLK) utilizará como fuente el HSI (Internal), PLLCLK si se usará PLL
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;   /* HCLK  = 16 MHz */ //Si se utiliza PLL se debe tener en cuenta está división de acuerdo a lo máximo que soporta el BUS
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;     /* APB1  = 16 MHz */ //Si se utiliza PLL se debe tener en cuenta está división de acuerdo a lo máximo que soporta el BUS
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;     /* APB2  = 16 MHz */ //Si se utiliza PLL se debe tener en cuenta está división de acuerdo a lo máximo que soporta el BUS

    /* FLASH_LATENCY_0 = zero wait states, correct for 16 MHz */ //Para  100 MHz, por ejemplo, se debe agregar ciclos de espera ya que la Flash no puede ir tan rapido como el CLK (3 ciclos -> Tabla 5 Manual de Referencia, CAP 3)
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0);
}

/*
 * gpio_Init
 * Configures PH1 as push-pull output — onboard LED on Nucleo board (PH1 tactic board)
 */
static void gpio_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* Enable GPIOH clock on AHB1 bus
       Same as bare-metal: RCC->AHB1ENR |= RCC_AHB1ENR_GPIOHEN */
    __HAL_RCC_GPIOH_CLK_ENABLE();

    /* Configure PH1 */
    GPIO_InitStruct.Pin   = GPIO_PIN_1;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(GPIOH, &GPIO_InitStruct); //Cargamos la configuracion en los registros FSR del MCU
    __NOP();
}


/* Configuración del GPIO y TIM1 y los 3 Canales de las 3 señales PWM (f = 2kHz) */
static void tim1_pwm_Init(void){

	__HAL_RCC_GPIOA_CLK_ENABLE(); //HAbilitando la señal de reloj del GPIOA (AHB1)

	/* Configuración inicial de los GPIO */
	//Configurando el GPIO PA8, PA9, PA10 para el CH1, CH2, CH3 respectivamente
	GPIO_InitTypeDef GPIO_Init_chtim1 = {0};

	GPIO_Init_chtim1.Pin = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10;
	GPIO_Init_chtim1.Mode = GPIO_MODE_AF_PP; //Modo funcion alternativa
	GPIO_Init_chtim1.Pull = GPIO_NOPULL; //No Pull Up ni Pull Down, los niveles altos y bajos serán controlados por el timer
	GPIO_Init_chtim1.Speed = GPIO_SPEED_FREQ_HIGH;
	GPIO_Init_chtim1.Alternate = GPIO_AF1_TIM1; //Funcion alternativa AF01 del TIM1

	//Cargando la configuración en los registros FSR del MCU
	HAL_GPIO_Init(GPIOA, &GPIO_Init_chtim1);

	/* Configuración del TIMER 1 Base */
	__HAL_RCC_TIM1_CLK_ENABLE(); //Habilitando la señal de reloj del TIM1 (APB2)

	htim1.Instance = TIM1;
	htim1.Init.Prescaler = 15; //Preescaler a 16 MHz / 16 =  1 MHz -> 1 us
	htim1.Init.CounterMode = TIM_COUNTERMODE_UP; //Contador de manera ascendente
	htim1.Init.Period = 499; //500 us de periodo, lo que da una frecuencia de 2 kHz para cada canal PWM del TIM1
	htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1; //NO se hace un división adicional del reloj
	htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

	//Cargando la configuración en los registros FSR del MCU
	HAL_TIM_PWM_Init(&htim1);

	//COnfiguración especifica de los Canales CH1, CH2 y CH3 del tim1
	TIM_OC_InitTypeDef ConfigOC_ch =  {0}; //Estructura Output Compare que configura como funciona cada canal

	ConfigOC_ch.OCMode = TIM_OCMODE_PWM1; //PWM Modo 1: Mientras CNT < CCR la salida está en alto, cuando CNT >= CCR la señal pasa a bajo (CCR (Capture/Compare Register) define el duty)
	ConfigOC_ch.Pulse = 0; //CCR inicia en 0 (Duty 0%) -> Led inicia apagado
	ConfigOC_ch.OCPolarity = TIM_OCPOLARITY_HIGH; //En alto el Led estará encendido, al contrario se invierte la logica de las salidas no complementarias (CH1, CH2 y CH3).
	ConfigOC_ch.OCFastMode = TIM_OCFAST_DISABLE; //Espera a terminar un ciclo para cambiar el duty y que no genere deformaciones en el pulso si se cambia al duty a mitad de un ciclo del pwm

	//Cargando la configuración en los registros FSR del MCU para cada Canal
	HAL_TIM_PWM_ConfigChannel (&htim1, &ConfigOC_ch, TIM_CHANNEL_1);
	HAL_TIM_PWM_ConfigChannel (&htim1, &ConfigOC_ch, TIM_CHANNEL_2);
	HAL_TIM_PWM_ConfigChannel (&htim1, &ConfigOC_ch, TIM_CHANNEL_3);

	//Iniciando o arrancando las señales en cada canal
	HAL_TIM_PWM_Start (&htim1, TIM_CHANNEL_1);
	HAL_TIM_PWM_Start (&htim1, TIM_CHANNEL_2);
	HAL_TIM_PWM_Start (&htim1, TIM_CHANNEL_3);


}

/* Configuración del TIM2 (32 bit) y los 2 Canales de las 2 entradas DT y CLK del encoder */
static void tim2_encoder_Init(void){
	__HAL_RCC_GPIOA_CLK_ENABLE(); //HAbilitando la señal de reloj del GPIOA (AHB1)

	/* Configuración inicial de los GPIO */
	//Configurando los pines del GPIO PA0 y  PA1 para el CH1 y CH2 respectivamente del TIM2
	GPIO_InitTypeDef GPIO_Init_chtim2 = { 0 };

	GPIO_Init_chtim2.Pin = GPIO_PIN_0 | GPIO_PIN_1; // Seleccion de GPIOA a usar
	GPIO_Init_chtim2.Mode = GPIO_MODE_AF_PP; //Indicando que funcionaran en modo ALternative Function
	GPIO_Init_chtim2.Pull = GPIO_PULLUP; //Asignando resistencia PullUp (nivel alto) para que los pines no queden sin definir o abiertos
	GPIO_Init_chtim2.Speed = GPIO_SPEED_FREQ_HIGH;
	GPIO_Init_chtim2.Alternate = GPIO_AF1_TIM2; //Funcion Alternatica AF01 del TIM2

	//Cargando la configuración en los registros FSR del MCU
	HAL_GPIO_Init(GPIOA, &GPIO_Init_chtim2);

	/* COnfiguración del TIMER 2 Base */
	__HAL_RCC_TIM2_CLK_ENABLE(); //Habilitando la señal de reloj del TIM2 (APB1)

	TIM_Encoder_InitTypeDef Config_encmode = {0};

	htim2.Instance = TIM2;
	htim2.Init.Prescaler = 0; //No dividirá los pulsos que reciba del encoder
	htim2.Init.CounterMode = TIM_COUNTERMODE_UP; //El modo encoder es quien decide hacia donde contar de acuerdo a lo que identifique (CW o CCW)
	htim2.Init.Period = 65535; //Contará hasta el maximo de la variable que es 16 bit (65535) y luego se desborda de nuevo a 0
	htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
	Config_encmode.EncoderMode = TIM_ENCODERMODE_TI12; // Modo encoder para usar ambos canales (CH1 y CH2) para determinar la dir y los steps (Por ello se registran 4 pasos por ciclo)


	//COnfiguración especifica de los Canales CH1
	Config_encmode.IC1Polarity = TIM_INPUTCHANNELPOLARITY_RISING; //Polaridad de entrada con flanco de subida para contar
	Config_encmode.IC1Selection = TIM_ICSELECTION_DIRECTTI; //Canal 1 leerá y estará conectado al Pin PA0 respectivo del TIM2 para CH1
	Config_encmode.IC1Prescaler = TIM_ICPSC_DIV1; //Se reliza captura cada que se detecta un flanco en la señal de entrada del CH1 sin division, todos los flancos
	Config_encmode.IC1Filter = 15; //Filtro donde deben pasar 15 (mask max) ciclos para aceptar el cambio de señal para evitar rebotes.

	//COnfiguración especifica de los Canales CH2
	Config_encmode.IC2Polarity = TIM_INPUTCHANNELPOLARITY_RISING; //Polaridad de entrada con flanco de subida para contar
	Config_encmode.IC2Selection = TIM_ICSELECTION_DIRECTTI; //Canal 2 leerá y estará conectado al Pin PA1 respectivo del TIM2 para CH2
	Config_encmode.IC2Prescaler = TIM_ICPSC_DIV1; //Se reliza captura cada que se detecta un flanco en la señal de entrada del CH2 sin division, todos los flancos
	Config_encmode.IC2Filter = 30; //Filtro donde deben pasar 30 ciclos para aceptar el cambio de señal para evitar rebotes.

	//Cargando la configuración en los registros FSR del MCU
	HAL_TIM_Encoder_Init(&htim2, &Config_encmode);

	//Iniciando o arrancando los contadores con las señales (CEN = 1)
	HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_ALL);


}
//USART2 init configuracion
//19200 8N1
static void usart2_Init(void){

	/* Enable GPIOA clock on AHB1 bus */
	 __HAL_RCC_GPIOA_CLK_ENABLE();

	 GPIO_InitTypeDef GPIO_Init_Tx = {0};

	 GPIO_Init_Tx.Pin = GPIO_PIN_2; //Pin PA2 (Tx)
	 GPIO_Init_Tx.Mode = GPIO_MODE_AF_PP;
	 GPIO_Init_Tx.Pull = GPIO_NOPULL;
	 GPIO_Init_Tx.Speed = GPIO_SPEED_FREQ_HIGH;
	 GPIO_Init_Tx.Alternate = GPIO_AF7_USART2;

	 HAL_GPIO_Init(GPIOA, &GPIO_Init_Tx);

	 GPIO_InitTypeDef GPIO_Init_Rx = {0};

	GPIO_Init_Rx.Pin = GPIO_PIN_3; //Pin PA3 (Rx)
	GPIO_Init_Rx.Mode = GPIO_MODE_AF_PP;
	GPIO_Init_Rx.Pull = GPIO_PULLUP; //COnfigurando resistencia PullUp para que luego de recibir, el pin quede un estado alto.
	GPIO_Init_Rx.Speed = GPIO_SPEED_FREQ_HIGH;
	GPIO_Init_Rx.Alternate = GPIO_AF7_USART2;


	 //Cargar la configuracion en los registros FSR del MCU
	 HAL_GPIO_Init(GPIOA, &GPIO_Init_Rx);

	 __NOP();


	/* Enable USART2 clock on APB1 bus */
	 __HAL_RCC_USART2_CLK_ENABLE();

	 huart2.Instance = USART2;
	 huart2.Init.BaudRate = 19200; //19200 bit / segundo, para 8N1 serían aprox 1920 bytes / s (Ya que es formato 8N1 son 1 Start, 8 Datos y 1 Stop (10 Bit)
	 huart2.Init.Mode = USART_MODE_TX_RX; //Modo transmisión (Tx) y recepcion (Rx)
	 huart2.Init.Parity = USART_PARITY_NONE;
	 huart2.Init.StopBits = USART_STOPBITS_1; //1 bit de parada
	 huart2.Init.WordLength = USART_WORDLENGTH_8B; //8N1

	 //Cargando la configuracion en los registros FSR del MCU
	 HAL_UART_Init(&huart2);
	 //Registrar la interrupcion en el NVIC para la recepcion Rx
	 HAL_NVIC_EnableIRQ(USART2_IRQn);

	 //Iniciando la Recepcion
	 HAL_UART_Receive_IT(&huart2, &rx_data, 1); //Guardará el dato en rx_data de a 1 byte

}


//Configurando el ADC Init
static void adc_Init(void){
		/* Enable GPIOA clock on AHB1 bus */
		 __HAL_RCC_GPIOA_CLK_ENABLE();

		 GPIO_InitTypeDef GPIO_Init_adc_ch6 = {0};

		 GPIO_Init_adc_ch6.Pin = GPIO_PIN_6;
		 GPIO_Init_adc_ch6.Mode = GPIO_MODE_ANALOG; //Modo de trabajo del pin Analogo
		 GPIO_Init_adc_ch6.Pull = GPIO_NOPULL; //No Pull UP ni Pull Down que alteren el nivel de voltaje que se están leyendo con el ADC


		 //Cargar la configuracion en los registros FSR del MCU
		 HAL_GPIO_Init(GPIOA, &GPIO_Init_adc_ch6);

		 __NOP();

		 /* Enable ADC clock on APB1 bus*/
		 __HAL_RCC_ADC1_CLK_ENABLE();

		 //Configurando la parte general del ADC

		 hadc1.Instance = ADC1;
		 hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2; //16 MHz / 2 = trabaja a 8 MHz
		 hadc1.Init.Resolution = ADC_RESOLUTION_12B; //Resolucion de 12 bit (4096 divisiones)
		 hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
		 hadc1.Init.ScanConvMode = DISABLE; //Desabilitado el modo escaneo ya que solo se usa un canal del ADC
		 hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV; //Se hace unicamente una conversión porque solo hay un Canal, hace la conversion y la entrega
		 hadc1.Init.ContinuousConvMode = DISABLE; //Unicamente se hará conversion por medio del TRGO
		 hadc1.Init.NbrOfConversion = 1; //Solo hay un canal por lo que solo se requiere una conversion
		 hadc1.Init.DiscontinuousConvMode = DISABLE;
		 hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T3_TRGO; //Utilizamos el TRGO del tim3 como trigger para la conversión
		 hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING; //Se lanza un ADC cada que detecte flanco de subida del tim3 con TRGO
		 hadc1.Init.DMAContinuousRequests = DISABLE;

		 //CArgar la configuracion en los registros FSR del MCU
		 HAL_ADC_Init(&hadc1);

		 //Configuracion y seleccion del canal especifico
		 ADC_ChannelConfTypeDef adc_ch6 = {0};

		 adc_ch6.Channel = ADC_CHANNEL_6;
		 adc_ch6.Rank = 1; //Orden de la secuencia, como solo hay 1 canal este tiene prioridad
		 adc_ch6.SamplingTime = ADC_SAMPLETIME_56CYCLES; //Ciclos que pasarán para que el condensador se cargue con la señal
		 adc_ch6.Offset = 0;

		 //Cargar la configuracion en los registros FSR del MCU
		 HAL_ADC_ConfigChannel(&hadc1, &adc_ch6);

		 //Registrar la interrupcion en el NVIC
		 HAL_NVIC_EnableIRQ(ADC_IRQn);

		 HAL_ADC_Start_IT(&hadc1); //Iiniciando el ADC1


}

/* Configuración del TIM3 para el TRGO del ADC1 */
static void tim3_adc_Init(void){

	__HAL_RCC_TIM3_CLK_ENABLE(); //Habilitando la señal de reloj para el TIM3 -> APB1

	htim3.Instance = TIM3;

	htim3.Init.Prescaler = 15999; //16 MHz / 16 kHz = 1 kHz (1 ms)
	htim3.Init.CounterMode = TIM_COUNTERMODE_UP; //Conteo en modo ascendente
	htim3.Init.Period = 29; //Periodo de 1 ms * 30 = 30 ms (30 conteos para que CNT  ARR se iguales y CNT vuelva a 0)
	htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1; //NO se hacen divisiones adicionales del reloj TIM3
	htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

	//Cargando la configuracion en los registros FSR del MCU
	HAL_TIM_Base_Init(&htim3);

	//Configurando el TRGO del TIM3 para interactuar con el ADC1
	TIM_MasterConfigTypeDef confTRGO = {0}; //Master genera eventos como el TRGO y slave responde a ellos (ADC1)

	confTRGO.MasterOutputTrigger = TIM_TRGO_UPDATE; //Cada que ocurra un evento de actualizacion, cada 30 ms
	confTRGO.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;

	//Cargando la configuracion en los registros FSR del MCU para la sincronización del TRGO generado por UPdate Event para hacer la conversion ADC
	HAL_TIMEx_MasterConfigSynchronization(&htim3, &confTRGO);

	//Iniciando el TIM4
	HAL_TIM_Base_Start(&htim3);

}
/*
 * tim4_Init
 * Configures TIM4 to generate an update event every 250 ms
 *
 * Clock chain:
 *   HSI (16 MHz) → APB1 (16 MHz) → TIM3 clock (16 MHz)
 *
 * PSC = 15999  →  tick = 16,000,000 / (15999 + 1) = 1,000 Hz  (1 ms per tick)
 * ARR = 249    →  period = (249 + 1) x 1 ms = 250 ms
 */
static void tim4_Init(void)
{
    /* Enable TIM4 clock on APB1 bus */
    __HAL_RCC_TIM4_CLK_ENABLE();

    /* Configure TIM4 base */
    htim4.Instance               = TIM4;
    htim4.Init.Prescaler         = 15999;
    htim4.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim4.Init.Period            = 249;
    htim4.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

    HAL_TIM_Base_Init(&htim4); //Cargando la configuracion en los registros FSR del MCU

    /* Enable TIM4 interrupt line in the NVIC */
    HAL_NVIC_EnableIRQ(TIM4_IRQn);

    /* Start TIM4 in interrupt mode — enables the update event interrupt */
    HAL_TIM_Base_Start_IT(&htim4);
    __NOP();

}


/* Configuración del MCO2 en PC9 para la lectura del Clock del MCU (16 MHz) */
static void mco2_Init(void){

	__HAL_RCC_GPIOC_CLK_ENABLE(); //Activando la señal de reloj para el GPIOC

	GPIO_InitTypeDef mco2_Init = {0};

	mco2_Init.Pin = GPIO_PIN_9;
	mco2_Init.Mode = GPIO_MODE_AF_PP; //Modo funcion alternativa
	mco2_Init.Pull = GPIO_NOPULL; //No pull up ni pull down ya que el pin será salida
	mco2_Init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	mco2_Init.Alternate = GPIO_AF0_MCO; //Funcion alternativa AF00 del MCO2

	//Cargando la configuración a los registros FSR del MCU
	HAL_GPIO_Init(GPIOC, &mco2_Init);

	//Iniciando el MCO
	HAL_RCC_MCOConfig(RCC_MCO2, RCC_MCO2SOURCE_SYSCLK, RCC_MCODIV_1); //1. Usar la salida del MCO2 (PC9), 2. El reloj que sacará por este pin será el principal del sistema(SYSCLK)

}



/*
 * HAL_TIM_PeriodElapsedCallback
 * Called automatically by HAL_TIM_IRQHandler() every time a timer
 * update event fires. Shared by all timers — always check htim->Instance.
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM4)
    {
        HAL_GPIO_TogglePin(GPIOH, GPIO_PIN_1);

    }
}

//Callback de la conversion ADC (Unicamente hace la conversión del raw al mV)
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc){
	if (hadc->Instance == ADC1){
		//Cargando  el dato de la conversion en una variable
		raw_adc = hadc->Instance->DR;
		adc_done = 1;
	}
}

//Callback de la ISR generada por el UART2 Rx
void HAL_UART_RxCpltCallback(UART_HandleTypeDef* huart){
	if (huart->Instance == USART2){

		usart_done = 1;
	}
}





