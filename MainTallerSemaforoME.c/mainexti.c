#include <stdint.h>
#include "stm32f411xe.h"
#include <stm32f4xx.h>

//Definición de variables
volatile uint8_t cambio = 0; // Variable para indicar el cambio de estado del semáforo
uint8_t parpadeos = 0;

//Cabecera de funciones

void init_GPIO(void);
void init_EXTI(void);
void init_timer(void);

typedef enum {
    Rojo,
    VerdeE,
    VerdeP,
    Amarillo
} EstadoSemaforo ;

EstadoSemaforo estado_actual = Rojo; // Estado inicial del semáforo

//Main


int main(void){

    init_GPIO();
    init_EXTI();
    init_timer();



    while(1){
        if (cambio == 1){
            cambio = 0;

            switch (estado_actual){

                    //Verde PB8, Amarillo PB9, Rojo PB6.
                case Rojo:
                    GPIOB->ODR |= GPIO_ODR_OD6; // Encender LED rojo
                    GPIOB->ODR &= ~GPIO_ODR_OD8; // Apagar LED verde
                    GPIOB->ODR &= ~GPIO_ODR_OD9; // Apagar LED amarillo

                    estado_actual = VerdeE;
                    break;

                case VerdeE:
                    GPIOB->ODR |= GPIO_ODR_OD8; // Encender LED verde
                    GPIOB->ODR &= ~GPIO_ODR_OD9; // Apagar LED amarillo
                    GPIOB->ODR &= ~GPIO_ODR_OD6; // Apagar LED rojo

                    estado_actual = VerdeP;
                    break;

                case VerdeP:

                        GPIOB->ODR ^= GPIO_ODR_OD8; // Alternar LED verde
                        parpadeos++;


                        if (parpadeos == 5*2) {
                            parpadeos = 0;
                        estado_actual = Amarillo;

                     }
                     break;

                case Amarillo:
                    GPIOB->ODR |= GPIO_ODR_OD9; // Encender LED amarillo
                    GPIOB->ODR &= ~GPIO_ODR_OD6; // Apagar LED rojo
                    GPIOB->ODR &= ~GPIO_ODR_OD8; // Apagar LED verde

                    estado_actual = Rojo;
                    break;

                default:
                    estado_actual = Rojo;
                    break;
            }


        }

    }
    return 0;
}

//Funciones
void init_GPIO(void){

    //Señal de reloj GPIOB (LEDs) y GPIOC (botón)

    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;


    //Verde PB8, Amarillo PB9, Rojo PB6.

    GPIOA->MODER &= ~(GPIO_MODER_MODE5); // Limpiar bits de modo para PA5
    GPIOA->MODER |= (GPIO_MODER_MODE5_0); // Configurar PA5 como salida (general purpose output)
    GPIOA->OTYPER &= ~(GPIO_OTYPER_OT5); // Configurar PA5 como salida push-pull
    GPIOA->OSPEEDR &= ~(GPIO_OSPEEDR_OSPEED5); // Configurar PA5 como baja velocidad
    GPIOA->PUPDR &= ~(GPIO_PUPDR_PUPD5); // Configurar PA5 sin pull-up/pull-down
    GPIOA->ODR &= ~(GPIO_ODR_OD5); // Inicializar PA5 en bajo

    GPIOB->MODER &= ~(GPIO_MODER_MODE8 | GPIO_MODER_MODE9 | GPIO_MODER_MODE6); // Limpiar bits de modo para PB8, PB9 y PB6
    GPIOB->MODER |= (GPIO_MODER_MODE8_0 | GPIO_MODER_MODE9_0 | GPIO_MODER_MODE6_0); // Configurar PB8, PB9 y PB6 como salida (general purpose output)
    GPIOB->OTYPER &= ~(GPIO_OTYPER_OT8 | GPIO_OTYPER_OT9 | GPIO_OTYPER_OT6); // Configurar PB8, PB9 y PB6 como salida push-pull
    GPIOB->OSPEEDR &= ~(GPIO_OSPEEDR_OSPEED8 | GPIO_OSPEEDR_OSPEED9 | GPIO_OSPEEDR_OSPEED6); // Configurar PB8, PB9 y PB6 como baja velocidad
    GPIOB->PUPDR &= ~(GPIO_PUPDR_PUPD8 | GPIO_PUPDR_PUPD9 | GPIO_PUPDR_PUPD6); // Configurar PB8, PB9 y PB6 sin pull-up/pull-down
    GPIOB->ODR &= ~(GPIO_ODR_OD8 | GPIO_ODR_OD9 | GPIO_ODR_OD6); // Inicializar PB8, PB9 y PB6 en bajo


    //Botón de usuario PC13 (entrada, la tarjeta ya tiene pull-up externa)

    GPIOC->MODER &= ~GPIO_MODER_MODE13; // PC13 como entrada (00)
    GPIOC->PUPDR &= ~GPIO_PUPDR_PUPD13; // Sin pull-up/pull-down interno

}

void init_EXTI(void){
    RCC->APB2ENR &= ~(RCC_APB2ENR_SYSCFGEN); // Deshabilitar el reloj para SYSCFG (por si acaso)
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN; // Habilitar el reloj para SYSCFG (conecta pines con líneas EXTI)

    SYSCFG->EXTICR[3] &= ~SYSCFG_EXTICR4_EXTI13; // Limpiar la selección de puerto para la línea 13
    SYSCFG->EXTICR[3] |= SYSCFG_EXTICR4_EXTI13_PC; // La línea EXTI13 viene del puerto C (PC13)

    EXTI->RTSR &= ~EXTI_RTSR_TR13; // Sin disparo por flanco de subida
    EXTI->FTSR |= EXTI_FTSR_TR13; // Disparo por flanco de bajada (el botón conecta PC13 a GND al presionar)

    EXTI->PR = EXTI_PR_PR13; // Limpiar la bandera pendiente (se limpia escribiendo 1)
    EXTI->IMR |= EXTI_IMR_MR13; // Desenmascarar la línea 13 (habilitar la interrupción)

    __NVIC_EnableIRQ(EXTI15_10_IRQn); // Habilitar en el NVIC la interrupción de las líneas EXTI 10 a 15

}

void init_timer(void){
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN; // Habilitar el reloj para TIM2
    TIM2->PSC = 16000 - 1; // Prescaler para contar en milisegundos (16 MHz / 16000 = 1 kHz)
    TIM2->ARR = 500 - 1; // Auto-reload para 1/2
    TIM2->CNT = 0; // Inicializar el contador en 0

    TIM2->DIER |= TIM_DIER_UIE; // Habilitar la interrupción de actualización (Update Interrupt)
    TIM2->CR1 |= TIM_CR1_DIR; // Contador ascendente
    TIM2->CR1 |= TIM_CR1_ARPE; // Habilitar la precarga del auto-reload
    

    TIM2->SR &= ~TIM_SR_UIF; // Limpiar la bandera de actualización
    NVIC_EnableIRQ(TIM2_IRQn); // Habilitar la interrupción de TIM
    TIM2->CR1 |= TIM_CR1_CEN; // Habilitar el contador


}

void EXTI15_10_IRQHandler(void){
    if (EXTI->PR & EXTI_PR_PR13) { // Verificar si la línea 13 generó la interrupción
        EXTI->PR |= EXTI_PR_PR13; // Limpiar la bandera pendiente (escribiendo 1)

        cambio = 1;
         
    }
}

void TIM2_IRQHandler(void){
    if (TIM2->SR & TIM_SR_UIF) { // Verificar si la interrupción fue por actualización
        TIM2->SR &= ~TIM_SR_UIF; // Limpiar la bandera de actualización

        GPIOA->ODR ^= GPIO_ODR_ODR_5; // Cambiar el estado del semáforo
    }
}
