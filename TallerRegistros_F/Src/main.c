/**
 ******************************************************************************
 * @file           : main.c
 * @author         : Taller de Electrónica Digital - Monitoría
 * @brief          : Hola Mundo (PA5) y Lectura de Pulsador (PC13) con CMSIS
 * @board          : STM32F411RE (NUCLEO-F411RE)
 ******************************************************************************
 */

#include <stm32f4xx.h>
#include <stdint.h>

// Variable global para visualizar estado en depuracion
volatile uint8_t estado_pulsador = 0;

int main(void)
{
    /* =========================================================================
     * PASO 1: HABILITAR RELOJES PERIFÉRICOS (RCC_AHB1ENR)
     * Manual de Referencia RM0383 - Seccion 6.3.9
     * ========================================================================= */
    RCC->AHB1ENR |= (1 << 0); // Habilitar señal de reloj para GPIOA (Bit 0)
    RCC->AHB1ENR |= (1 << 2); // Habilitar señal de reloj para GPIOC (Bit 2)

    /* =========================================================================
     * PASO 2: CONFIGURAR GPIOA - PIN 5 (LED LD2 VERDE)
     * Manual de Referencia RM0383 - Seccion 8.4
     * ========================================================================= */
    // 2.1 Configurar modo como Salida (MODER5 = 01)
    GPIOA->MODER &= ~(0b11 << (5 * 2)); // Limpiar bits 11:10
    GPIOA->MODER |=  (0b01 << (5 * 2)); // Asignar 01 (Salida)

    // 2.2 Configurar tipo de salida como Push-Pull (OTYPER5 = 0)
    GPIOA->OTYPER &= ~(1 << 5);

    // 2.3 Configurar velocidad como Baja (OSPEEDR5 = 00)
    GPIOA->OSPEEDR &= ~(0b11 << (5 * 2));

    // 2.4 Sin resistencias internas de Pull-up/Pull-down (PUPDR5 = 00)
    GPIOA->PUPDR &= ~(0b11 << (5 * 2));

    /* =========================================================================
     * PASO 3: CONFIGURAR GPIOC - PIN 13 (PULSADOR USER B1)
     * Manual de Referencia RM0383 - Seccion 8.4
     * ========================================================================= */
    // 3.1 Configurar modo como Entrada (MODER13 = 00)
    GPIOC->MODER &= ~(0b11 << (13 * 2)); // Limpiar bits 27:26

    // 3.2 Sin pull-up/pull-down interno ya que cuenta con pull-up externo
    GPIOC->PUPDR &= ~(0b11 << (13 * 2));

    /* =========================================================================
     * BUCLE PRINCIPAL (SUPER LOOP)
     * ========================================================================= */
    while (1)
    {
        // Evaluamos el bit 13 del Registro de Datos de Entrada de GPIOC (IDR)
        if ((GPIOC->IDR & (1 << 13)) == 0)
        {
            // Boton Presionado (GND = Nivel Logico 0)
            GPIOA->ODR |= (1 << 5);   // Encender LED PA5
            estado_pulsador = 1;
        }
        else
        {
            // Boton Suelto (VCC = Nivel Logico 1 por Pull-up)
            GPIOA->ODR &= ~(1 << 5);  // Apagar LED PA5
            estado_pulsador = 0;
        }
    }
}
