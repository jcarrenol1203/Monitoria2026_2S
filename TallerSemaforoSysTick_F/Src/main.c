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

/* Pines de los 3 LEDs del semáforo (resistencias externas ya puestas físicamente,
 * por eso no se configura PUPDR para ninguno de estos pines). */
#define LED_VERDE_PORT      GPIOA
#define LED_VERDE_PIN       5   // PB8

#define LED_AMARILLO_PORT   GPIOC
#define LED_AMARILLO_PIN    8   // PC8

#define LED_ROJO_PORT       GPIOC
#define LED_ROJO_PIN        9   // PC9

/* HCLK = 16 MHz (HSI interno, valor de reset; no se llama SystemClock_Config). */
#define SYSTICK_RELOAD_500MS (8000000UL - 1UL)

/* Estados del semáforo (switch-case simple, todavía sin máquina de estados) */
#define ESTADO_VERDE     0
#define ESTADO_AMARILLO  1
#define ESTADO_ROJO      2

static void semaforo_ApagarTodos(void);

int main(void)
{
    uint8_t estado_actual = ESTADO_VERDE;

    /* =========================================================================
     * PASO 1: HABILITAR RELOJES DE GPIOB Y GPIOC (RCC->AHB1ENR)
     * Manual de Referencia RM0383 - Seccion 6.3.9
     * ========================================================================= */
    RCC->AHB1ENR |= (1 << 0); // Habilitar señal de reloj para GPIOB (Bit 1)
    RCC->AHB1ENR |= (1 << 2); // Habilitar señal de reloj para GPIOC (Bit 2)

    /* =========================================================================
     * PASO 2: CONFIGURAR PB8, PC8 Y PC9 COMO SALIDA PUSH-PULL, BAJA VELOCIDAD
     * (No se toca PUPDR: las resistencias ya están puestas en el protoboard)
     * Manual de Referencia RM0383 - Seccion 8.4
     * ========================================================================= */
    LED_VERDE_PORT->MODER    &= ~(0b11 << (LED_VERDE_PIN * 2));
    LED_VERDE_PORT->MODER    |=  (0b01 << (LED_VERDE_PIN * 2));
    LED_VERDE_PORT->OTYPER   &= ~(1 << LED_VERDE_PIN);
    LED_VERDE_PORT->OSPEEDR  &= ~(0b11 << (LED_VERDE_PIN * 2));

    LED_AMARILLO_PORT->MODER   &= ~(0b11 << (LED_AMARILLO_PIN * 2));
    LED_AMARILLO_PORT->MODER   |=  (0b01 << (LED_AMARILLO_PIN * 2));
    LED_AMARILLO_PORT->OTYPER  &= ~(1 << LED_AMARILLO_PIN);
    LED_AMARILLO_PORT->OSPEEDR &= ~(0b11 << (LED_AMARILLO_PIN * 2));

    LED_ROJO_PORT->MODER    &= ~(0b11 << (LED_ROJO_PIN * 2));
    LED_ROJO_PORT->MODER    |=  (0b01 << (LED_ROJO_PIN * 2));
    LED_ROJO_PORT->OTYPER   &= ~(1 << LED_ROJO_PIN);
    LED_ROJO_PORT->OSPEEDR  &= ~(0b11 << (LED_ROJO_PIN * 2));

    /* =========================================================================
     * PASO 3: CONFIGURAR EL SYSTICK PARA QUE CUENTE 500 ms, SIN INTERRUPCIONES
     * (Core Cortex-M4 - core_cm4.h)
     * ========================================================================= */
    SysTick->LOAD = SYSTICK_RELOAD_500MS;      // Valor de recarga del contador
    SysTick->VAL  = 0;                         // Reinicia el contador actual
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk // Reloj del procesador (16 MHz)
                  | SysTick_CTRL_ENABLE_Msk;   // Habilita el SysTick
                                                // (TICKINT en 0: sin interrupción)

    semaforo_ApagarTodos();
    LED_VERDE_PORT->ODR |= (1 << LED_VERDE_PIN); // Arranca en verde

    /* =========================================================================
     * BUCLE PRINCIPAL (SUPER LOOP)
     * ========================================================================= */
    while (1)
    {
        // El bit COUNTFLAG se pone en 1 cuando el contador llega a 0,
        // y se limpia automaticamente al leer CTRL.
        if (SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk)
        {
            semaforo_ApagarTodos();

            switch (estado_actual)
            {
                case ESTADO_VERDE:
                    LED_AMARILLO_PORT->ODR |= (1 << LED_AMARILLO_PIN);
                    estado_actual = ESTADO_AMARILLO;
                    break;

                case ESTADO_AMARILLO:
                    LED_ROJO_PORT->ODR |= (1 << LED_ROJO_PIN);
                    estado_actual = ESTADO_ROJO;
                    break;

                case ESTADO_ROJO:
                    LED_VERDE_PORT->ODR |= (1 << LED_VERDE_PIN);
                    estado_actual = ESTADO_VERDE;
                    break;

                default:
                    estado_actual = ESTADO_VERDE;
                    break;
            }
        }
    }
}

static void semaforo_ApagarTodos(void)
{
    LED_VERDE_PORT->ODR    &= ~(1 << LED_VERDE_PIN);
    LED_AMARILLO_PORT->ODR &= ~(1 << LED_AMARILLO_PIN);
    LED_ROJO_PORT->ODR     &= ~(1 << LED_ROJO_PIN);
}
