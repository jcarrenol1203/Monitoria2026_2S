/**
 ******************************************************************************
 * @file           : Tarea2_TallerV_MABG.c
 * @author         : Miguel A. Bedoya Gonzalez --> mibedoyag@unal.edu.co
 * @brief          : Tarea 2 - Siete segmentos y fotocompuertas
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
 * Mapeo de Pines:
 * - 7 SEGMENTOS:
 *
 * A -> PC9
 * B -> PB8
 * C -> PC11
 * D -> PC12
 * E -> PC10
 * F -> PC8
 * G -> PD2
 *
 * - TRANSISTORES (DIGITOS):
 * D1 (Digito 1) -> PC6
 * D2 (Digito 2) -> PB9
 * D3 (Digito 3) -> PC3
 * D4 (Digito 4) -> PB7
 *
 * - FOTOCOMPUERTAS:
 * F1 (Fotocompuerta 1 - Flanco Subida) -> PC2
 * F2 (Fotocompuerta 2 - Flanco Bajada) -> PA0
 *
 * - BLINKY -> PH1
 *
 ******************************************************************************
 */

#include <stdint.h>
#include <stm32f4xx.h>

/* Varbiables */

uint16_t counter_exti = 0; //Variable que aumenta o disminuye de acuerdo a EXTI que se active (EXTI0 o EXTI2)
volatile uint8_t increment_counter = 0; //Variable volatil que cambia activando la tarea de acuerdo a que ISR del EXTI se activa
volatile uint8_t digits[4] = {0, 0, 0, 0}; //Arreglo de 4 digitos volatil que se conforma por cada uno de los digitos del 7 segmentos
volatile uint8_t request_isrtim = 0; //Variable volatil que cambia cada que se activa la interrupción del TIM3 (6ms) que refresca o actualiza cada digito
volatile uint8_t currentdigit = 0; //Variable volatil que indica cual es el digito actual que se va a procesar (idicando que segmentos del digito encienden y cual transistor se activa)

/* Definición de funciones*/

void init_hardware_sevensegm(void); //Función que inicia el hardware del 7 segmentos (GPIO de segmentos, transistores y fotocompuertas)
void blinky(void); //Funcion encargada netamente del Blinky de estado
void init_EXTI(void); //Función encarga de iniciar y configurar EXTI0 y EXTI2
void update_digits(void); //Función encargada de verificar el número en que está el contador y descomponerlo en sus unidades
void init_refresh(void); //Función encargada de configurar TIM3 que es quien indica la interrupción que lleva el tiempo de activación de cada digito
void set_segments(void); //Función encargada de indicar, de acuerdo al numero de cada digito, qué segmentos debe activar
void mostrardigitos (void); //Función encargada de Encender un digito (activar transistor) por interrupción del TIM3

/* Main */

int main(void){
	blinky();
	init_hardware_sevensegm();
	init_EXTI();
	init_refresh();


	while(1){

		//Logica del los EXTI0 y EXTI2
		if (increment_counter == 1){
			counter_exti--; //Resta 1 unidad al contador
			increment_counter = 0; //Vuelve la variable volatil a 0 para que no se sobreescriba, indicando que ya fue atendida la ISR del TIM0
		} else if (increment_counter == 2){
			counter_exti++; //Suma 1 unidad al contador
			increment_counter = 0; //Vuelve la variable volatil a 0 para que no se sobreescriba, indicando que ya fue atendida la ISR del TIM2
		}

		//Logica del ISR TIM3
		if (request_isrtim == 1){
			request_isrtim = 0; //VUelve el valor a 0 para que no se sobreescriba y vuelva al ciclo, indicando que ya se atendio la ISR del TIM3
			mostrardigitos (); //Ejecuta la función que muestra cada digito cada 8 ms
		}
		update_digits(); //Ejecuta la función que revisa si hay nuevo digito y lo descompone en sus unidades actualizando el valor del display digito a digito

	}

	return 0;
}

/* Funciones */

void init_hardware_sevensegm(void){
	//Activando las señales de reloj/
	RCC->AHB1ENR &= ~RCC_AHB1ENR_GPIOAEN; //Limpiando el registro
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN; //Activando las señal de reloj para GPIOA
	RCC->AHB1ENR &= ~RCC_AHB1ENR_GPIOBEN; //Limpiando el registro
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN; //Activando las señal de reloj para GPIOB
	RCC->AHB1ENR &= ~RCC_AHB1ENR_GPIOCEN; //Limpiando el registro
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN; //Activando las señal de reloj para GPIOC
	RCC->AHB1ENR &= ~RCC_AHB1ENR_GPIODEN; //Limpiando el registro
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN; //Activando las señal de reloj para GPIOD
	RCC->AHB1ENR &= ~RCC_AHB1ENR_GPIOHEN; //Limpiando el registro
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOHEN; //Activando las señal de reloj para GPIOH

	/* Configuración de puertos GPIO para los LED del 7 segmentos */

	/*Configuracion del pin B8 -> Segmento B */
	//Configurción como salida
	GPIOB->MODER &= ~GPIO_MODER_MODE8; //Limpiando el registro
	GPIOB->MODER |= GPIO_MODER_MODE8_0;

	//Configuracion como salida push-pull/
	GPIOB->OTYPER &= ~(GPIO_OTYPER_OT8);

	//Configuracion de la velocidad como alta/
	GPIOB->OSPEEDR &= ~GPIO_OSPEEDR_OSPEED8; //Limpiando el registro
	GPIOB->OSPEEDR |= GPIO_OSPEEDR_OSPEED8_1;

	GPIOB->PUPDR &= ~GPIO_PUPDR_PUPD8; //No pull up, no pull down

	//Encendido del LED/
	GPIOB->ODR &= ~GPIO_ODR_OD8;

	/*Configuracion del pin C8 -> Segmento F */
	//Configurción como salida
	GPIOC->MODER &= ~GPIO_MODER_MODE8; //Limpiando el registro
	GPIOC->MODER |= GPIO_MODER_MODE8_0;

	//Configuracion como salida push-pull/
	GPIOC->OTYPER &= ~(GPIO_OTYPER_OT8);

	//Configuracion de la velocidad como alta/
	GPIOC->OSPEEDR &= ~GPIO_OSPEEDR_OSPEED8; //Limpiando el registro
	GPIOC->OSPEEDR |= GPIO_OSPEEDR_OSPEED8_1;

	GPIOC->PUPDR &= ~GPIO_PUPDR_PUPD8; //No pull up, no pull down

	//Encendido del LED/
	GPIOC->ODR &= ~GPIO_ODR_OD8;

	/*Configuracion del pin C9 -> Segmento A */
	//Configurción como salida
	GPIOC->MODER &= ~GPIO_MODER_MODE9; //Limpiando el registro
	GPIOC->MODER |= GPIO_MODER_MODE9_0;

	//Configuracion como salida push-pull/
	GPIOC->OTYPER &= ~(GPIO_OTYPER_OT9);

	//Configuracion de la velocidad como alta/
	GPIOC->OSPEEDR &= ~GPIO_OSPEEDR_OSPEED9; //Limpiando el registro
	GPIOC->OSPEEDR |= GPIO_OSPEEDR_OSPEED9_1;

	GPIOC->PUPDR &= ~GPIO_PUPDR_PUPD9; //No pull up, no pull down

	//Encendido del LED/
	GPIOC->ODR &= ~GPIO_ODR_OD9;

	/*Configuracion del pin C10 -> Segmento E */
	//Configurción como salida
	GPIOC->MODER &= ~GPIO_MODER_MODE10; //Limpiando el registro
	GPIOC->MODER |= GPIO_MODER_MODE10_0;

	//Configuracion como salida push-pull/
	GPIOC->OTYPER &= ~(GPIO_OTYPER_OT10);

	//Configuracion de la velocidad como alta/
	GPIOC->OSPEEDR &= ~GPIO_OSPEEDR_OSPEED10; //Limpiando el registro
	GPIOC->OSPEEDR |= GPIO_OSPEEDR_OSPEED10_1;

	GPIOC->PUPDR &= ~GPIO_PUPDR_PUPD10; //No pull up, no pull down

	//Encendido del LED/
	GPIOC->ODR &= ~GPIO_ODR_OD10;

	/*Configuracion del pin C11 -> Segmento C */
	//Configurción como salida
	GPIOC->MODER &= ~GPIO_MODER_MODE11; //Limpiando el registro
	GPIOC->MODER |= GPIO_MODER_MODE11_0;

	//Configuracion como salida push-pull/
	GPIOC->OTYPER &= ~(GPIO_OTYPER_OT11);

	//Configuracion de la velocidad como alta/
	GPIOC->OSPEEDR &= ~GPIO_OSPEEDR_OSPEED11; //Limpiando el registro
	GPIOC->OSPEEDR |= GPIO_OSPEEDR_OSPEED11_1;

	GPIOC->PUPDR &= ~GPIO_PUPDR_PUPD11; //No pull up, no pull down

	//Encendido del LED/
	GPIOC->ODR &= ~GPIO_ODR_OD11;

	/*Configuracion del pin C12 -> Segmento D */
	//Configurción como salida
	GPIOC->MODER &= ~GPIO_MODER_MODE12; //Limpiando el registro
	GPIOC->MODER |= GPIO_MODER_MODE12_0;

	//Configuracion como salida push-pull/
	GPIOC->OTYPER &= ~(GPIO_OTYPER_OT12);

	//Configuracion de la velocidad como alta/
	GPIOC->OSPEEDR &= ~GPIO_OSPEEDR_OSPEED12; //Limpiando el registro
	GPIOC->OSPEEDR |= GPIO_OSPEEDR_OSPEED12_1;

	GPIOC->PUPDR &= ~GPIO_PUPDR_PUPD12; //No pull up, no pull down

	//Encendido del LED/
	GPIOC->ODR &= ~GPIO_ODR_OD12;

	/*Configuracion del pin D2 -> Segmento G */
	//Configurción como salida
	GPIOD->MODER &= ~GPIO_MODER_MODE2; //Limpiando el registro
	GPIOD->MODER |= GPIO_MODER_MODE2_0;

	//Configuracion como salida push-pull/
	GPIOD->OTYPER &= ~(GPIO_OTYPER_OT2);

	//Configuracion de la velocidad como alta/
	GPIOD->OSPEEDR &= ~GPIO_OSPEEDR_OSPEED2; //Limpiando el registro
	GPIOD->OSPEEDR |= GPIO_OSPEEDR_OSPEED2_1;

	GPIOD->PUPDR &= ~GPIO_PUPDR_PUPD2; //No pull up, no pull down

	//Encendido del LED/
	GPIOD->ODR &= ~GPIO_ODR_OD2;


	/* Configuración de puertos GPIO para los Transistores */

	/*Configuracion del pin C6 -> Digito D1 */
	//Configurción como salida
	GPIOC->MODER &= ~GPIO_MODER_MODE6; //Limpiando el registro
	GPIOC->MODER |= GPIO_MODER_MODE6_0;

	//Configuracion como salida push-pull/
	GPIOC->OTYPER &= ~(GPIO_OTYPER_OT6);

	//Configuracion de la velocidad como alta/
	GPIOC->OSPEEDR &= ~GPIO_OSPEEDR_OSPEED6; //Limpiando el registro
	GPIOC->OSPEEDR |= GPIO_OSPEEDR_OSPEED6_1;

	GPIOC->PUPDR &= ~GPIO_PUPDR_PUPD6; //No pull up, no pull down

	//Activación/encendido del transistor
	GPIOC->ODR |= GPIO_ODR_OD6;

	/*Configuracion del pin B9 -> Digito D2 */
	//Configurción como salida
	GPIOB->MODER &= ~GPIO_MODER_MODE9; //Limpiando el registro
	GPIOB->MODER |= GPIO_MODER_MODE9_0;

	//Configuracion como salida push-pull/
	GPIOB->OTYPER &= ~(GPIO_OTYPER_OT9);

	//Configuracion de la velocidad como alta/
	GPIOB->OSPEEDR &= ~GPIO_OSPEEDR_OSPEED9; //Limpiando el registro
	GPIOB->OSPEEDR |= GPIO_OSPEEDR_OSPEED9_1;

	GPIOB->PUPDR &= ~GPIO_PUPDR_PUPD9; //No pull up, no pull down

	//Activación/encendido del transistor
	GPIOB->ODR |= GPIO_ODR_OD9;

	/*Configuracion del pin C3 -> Digito D3 */
	//Configurción como salida
	GPIOC->MODER &= ~GPIO_MODER_MODE3; //Limpiando el registro
	GPIOC->MODER |= GPIO_MODER_MODE3_0;

	//Configuracion como salida push-pull/
	GPIOC->OTYPER &= ~GPIO_OTYPER_OT3;

	//Configuracion de la velocidad como alta/
	GPIOC->OSPEEDR &= ~GPIO_OSPEEDR_OSPEED3; //Limpiando el registro
	GPIOC->OSPEEDR |= GPIO_OSPEEDR_OSPEED3_1;

	GPIOC->PUPDR &= ~GPIO_PUPDR_PUPD3; //No pull up, no pull down

	//Activación/encendido del transistor
	GPIOC->ODR |= GPIO_ODR_OD3;

	/*Configuracion del pin B7 -> Digito D4 */
	//Configurción como salida
	GPIOB->MODER &= ~GPIO_MODER_MODE7; //Limpiando el registro
	GPIOB->MODER |= GPIO_MODER_MODE7_0;

	//Configuracion como salida push-pull/
	GPIOB->OTYPER &= ~(GPIO_OTYPER_OT7);

	//Configuracion de la velocidad como alta/
	GPIOB->OSPEEDR &= ~GPIO_OSPEEDR_OSPEED7; //Limpiando el registro
	GPIOB->OSPEEDR |= GPIO_OSPEEDR_OSPEED7_1;

	GPIOB->PUPDR &= ~GPIO_PUPDR_PUPD7; //No pull up, no pull down

	//Activación/encendido del transistor
	GPIOB->ODR |= GPIO_ODR_OD7;


	/* Configuración de puertos GPIO para las Fotocompuertas */

	/*Configuracion del pin C2 -> Fotocompuerta F1 */
	//Configuracion como entrada simple
	GPIOC->MODER &= ~GPIO_MODER_MODE2;
	GPIOC->PUPDR &= ~GPIO_PUPDR_PUPD2; //No resistencia pull up, ni pull down

	/*Configuracion del pin A0 -> Fotocompuerta F2 */
	//Configuracion como entrada simple
	GPIOA->MODER &= ~GPIO_MODER_MODE0;
	GPIOA->PUPDR &= ~GPIO_PUPDR_PUPD0; //No resistencia pull up, ni pull down


}

void blinky(void){
	//Encendiendo la señal de reloj para el bus AHB1 donde esta el GPIOH
	RCC->AHB1ENR &= ~RCC_AHB1ENR_GPIOHEN; //Limpiando el registro del RCC en la posición del GPIOH
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOHEN;  //Encendiendo la señal de reloj para el GPIOH

	/* Configuracion del pin H1 */
	GPIOH->MODER &= ~GPIO_MODER_MODE1; //Limpiando el registro
	GPIOH->MODER |= GPIO_MODER_MODE1_0; //Asignando el pin H1 como salida

	//Configuracion del pin H1 como salida push-pull/
	GPIOH->OTYPER &= ~(GPIO_OTYPER_OT1);

	//Configuracion de la velocidad como alta/
	GPIOH->OSPEEDR &= ~GPIO_OSPEEDR_OSPEED1; //Limpiando el registro
	GPIOH->OSPEEDR |= GPIO_OSPEEDR_OSPEED1_1; //Asignando la velocidad de alta

	GPIOH->PUPDR &= ~GPIO_PUPDR_PUPD1; //No pull up, no pull down

	//Encendido del LED/
	GPIOH->ODR |= GPIO_ODR_OD1;


	/* Configuración del Timer 2 */
	RCC->APB1ENR &= ~RCC_APB1ENR_TIM2EN; //Limpieza de la posición del registro
	RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;  //Encendiendo la señal de reloj para TIM2

	//Configurando el Prescaler
	TIM2->PSC = 1600 - 1; // 0.1 ms O 10 kHz ya que el TIM2 es a 16 MHz / 1600 = 0.1 ms

	//Configurando el ARR
	TIM2->ARR = 2500 - 1; // 0.1 ms * 5000 = 250 ms para que genere un ciclo

	//Reinicio del contador
	TIM2->CNT = 0; //Inicializa en 0

	//Limpieza de la bandera de la interrupción
	TIM2->SR &= ~TIM_SR_UIF;

	//Asignando el tipo de interrupcion como update-event
	TIM2->DIER &= ~ TIM_DIER_UIE; //Limpiando la posición del registro
	TIM2->DIER |=  TIM_DIER_UIE;

	//Matriculando la interrupción generada por TIM2 en el NVIC
	__NVIC_EnableIRQ(TIM2_IRQn);

	//Configuracion del contador
	TIM2->CR1 &= ~TIM_CR1_DIR; //Indicando la dirección de conteo para el contador

	//Estableciendo la precarga del ARR
	TIM2->CR1 &= ~TIM_CR1_ARPE; //Limpiando la posición del registro antes de asignar
	TIM2->CR1 |= TIM_CR1_ARPE; //Activando la precarga

	//Activando el contador para que el reloj inicie la propagación
	TIM2->CR1 |= TIM_CR1_CEN;
}

/* Funcion ISR para el TIM2 (Startup) */
void TIM2_IRQHandler(void){
	//Confirmar que se ha generado una interrupcion
	if(TIM2->SR && TIM_SR_UIF){
		GPIOH->ODR ^= GPIO_ODR_OD1; //Hace el toggle del LED
		TIM2->SR &= ~TIM_SR_UIF; //Baja la bandera para que la interrupción se pueda volver a generar
	}
}

/* Funcion de condiguración para el EXTI */
void init_EXTI(void){
	//Encendiendo la señal de reloj para el bus APB2 donde esta el EXTI
	RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
	//Configurando el MUX del EXTI 0 -> Fotocompuerta F2 (PA0)(Flanco de bajada)
	SYSCFG->EXTICR[0] &= ~SYSCFG_EXTICR1_EXTI0; //Limpiamos la dirección del registro y se confiura el EXTI0 para que reconozca el PA0 (0000)

	//Configurando para detectar flanco de bajada
	EXTI->FTSR |= EXTI_FTSR_TR0; //Detectando flancos de bajada en la posición 0
	EXTI->RTSR &= ~EXTI_RTSR_TR0; //Confirmando que el de subida este desactivado


	//Configurando el MUX del EXTI 2 -> Fotocompuerta F1 (PC2) (Flanco de subida)
	SYSCFG->EXTICR[0] &= ~SYSCFG_EXTICR1_EXTI2; //Limpiamos la dirección del registro
	SYSCFG->EXTICR[0] |= SYSCFG_EXTICR1_EXTI2_PC; //EXTI2 reconociendo el puerto C (pin2)

	//Configurando para detectar flanco de subida
	EXTI->RTSR |= EXTI_RTSR_TR2; //Detectando flancos de subida en la posición 2
	EXTI->FTSR &= ~EXTI_FTSR_TR2; //Confirmando que el de bajada este desactivado

	//Matriculando ambas interrupciones en el NVIC
	__NVIC_EnableIRQ(EXTI0_IRQn);
	__NVIC_EnableIRQ(EXTI2_IRQn);

	//Limpiando la bandera de ambos EXTI (Se limpia con 1)
	EXTI->PR |= EXTI_PR_PR0; //Limpiando la bandera de EXTI0
	EXTI->PR |= EXTI_PR_PR2; //Limpiando la bandera de EXTI2

	//Activando, por último, ambas interrupciones (OR)
	EXTI->IMR |= EXTI_IMR_MR0; //Activando para el EXTI0
	EXTI->IMR |= EXTI_IMR_MR2; //Activando para el EXTI2
}

/* Funcion ISR para el EXTI0 */
void EXTI0_IRQHandler(void){
	if (EXTI->PR && EXTI_PR_PR0){
		//Bajamos la bandera del EXTI0 para que el ciclo continue
		EXTI->PR |= EXTI_PR_PR0;
		__NOP();
		increment_counter = 1; //Variable volatil (Descartada en el proceso de revisar la ISR)
	}
}

/* Funcion ISR para el EXTI2 */
void EXTI2_IRQHandler(void){
	if (EXTI->PR && EXTI_PR_PR2){
		//Bajamos la bandera del EXTI2 para que el ciclo continue
		EXTI->PR |= EXTI_PR_PR2;
		__NOP();
		increment_counter = 2; //Variable volatil (Descartada en el proceso de revisar la ISR)
	}
}

/* Función que separa el valor de contador en unidades, decenas, centenas, unidades de mil */
void update_digits(void){
	uint16_t value = counter_exti % 10000; //Operación modulo
	digits[0] = value / 1000; //Digito 1 del 7 segmentos a partir del contador
	digits[1] = (value % 1000) / 100; //Digito 2 del 7 segmentos a partir del contador
	digits[2] = (value % 100) / 10; //Digito 3 del 7 segmentos a partir del contador
	digits[3] = value % 10; //Digito 4 del 7 segmentos a partir del contador
}


/* Funcion encargada del TIM3 para la actulización del 7 segmentos */
void init_refresh(void){
	RCC->APB1ENR &= ~RCC_APB1ENR_TIM3EN; //Limpieza de la posición del registro
	RCC->APB1ENR |= RCC_APB1ENR_TIM3EN; //Encendiendo la señal de reloj para TIM3

	//Configurando el Prescaler
	TIM3->PSC = 1600 - 1; // 0.1 ms O 10 kHz ya que el TIM2 es a 16 MHz / 1600 = 0.1 ms

	//Configurando el ARR
	TIM3->ARR = 60 - 1; // 0.1 ms * 60 = 60 ms para que genere un ciclo

	//Reinicio del contador
	TIM3->CNT = 0; //Inicializa en 0

	//Limpieza de la bandera de la interrupción
	TIM3->SR &= ~TIM_SR_UIF;

	//Asignando el tipo de interrupcion como update-event
	TIM3->DIER &= ~ TIM_DIER_UIE; //Limpiando la posición del registro
	TIM3->DIER |= TIM_DIER_UIE;

	//Matriculando la interrupción generada por TIM3 en el NVIC
	__NVIC_EnableIRQ(TIM3_IRQn);

	//Configuracion del contador
	TIM3->CR1 &= ~TIM_CR1_DIR; //Indicando la dirección de conteo para el contador

	//Estableciendo la precarga del ARR
	TIM3->CR1 &= ~TIM_CR1_ARPE; //Limpiando la posición del registro antes de asignar
	TIM3->CR1 |= TIM_CR1_ARPE; //Activando la precarga

	//Activando el contador para que el reloj inicie la propagación
	TIM3->CR1 |= TIM_CR1_CEN;
}

/* Funcion ISR para el TIM3 (Startup) */
void TIM3_IRQHandler(void){
	//Confirmar que se ha generado una interrupcion
	if(TIM3->SR && TIM_SR_UIF){
		TIM3->SR &= ~TIM_SR_UIF; //Baja la bandera para que la interrupción se pueda volver a generar
		request_isrtim = 1; //Cambia el estado de la variable volatil para no sobrecargar la funcion
	}
}

/*Función que activa cada digito del 7 segmentos */
void mostrardigitos (void){
	//Apago todos lo digitos
	GPIOC->ODR |= GPIO_ODR_OD6; //Se apaga el digito D1
	GPIOB->ODR |= GPIO_ODR_OD9; //Se apaga el digito D2
	GPIOC->ODR |= GPIO_ODR_OD3; //Se apaga el digito D3
	GPIOB->ODR |= GPIO_ODR_OD7; //Se apaga el digito D4

	//Configura y muestras los segmentos a activar por digito
	set_segments();
	switch(currentdigit)
	    {
	        case 0:
	            GPIOC->ODR &= ~GPIO_ODR_OD6; //Enciendo el digito D1
	            break;

	        case 1:
	            GPIOB->ODR &= ~GPIO_ODR_OD9; //Enciiendo el digito D2
	            break;

	        case 2:
	            GPIOC->ODR &= ~GPIO_ODR_OD3; //Enciiendo el digito D3
	            break;

	        case 3:
	            GPIOB->ODR &= ~GPIO_ODR_OD7; //Enciiendo el digito D4
	            break;
	    }
	    currentdigit++; //Una vez muestra el digito del caso 0 (D1) pasa el siguiente digito

	    if(currentdigit > 3)
	    {
	        currentdigit = 0; //Si la variable currendigit se paso de 3 que es el digito maximo, re reinicia al D1 o case 0
	    }
	}

/* Funcion que asigna los segmentos a activar para cada digito */
void set_segments(void){
	uint8_t numero; ///Variable que tendra el valor de cada uno de los digitos para saber que segmentos activar
	    numero = digits[currentdigit];

	    switch(numero)
	    {
	        case 0: //Activa los segmentos para escribir un 0
	        	GPIOC->ODR &= ~GPIO_ODR_OD9; //Enciendo el segmento A
	        	GPIOB->ODR &= ~GPIO_ODR_OD8; //Enciendo el segmento B
	        	GPIOC->ODR &= ~GPIO_ODR_OD11; //Enciendo el segmento C
	        	GPIOC->ODR &= ~GPIO_ODR_OD12; //Enciendo el segmento D
	        	GPIOC->ODR &= ~GPIO_ODR_OD10; //Enciendo el segmento E
	        	GPIOC->ODR &= ~GPIO_ODR_OD8; //Enciendo el segmento F
	        	GPIOD->ODR |= GPIO_ODR_OD2; //Apaga el segmento G
	            break;

			case 1: //Activa los segmentos para escribir un 1
				GPIOC->ODR |= GPIO_ODR_OD9; //Apaga el segmento A
				GPIOB->ODR &= ~GPIO_ODR_OD8; //Enciendo el segmento B
				GPIOC->ODR &= ~GPIO_ODR_OD11; //Enciendo el segmento C
				GPIOC->ODR |= GPIO_ODR_OD12; //Apaga el segmento D
				GPIOC->ODR |= GPIO_ODR_OD10; //Apaga el segmento E
				GPIOC->ODR |= GPIO_ODR_OD8; //Apaga el segmento F
				GPIOD->ODR |= GPIO_ODR_OD2; //Apaga el segmento G
	            break;

			case 2: //Activa los segmentos para escribir un 2
				GPIOC->ODR &= ~GPIO_ODR_OD9; //Enciendo el segmento A
				GPIOB->ODR &= ~GPIO_ODR_OD8; //Enciendo el segmento B
				GPIOC->ODR |= GPIO_ODR_OD11; //Apaga el segmento C
				GPIOC->ODR &= ~GPIO_ODR_OD12; //Enciendo el segmento D
				GPIOC->ODR &= ~GPIO_ODR_OD10; //Enciendo el segmento E
				GPIOC->ODR |= GPIO_ODR_OD8; //Apaga el segmento F
				GPIOD->ODR &= ~GPIO_ODR_OD2; //Enciendo el segmento G
				break;

			case 3: //Activa los segmentos para escribir un 3
				GPIOC->ODR &= ~GPIO_ODR_OD9; //Enciendo el segmento A
				GPIOB->ODR &= ~GPIO_ODR_OD8; //Enciendo el segmento B
				GPIOC->ODR &= ~GPIO_ODR_OD11; //Enciendo el segmento C
				GPIOC->ODR &= ~GPIO_ODR_OD12; //Enciendo el segmento D
				GPIOC->ODR |= GPIO_ODR_OD10; //Apaga  el segmento E
				GPIOC->ODR |= GPIO_ODR_OD8; //Apaga el segmento F
				GPIOD->ODR &= ~GPIO_ODR_OD2; //Enciendo el segmento G
				break;

			case 4: //Activa los segmentos para escribir un 4
				GPIOC->ODR |= GPIO_ODR_OD9; //Apaga el segmento A
				GPIOB->ODR &= ~GPIO_ODR_OD8; //Enciendo el segmento B
				GPIOC->ODR &= ~GPIO_ODR_OD11; //Enciendo el segmento C
				GPIOC->ODR |= GPIO_ODR_OD12; //Apaga el segmento D
				GPIOC->ODR |= GPIO_ODR_OD10; //Apaga  el segmento E
				GPIOC->ODR &= ~GPIO_ODR_OD8; //Enciendo  el segmento F
				GPIOD->ODR &= ~GPIO_ODR_OD2; //Enciendo el segmento G
				break;

			case 5: //Activa los segmentos para escribir un 5
				GPIOC->ODR &= ~GPIO_ODR_OD9; //Enciendo el segmento A
				GPIOB->ODR |= GPIO_ODR_OD8; //Apaga el segmento B
				GPIOC->ODR &= ~GPIO_ODR_OD11; //Enciendo el segmento C
				GPIOC->ODR &= ~GPIO_ODR_OD12; //Enciendo el segmento D
				GPIOC->ODR |= GPIO_ODR_OD10; //Apaga  el segmento E
				GPIOC->ODR &= ~GPIO_ODR_OD8; //Enciendo  el segmento F
				GPIOD->ODR &= ~GPIO_ODR_OD2; //Enciendo el segmento G
				break;

			case 6: //Activa los segmentos para escribir un 6
				GPIOC->ODR &= ~GPIO_ODR_OD9; //Enciendo el segmento A
				GPIOB->ODR |= GPIO_ODR_OD8; //Apaga el segmento B
				GPIOC->ODR &= ~GPIO_ODR_OD11; //Enciendo el segmento C
				GPIOC->ODR &= ~GPIO_ODR_OD12; //Enciendo el segmento D
				GPIOC->ODR &= ~GPIO_ODR_OD10; //Enciendo el segmento E
				GPIOC->ODR &= ~GPIO_ODR_OD8; //Enciendo  el segmento F
				GPIOD->ODR &= ~GPIO_ODR_OD2; //Enciendo el segmento G
				break;

			case 7: //Activa los segmentos para escribir un 7
				GPIOC->ODR &= ~GPIO_ODR_OD9; //Enciendo el segmento A
				GPIOB->ODR &= ~GPIO_ODR_OD8; //Enciendo el segmento B
				GPIOC->ODR &= ~GPIO_ODR_OD11; //Enciendo el segmento C
				GPIOC->ODR |= GPIO_ODR_OD12; //Apaga el segmento D
				GPIOC->ODR |= GPIO_ODR_OD10; //Apaga el segmento E
				GPIOC->ODR |= GPIO_ODR_OD8; //Apaga el segmento F
				GPIOD->ODR |= GPIO_ODR_OD2; //Apaga el segmento G
				break;

			case 8: //Activa los segmentos para escribir un 8
				GPIOC->ODR &= ~GPIO_ODR_OD9; //Enciendo el segmento A
				GPIOB->ODR &= ~GPIO_ODR_OD8; //Enciendo el segmento B
				GPIOC->ODR &= ~GPIO_ODR_OD11; //Enciendo el segmento C
				GPIOC->ODR &= ~GPIO_ODR_OD12; //Enciendo el segmento D
				GPIOC->ODR &= ~GPIO_ODR_OD10; //Enciendo el segmento E
				GPIOC->ODR &= ~GPIO_ODR_OD8; //Enciendo  el segmento F
				GPIOD->ODR &= ~GPIO_ODR_OD2; //Enciendo el segmento G
				break;

			case 9: //Activa los segmentos para escribir un 9
				GPIOC->ODR &= ~GPIO_ODR_OD9; //Enciendo el segmento A
				GPIOB->ODR &= ~GPIO_ODR_OD8; //Apaga el segmento B
				GPIOC->ODR &= ~GPIO_ODR_OD11; //Enciendo el segmento C
				GPIOC->ODR &= ~GPIO_ODR_OD12; //Enciendo el segmento D
				GPIOC->ODR |= GPIO_ODR_OD10; //Apaga el segmento E
				GPIOC->ODR &= ~GPIO_ODR_OD8; //Enciendo  el segmento F
				GPIOD->ODR &= ~GPIO_ODR_OD2; //Enciendo el segmento G
				break;
	    }
}

