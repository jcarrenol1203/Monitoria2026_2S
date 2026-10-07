#include <stdint.h>
#include "stm32f411xe.h"
#include <stm32f4xx.h>

//Definición de variables
uint8_t cambio = 0; // Variable para indicar el cambio de estado del semáforo
uint8_t parpadeos = 0;


//Cabecera de funciones

void init_GPIO(void);
void init_timers(void);

typedef enum {
    Rojo,

    Amarillo,
    VerdeE,
    VerdeP
} EstadoSemaforo ;

EstadoSemaforo estado_actual = Rojo; // Estado inicial del semáforo

//Main


int main(void){

    init_GPIO();
    init_timers();



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

                        TIM2->ARR = 250;
                        GPIOB->ODR ^= GPIO_ODR_OD8; // Alternar LED verde
                        parpadeos++;
                

                        if (parpadeos == 5*2) {
                            parpadeos = 0;
                        estado_actual = Amarillo;
                        TIM2->ARR = 1000;
                        
                     }
                     break;
                     

                     
                    



                    



                case Amarillo:
                    GPIOB->ODR |= GPIO_ODR_OD9; // Encender LED amarillo
                    GPIOB->ODR &= ~GPIO_ODR_OD6; // Apagar LED rojo
                    GPIOB->ODR &= ~GPIO_ODR_OD8; // Apagar LED verde

                    TIM2->ARR = 1000; // Configurar el auto-reload para 1 segundo
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

    //Señal de reloj GPIOB

    RCC->AHB1ENR &= ~RCC_AHB1ENR_GPIOBEN; // Limpiar bit de habilitación de reloj para GPIOB
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN; 


    //Verde PB8, Amarillo PB9, Rojo PB6.

    GPIOB->MODER &= ~(GPIO_MODER_MODE8 | GPIO_MODER_MODE9 | GPIO_MODER_MODE6); // Limpiar bits de modo para PB8, PB9 y PB6
    GPIOB->MODER |= (GPIO_MODER_MODE8_0 | GPIO_MODER_MODE9_0 | GPIO_MODER_MODE6_0); // Configurar PB8, PB9 y PB6 como salida (general purpose output)
    GPIOB->OTYPER &= ~(GPIO_OTYPER_OT8 | GPIO_OTYPER_OT9 | GPIO_OTYPER_OT6); // Configurar PB8, PB9 y PB6 como salida push-pull
    GPIOB->OSPEEDR &= ~(GPIO_OSPEEDR_OSPEED8 | GPIO_OSPEEDR_OSPEED9 | GPIO_OSPEEDR_OSPEED6); // Configurar PB8, PB9 y PB6 como baja velocidad
    GPIOB->PUPDR &= ~(GPIO_PUPDR_PUPD8 | GPIO_PUPDR_PUPD9 | GPIO_PUPDR_PUPD6); // Configurar PB8, PB9 y PB6 sin pull-up/pull-down
    GPIOB->ODR &= ~(GPIO_ODR_OD8 | GPIO_ODR_OD9 | GPIO_ODR_OD6); // Inicializar PB8, PB9 y PB6 en bajo

}

void init_timers(void){
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN; // Habilitar el reloj para TIM2
    TIM2->PSC = 16000 - 1; // Configurar el prescaler
    TIM2->ARR = 1000 - 1; // Configurar el auto-reload
    TIM2->CNT = 0; // Inicializar el contador en 0


    TIM2->CR1 &= ~TIM_CR1_DIR;
    TIM2->CR1 &= ~TIM_CR1_ARPE;
    TIM2->CR1 |= TIM_CR1_ARPE;

    TIM2->SR &= ~TIM_SR_UIF; // Limpiar la bandera de actualización
    TIM2->DIER |= TIM_DIER_UIE; // Habilitar la interrupción de actualización

    __NVIC_EnableIRQ(TIM2_IRQn); // Habilitar la interrupción de TIM2 en el NVIC

    TIM2->CR1 &= ~TIM_CR1_CEN; // Deshabilitar el temporizador antes de configurarlo
    TIM2->CR1 |= TIM_CR1_CEN; // Habilitar el temporizador


}

void TIM2_IRQHandler(void){
    if (TIM2->SR & TIM_SR_UIF) { // Verificar si la bandera de actualización está activa
        TIM2->SR &= ~TIM_SR_UIF; // Limpiar la bandera de actualización

        cambio = 1;
    }
}