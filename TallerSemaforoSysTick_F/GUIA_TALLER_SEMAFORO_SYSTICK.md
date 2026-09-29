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

- Configurar el **SysTick** del núcleo Cortex-M4 para generar una base de tiempo de 500 ms, sin usar interrupciones.
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

- 3 LEDs conectados así:
  - **LED Verde** → `PB8`
  - **LED Amarillo** → `PC8`
  - **LED Rojo** → `PC9`
- Cada LED ya tiene su resistencia limitadora físicamente puesta en el protoboard — **por eso en el código no se configura `PUPDR`** para estos pines.
- Revisa el código de colores del cableado y que el protoboard esté ordenado antes de energizar la placa.

---

## 5. Los registros que vamos a tocar

### 5.1 Reloj de periféricos — `RCC->AHB1ENR`

| Bit 2 | Bit 1 | Bit 0 |
| :---: | :---: | :---: |
| **`GPIOCEN`** (Puerto C) | **`GPIOBEN`** (Puerto B) | GPIOAEN |

### 5.2 Registros GPIO (por puerto/pin)

| Registro | Bits/pin | Función |
| :--- | :---: | :--- |
| `MODER` | 2 bits | `01` = salida de propósito general |
| `OTYPER` | 1 bit | `0` = push-pull |
| `OSPEEDR` | 2 bits | `00` = baja velocidad |
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

1. **Habilitar el reloj de los puertos B y C:**

   ```c
   RCC->AHB1ENR |= (1 << 1); // GPIOB
   RCC->AHB1ENR |= (1 << 2); // GPIOC
   ```

2. **Configurar `PB8`, `PC8` y `PC9` como salida push-pull, baja velocidad** (repite este bloque para cada pin, cambiando el puerto y el número de pin):

   ```c
   GPIOB->MODER   &= ~(0b11 << (8 * 2));
   GPIOB->MODER   |=  (0b01 << (8 * 2));
   GPIOB->OTYPER  &= ~(1 << 8);
   GPIOB->OSPEEDR &= ~(0b11 << (8 * 2));
   ```

3. **Configurar el SysTick para 500 ms, sin interrupciones:**

   ```c
   SysTick->LOAD = (8000000UL - 1UL); // 500 ms @ 16 MHz (HSI por defecto)
   SysTick->VAL  = 0;
   SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;
   ```

4. **En el `while(1)`, revisar `COUNTFLAG` y avanzar un `switch-case`:**

   ```c
   if (SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk)
   {
       // apagar los 3 LEDs y luego, según el estado_actual, encender el siguiente
       switch (estado_actual)
       {
           case ESTADO_VERDE:
               // encender amarillo
               estado_actual = ESTADO_AMARILLO;
               break;
           case ESTADO_AMARILLO:
               // encender rojo
               estado_actual = ESTADO_ROJO;
               break;
           case ESTADO_ROJO:
               // encender verde
               estado_actual = ESTADO_VERDE;
               break;
       }
   }
   ```

---

## 7. Código fuente completo

```c
#include <stm32f4xx.h>
#include <stdint.h>

#define LED_VERDE_PORT      GPIOB
#define LED_VERDE_PIN       8

#define LED_AMARILLO_PORT   GPIOC
#define LED_AMARILLO_PIN    8

#define LED_ROJO_PORT       GPIOC
#define LED_ROJO_PIN        9

#define SYSTICK_RELOAD_500MS (8000000UL - 1UL)

#define ESTADO_VERDE     0
#define ESTADO_AMARILLO  1
#define ESTADO_ROJO      2

static void semaforo_ApagarTodos(void);

int main(void)
{
    uint8_t estado_actual = ESTADO_VERDE;

    RCC->AHB1ENR |= (1 << 1);
    RCC->AHB1ENR |= (1 << 2);

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

    SysTick->LOAD = SYSTICK_RELOAD_500MS;
    SysTick->VAL  = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;

    semaforo_ApagarTodos();
    LED_VERDE_PORT->ODR |= (1 << LED_VERDE_PIN);

    while (1)
    {
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
```

---

## 8. Checklist / errores comunes

- [ ] `#include <stm32f4xx.h>` marca error o no reconoce `RCC`/`GPIOB`/`GPIOC`/`SysTick` → revisa las dos rutas de `target_include_directories` y corre `CMake: Configure` de nuevo.
- [ ] Los LEDs nunca cambian de color → revisa que **primero** se habilitó el reloj en `RCC->AHB1ENR` antes de tocar `GPIOB`/`GPIOC`. Sin esto, escribir en `MODER`/`ODR` no tiene efecto.
- [ ] El semáforo cambia demasiado rápido o demasiado lento → revisa el valor de `SysTick->LOAD`; si tu tarjeta no corre a 16 MHz por alguna configuración de reloj distinta, el cálculo cambia.
- [ ] Intentaste usar la variable `SystemCoreClock` y el proyecto no enlaza (*undefined reference*) → en este proyecto `system_stm32f4xx.c` no está compilado, así que esa variable no existe; usa el valor de reloj fijo (`8000000UL - 1UL` para 500 ms a 16 MHz) como se hizo aquí.
- [ ] `COUNTFLAG` nunca se pone en 1 → revisa que `SysTick->CTRL` tenga el bit `ENABLE` en 1 (usa `SysTick_CTRL_ENABLE_Msk`), y que `LOAD` no haya quedado en 0.
- [ ] `MODER` quedó en un valor raro → asegúrate de **limpiar antes de asignar** (`&= ~(...)` seguido de `|= (...)`), nunca solo `|=` directo sobre un registro que puede tener basura de otro modo.
- [ ] Olvidaste la macro `STM32F411xE` en `target_compile_definitions` → sin ella, `stm32f4xx.h` no sabe qué variante compilar y puede fallar la compilación o generar mapeos de memoria incorrectos.
