# Taller: Semáforo de 3 LEDs con SysTick — Timers sin interrupciones (CMSIS)

> **Asignatura:** Electrónica Digital
> **Tarjeta:** NUCLEO-F411RE (STM32F411RET6, ARM Cortex-M4)
> **Entorno:** VS Code + extensión oficial **STM32 for VS Code** (STMicroelectronics) + CMake
> **Enfoque:** Temporización con el **SysTick** del núcleo, por *polling* (sin interrupciones), acceso directo a registros con **CMSIS** (sin capa HAL)

---

## Tabla de contenido

1. [Objetivos](#1-objetivos)
2. [Crear el proyecto en VS Code](#2-crear-el-proyecto-en-vs-code)
3. [Incluir las rutas de drivers CMSIS en `CMakeLists.txt`](#3-incluir-las-rutas-de-drivers-cmsis-en-cmakeliststxt)
4. [Montaje físico](#4-montaje-físico)
5. [Los registros que vamos a tocar](#5-los-registros-que-vamos-a-tocar)
6. [Ejercicio — Semáforo de 3 LEDs con SysTick](#6-ejercicio--semáforo-de-3-leds-con-systick)
7. [Código fuente completo](#7-código-fuente-completo)
8. [Checklist / errores comunes](#8-checklist--errores-comunes)

---

## 1. Objetivos

- Configurar el **SysTick** del núcleo Cortex-M4 para generar una base de tiempo de 1 segundo, sin usar interrupciones.
- Usar un `switch-case` para cambiar de estado cada vez que se cumple la base de tiempo.
- Controlar 3 LEDs (semáforo) por registros CMSIS, sin HAL.

---

## 2. Crear el proyecto en VS Code

1. Abre VS Code y usa la extensión **STM32 for VS Code** para crear un proyecto nuevo.
2. Board/MCU: **NUCLEO-F411RE** / **STM32F411RET6**.
3. Nombre del proyecto: `TallerSemaforoSysTick`.
4. Verifica que se haya creado la carpeta `Src/` con `main.c`, el arranque `startup_stm32f411xx.S`, y el script de enlace `.ld`.

---

## 3. Incluir las rutas de drivers CMSIS en `CMakeLists.txt`

En `target_include_directories`, agrega las dos rutas de CMSIS de tu instalación local de `STM32Cube_FW_F4`:

```cmake
target_include_directories(${PROJECT_NAME} PRIVATE
  "<ruta-a-tu-STM32Cube>/Drivers/CMSIS/Include"
  "<ruta-a-tu-STM32Cube>/Drivers/CMSIS/Device/ST/STM32F4xx/Include"
)
```

> **Nota:** las rutas corresponden a tu instalación local de `STM32Cube_FW_F4`; ajústalas según donde la tengas instalada, pero la estructura interna (`Drivers/CMSIS/Include` y `Drivers/CMSIS/Device/ST/STM32F4xx/Include`) es siempre la misma.

No olvides tampoco la macro del chip en `target_compile_definitions`:

```cmake
target_compile_definitions(${PROJECT_NAME} PRIVATE
  STM32F411xE
)
```

---

## 4. Montaje físico

- 3 LEDs conectados así (los tres en el mismo puerto):
  - **LED Verde** → `PB8`
  - **LED Amarillo** → `PB9`
  - **LED Rojo** → `PB6`
- Aunque las resistencias son externas, en el código igual se limpia `PUPDR` a `00` (sin pull-up/pull-down) para cada pin, dejándolo en un estado conocido.
- Revisa el código de colores del cableado y que el protoboard esté ordenado antes de energizar la placa.

---

## 5. Los registros que vamos a tocar

### 5.1 Reloj de periféricos — `RCC->AHB1ENR`

| Bit 1 | Bit 0 |
| :---: | :---: |
| **`GPIOBEN`** (Puerto B) | GPIOAEN |

### 5.2 Registros GPIO (por puerto/pin)

| Registro | Bits/pin | Función |
| :--- | :---: | :--- |
| `MODER` | 2 bits | `01` = salida de propósito general |
| `OTYPER` | 1 bit | `0` = push-pull |
| `OSPEEDR` | 2 bits | `00` = baja velocidad |
| `PUPDR` | 2 bits | `00` = sin pull-up/pull-down |
| `ODR` | 1 bit | `1` = encendido, `0` = apagado |

### 5.3 SysTick (núcleo Cortex-M4)

| Registro | Función |
| :--- | :--- |
| `SysTick->LOAD` | Valor de recarga del contador (24 bits) |
| `SysTick->VAL` | Valor actual del contador |
| `SysTick->CTRL` | Bit 0 `ENABLE`, bit 1 `TICKINT`, bit 2 `CLKSOURCE`, bit 16 `COUNTFLAG` |

> **Nota:** en este taller `TICKINT` se deja en `0` (sin interrupción); el cambio de estado se detecta revisando el bit `COUNTFLAG` de `CTRL` dentro del `while(1)`.

---

## 6. Ejercicio — Semáforo de 3 LEDs con SysTick

### Paso a paso

1. **Habilitar el reloj del puerto B** (los 3 LEDs están en `GPIOB`, así que basta con un solo bit):

   ```c
   RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN; // Habilitar señal de reloj para GPIOB (Bit 1)
   ```

2. **Configurar `PB8`, `PB9` y `PB6` como salida push-pull, baja velocidad, sin pull-up/pull-down** (repite este bloque para cada pin, usando las macros CMSIS del bitfield de cada pin):

   ```c
   GPIOB->MODER    &= ~(GPIO_MODER_MODE8);   // Limpiar bits de modo para PB8
   GPIOB->MODER    |=  (GPIO_MODER_MODE8_0); // Configurar PB8 como salida (general purpose output)
   GPIOB->OTYPER   &= ~ GPIO_OTYPER_OT8;     // Configurar PB8 como salida push-pull
   GPIOB->OSPEEDR  &= ~(GPIO_OSPEEDR_OSPEED8); // Configurar PB8 como baja velocidad
   GPIOB->PUPDR    &= ~(GPIO_PUPDR_PUPD8);   // Configurar PB8 sin pull-up/pull-down
   GPIOB->ODR      &= ~(GPIO_ODR_OD8);       // Inicializar PB8 en bajo
   ```

   Y lo mismo para `PB9` (amarillo) y `PB6` (rojo), cambiando el número de pin en cada macro.

3. **Configurar el SysTick para 1 segundo, sin interrupciones** (se arma `CTRL` limpiando primero cada bit y luego poniéndolo, en vez de asignar todo el registro de una sola vez):

   ```c
   SysTick->LOAD = 16000000 - 1;                    // Valor de recarga del contador
   SysTick->VAL  = 0;                                // Reinicia el contador actual
   SysTick->CTRL &= ~(SysTick_CTRL_CLKSOURCE_Msk);   // Deshabilitar interrupciones del Systick
   SysTick->CTRL |= (SysTick_CTRL_CLKSOURCE_Msk);    // Seleccionar reloj del procesador (16 MHz)

   SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk; // Deshabilitar el contador del Systick
   SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk;  // Habilitar el contador del Systick
   ```

4. **En el `while(1)`, revisar `COUNTFLAG` y avanzar un `switch-case`** (cada `case` enciende un LED y apaga los otros dos directamente sobre `ODR`, sin una función aparte para apagar todos):

   ```c
   if (SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk)
   {
       switch (estado_actual)
       {
           case 0:
               GPIOB->ODR |= (GPIO_ODR_OD8);  // Encender LED verde
               GPIOB->ODR &= ~(GPIO_ODR_OD9); // Apagar LED amarillo
               GPIOB->ODR &= ~(GPIO_ODR_OD6); // Apagar LED rojo
               estado_actual++;
               break;

           case 1:
               GPIOB->ODR |= (GPIO_ODR_OD9);  // Encender LED amarillo
               GPIOB->ODR &= ~(GPIO_ODR_OD8); // Apagar LED verde
               GPIOB->ODR &= ~(GPIO_ODR_OD6); // Apagar LED rojo
               estado_actual++;
               break;

           case 2:
               GPIOB->ODR &= ~(GPIO_ODR_OD8); // Apagar LED verde
               GPIOB->ODR &= ~(GPIO_ODR_OD9); // Apagar LED amarillo
               GPIOB->ODR |= (GPIO_ODR_OD6);  // Encender LED rojo
               estado_actual = 0;
               break;

           default:
               estado_actual = 0;
               break;
       }
   }
   ```

---

## 7. Código fuente completo

```c
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
```

---

## 8. Checklist / errores comunes

- [ ] `#include <stm32f4xx.h>` marca error o no reconoce `RCC`/`GPIOB`/`SysTick` → revisa las dos rutas de `target_include_directories` y corre `CMake: Configure` de nuevo.
- [ ] Los LEDs nunca cambian de color → revisa que **primero** se habilitó el reloj en `RCC->AHB1ENR` (bit `GPIOBEN`) antes de tocar `GPIOB`. Sin esto, escribir en `MODER`/`ODR` no tiene efecto.
- [ ] El semáforo cambia demasiado rápido o demasiado lento → revisa el valor de `SysTick->LOAD`; si tu tarjeta no corre a 16 MHz por alguna configuración de reloj distinta, el cálculo cambia.
- [ ] Intentaste usar la variable `SystemCoreClock` y el proyecto no enlaza (*undefined reference*) → en este proyecto `system_stm32f4xx.c` no está compilado, así que esa variable no existe; usa el valor de reloj fijo (`16000000 - 1` para 1 segundo a 16 MHz) como se hizo aquí.
- [ ] Aunque las resistencias limitadoras sean externas, limpia siempre `PUPDR` explícitamente (`&= ~(GPIO_PUPDR_PUPDx)`) para cada pin de salida; no asumas que el valor de reset ya te sirve.
- [ ] `COUNTFLAG` nunca se pone en 1 → revisa que `SysTick->CTRL` tenga el bit `ENABLE` en 1 (usa `SysTick_CTRL_ENABLE_Msk`), y que `LOAD` no haya quedado en 0.
- [ ] `MODER` quedó en un valor raro → asegúrate de **limpiar antes de asignar** (`&= ~(...)` seguido de `|= (...)`), nunca solo `|=` directo sobre un registro que puede tener basura de otro modo.
- [ ] Olvidaste la macro `STM32F411xE` en `target_compile_definitions` → sin ella, `stm32f4xx.h` no sabe qué variante compilar y puede fallar la compilación o generar mapeos de memoria incorrectos.
