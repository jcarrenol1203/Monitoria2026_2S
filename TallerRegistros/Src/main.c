/**
 ******************************************************************************
 * @file           : main.c
 * @author         : Juan Andres Carreno Londono / Miguel Angel Bedoya Gonzalez
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */

 //Includes
#include <stdint.h>
#include "stm32f4xx.h"

//Definicion Variables

//Cabecera funciones
void init_hardware(void);


//Main
int main(void)
{
    //Inicializacion de variables
    init_hardware();
    //Inicializacion de perifericos

    //Bucle infinito
    while (1)
    {
        if ((GPIOC->IDR & (0b1 << 13)) == 0) //Si el boton esta presionado
        {
            GPIOA->ODR |= (0b1 << 5); //Apagar el led
        }
        else
        {
            GPIOA->ODR &= ~(0b1 << 5); //Encender el led
        }
        //Codigo principal
    }
    return 0;
}

//Funciones
void init_hardware(void)
{

   
    //Encender la señal de reloj para PA5 y PC13.
    RCC->AHB1ENR |= (0b1 << 0); //Habilitar reloj para GPIOA
    RCC->AHB1ENR |= (0b1 << 2); //Habilitar reloj para GPIOC

    GPIOA->MODER &= ~(0b11 << (5 * 2)); //Limpiar bits de modo para PA5
    GPIOA->MODER |= (0b01 << (5 * 2)); //Configurar PA5 como salida (modo general purpose output)

    GPIOC->MODER &= ~(0b11 << (13 * 2)); //Limpiar bits de modo para PC13 y modo entrada.

    GPIOA->OTYPER &= ~(0b1 << 5); //Configurar PA5 como salida push-pull

    GPIOA->OSPEEDR |= (0b11 << (5 * 2)); //Configurar PA5 como alta velocidad

    GPIOA->PUPDR &= ~(0b11 << (5 * 2)); //Configurar PA5 sin pull-up/pull-down

    GPIOA->ODR  |= (0b1 << 5); //Inicializar PA5 en alto


    
    


    //Inicializacion de perifericos
}