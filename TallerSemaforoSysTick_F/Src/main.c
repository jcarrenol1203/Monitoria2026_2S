/**
 ******************************************************************************
 * @file           : main.c
 * @author         : Taller de Electrónica Digital - Monitoría
 * @brief          : Semáforo de 3 LEDs con SysTick, sin interrupciones (CMSIS)
 * @board          : STM32F411RE (NUCLEO-F411RE)
 ******************************************************************************
 */

#include <stm32f4xx.h>
#include <stdint.h>

uint8_t estado_actual = 0; 

int main(void)
{
    
    /* =========================================================================
     * PASO 1: HABILITAR RELOJES DE GPIOB (RCC->AHB1ENR)
     * Manual de Referencia RM0383 - Seccion 6.3.9
     * ========================================================================= */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN; // Habilitar señal de reloj para GPIOB (Bit 1)
    /* =========================================================================
     * PASO 2: CONFIGURAR PB8, PB9 Y PB6 COMO SALIDA PUSH-PULL, BAJA VELOCIDAD
     * (No se toca PUPDR: las resistencias ya están puestas en el protoboard)
     * Manual de Referencia RM0>383 - Seccion 8.4
     * ========================================================================= */
    /*
    Verde
    */
     GPIOB->MODER    &= ~ (GPIO_MODER_MODE8); // Limpiar bits de modo para PB8;
     GPIOB->MODER    |=  (GPIO_MODER_MODE8_0); // Configurar PB8 como salida (general purpose output)
     GPIOB->OTYPER   &= ~ GPIO_OTYPER_OT8; // Configurar PB8 como salida push-pull
     GPIOB->OSPEEDR  &= ~ (GPIO_OSPEEDR_OSPEED8); // Configurar PB8 como baja velocidad
     GPIOB->PUPDR    &= ~ (GPIO_PUPDR_PUPD8); // Configurar PB8 sin pull-up/pull-down
     GPIOB->ODR      &= ~ (GPIO_ODR_OD8); // Inicializar PB8 en bajo

     /*
     Amarillo
     */
     GPIOB->MODER    &= ~ (GPIO_MODER_MODE9); // Limpiar bits de modo para PB9;
     GPIOB->MODER    |=  (GPIO_MODER_MODE9_0); // Configurar PB9 como salida (general purpose output)
     GPIOB->OTYPER   &= ~ GPIO_OTYPER_OT9; // Configurar PB9 como salida push-pull
     GPIOB->OSPEEDR  &= ~ (GPIO_OSPEEDR_OSPEED9); // Configurar PB9 como baja velocidad
     GPIOB->PUPDR    &= ~ (GPIO_PUPDR_PUPD9); // Configurar PB9 sin pull-up/pull-down
     GPIOB->ODR      &= ~ (GPIO_ODR_OD9); // Inicializar PC9 en bajo

     /*
     Rojo
     */
     GPIOB->MODER    &= ~ (GPIO_MODER_MODE6); // Limpiar bits de modo para PB6;
     GPIOB->MODER    |=  (GPIO_MODER_MODE6_0); // Configurar PB6 como salida (general purpose output)
     GPIOB->OTYPER   &= ~ GPIO_OTYPER_OT6; // Configurar PB6 como salida push-pull
     GPIOB->OSPEEDR  &= ~ (GPIO_OSPEEDR_OSPEED6); // Configurar PB6 como baja velocidad
     GPIOB->PUPDR    &= ~ (GPIO_PUPDR_PUPD6); // Configurar PB6 sin pull-up/pull-down
     GPIOB->ODR      &= ~ (GPIO_ODR_OD6); // Inicializar PB6 en bajo



    

    /* =========================================================================
     * PASO 3: CONFIGURAR EL SYSTICK PARA QUE CUENTE 1000 ms, SIN INTERRUPCIONES
     * (Core Cortex-M4 - core_cm4.h)
     * ========================================================================= */
    SysTick->LOAD = 16000000 - 1;      // Valor de recarga del contador
    SysTick->VAL  = 0;                         // Reinicia el contador actual
    SysTick->CTRL &= ~(SysTick_CTRL_CLKSOURCE_Msk); // Deshabilitar interrupciones del Systick
    SysTick->CTRL |= (SysTick_CTRL_CLKSOURCE_Msk); // Seleccionar reloj del procesador (16 MHz)

    SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk; // Deshabilitar el contador del Systick
    SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk; // Habilitar el contador del Systick

    /* =========================================================================
     * BUCLE PRINCIPAL (SUPER LOOP)
     * ========================================================================= */
    while (1)
    {
        // El bit COUNTFLAG se pone en 1 cuando el contador llega a 0,
        // y se limpia automaticamente al leer CTRL.
        if (SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk)
        {

            switch (estado_actual)
            {
                case 0:
                    GPIOB->ODR |= (GPIO_ODR_OD8); // Encender LED verde
                    GPIOB->ODR &= ~(GPIO_ODR_OD9); // Apagar LED amarillo
                    GPIOB->ODR &= ~(GPIO_ODR_OD6); // Apagar LED rojo
                    estado_actual ++;
                    break;

                case 1:
                    GPIOB->ODR |= (GPIO_ODR_OD9); // Encender LED amarillo
                    GPIOB->ODR &= ~(GPIO_ODR_OD8); // Apagar LED verde
                    GPIOB->ODR &= ~(GPIO_ODR_OD6); // Apagar LED rojo
                    estado_actual ++;
                   
                    break;
                   

                case 2:
                    GPIOB->ODR &= ~(GPIO_ODR_OD8); // Apagar LED verde
                    GPIOB->ODR &= ~(GPIO_ODR_OD9); // Apagar LED amarillo
                    GPIOB->ODR |= (GPIO_ODR_OD6); // Encender LED rojo
                    estado_actual = 0;
                    break;

                default:
                    estado_actual = 0;
                    break;
            }
        }
    }
}