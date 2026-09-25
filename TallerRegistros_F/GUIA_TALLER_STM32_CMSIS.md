# Taller: Registros CMSIS en el STM32F411RE — De "Hola Mundo" (PA5) al Pulsador B1 (PC13)

> **Asignatura:** Electrónica Digital
> **Tarjeta:** NUCLEO-F411RE (STM32F411RET6, ARM Cortex-M4)
> **Entorno:** VS Code + extensión oficial **STM32 for VS Code** (STMicroelectronics) + CMake
> **Enfoque:** Acceso directo a registros con **CMSIS** (sin capa HAL), siguiendo el **Manual de Referencia RM0383**

---

## Tabla de contenido

1. [Objetivos](#1-objetivos)
2. [Crear el proyecto en VS Code](#2-crear-el-proyecto-en-vs-code)
3. [Incluir las rutas de drivers CMSIS en `CMakeLists.txt`](#3-incluir-las-rutas-de-drivers-cmsis-en-cmakeliststxt)
4. [Cómo se manipulan bits en C](#4-cómo-se-manipulan-bits-en-c)
5. [Los registros que vamos a tocar (RM0383)](#5-los-registros-que-vamos-a-tocar-rm0383)
6. [Ejercicio 1 — "Hola Mundo": encender y titilar PA5 (LED LD2)](#6-ejercicio-1--hola-mundo-encender-y-titilar-pa5-led-ld2)
7. [Ejercicio 2 (si alcanza el tiempo) — Leer el pulsador B1 en PC13](#7-ejercicio-2-si-alcanza-el-tiempo--leer-el-pulsador-b1-en-pc13)
8. [Código fuente completo](#8-código-fuente-completo)
9. [Checklist / errores comunes](#9-checklist--errores-comunes)

---

## 1. Objetivos

1. Entender que cada periférico (GPIOA, GPIOC, RCC...) es en realidad una dirección fija de memoria, y que CMSIS solo nos da nombres en C (`GPIOA`, `RCC`) para esas direcciones.
2. Configurar un pin GPIO **a mano**, registro por registro, consultando el **Manual de Referencia RM0383**.
3. "Hola Mundo" del hardware: encender y hacer titilar el LED de usuario **LD2**, conectado al pin **PA5**.
4. Si el tiempo lo permite: leer el estado del pulsador de usuario **B1**, conectado al pin **PC13**, y usarlo para controlar el LED.

No se usa la librería HAL de ST en este taller: todo se hace escribiendo directamente sobre los registros que expone CMSIS.

---

## 2. Crear el proyecto en VS Code

1. Abre VS Code con la extensión **STM32 for VS Code** instalada.
2. `Ctrl + Shift + P` → busca y ejecuta **`STM32: Create a New Project`**.
3. Selecciona la tarjeta **NUCLEO-F411RE** (chip `STM32F411RET6`).
4. Confirma **CMake** como sistema de construcción. La extensión genera automáticamente:
   - `Src/` → aquí va `main.c`, el arranque (`startup_stm32f411xx.S`) y los stubs de syscalls.
   - `Inc/` → tus propios `.h` (queda vacío al inicio).
   - `CMakeLists.txt`, `cmake/`, el script de enlazado `stm32f411xe_flash.ld` y `project-description.json`.

En este punto el proyecto compila (genera un binario vacío), pero **todavía no reconoce** `RCC`, `GPIOA` ni `GPIOC` porque el compilador no sabe dónde están los encabezados de CMSIS. Eso se arregla en el siguiente paso.

---

## 3. Incluir las rutas de drivers CMSIS en `CMakeLists.txt`

CMSIS se compone de dos paquetes de encabezados que hay que indicarle al compilador por separado:

| Ruta | Contenido |
| :--- | :--- |
| `drivers/CMSIS/Include` | Núcleo genérico ARM Cortex-M4 (`core_cm4.h`, tipos, intrínsecos) |
| `drivers/CMSIS/Device/ST/STM32F4xx/Include` | Definiciones específicas de ST para la familia F4 (`stm32f4xx.h`, `stm32f411xe.h`): aquí están `RCC`, `GPIOA`, `GPIOC`, etc. |

### Paso a paso

1. Abre `CMakeLists.txt` en la raíz del proyecto.
2. Busca el bloque `target_include_directories(${PROJECT_NAME} PRIVATE ...)` y agrega las dos rutas anteriores (ruta absoluta a tu instalación de STM32Cube):

   ```cmake
   target_include_directories(${PROJECT_NAME} PRIVATE
     # Ruta 1: núcleo CMSIS genérico ARM Cortex-M
     "/home/mgz/STM32Cube/Repository/STM32Cube_FW_F4_V1.28.3/Drivers/CMSIS/Include"

     # Ruta 2: cabeceras CMSIS específicas de ST para STM32F4xx
     "/home/mgz/STM32Cube/Repository/STM32Cube_FW_F4_V1.28.3/Drivers/CMSIS/Device/ST/STM32F4xx/Include"

     # Ruta 3: cabeceras propias del proyecto
     "${CMAKE_CURRENT_SOURCE_DIR}/Inc"
   )
   ```

3. Busca `target_compile_definitions(${PROJECT_NAME} PRIVATE ...)` y agrega el símbolo del chip exacto. Esta macro es la que le dice a `stm32f4xx.h` qué variante del F4 estás usando (memoria, periféricos disponibles, vector de interrupciones):

   ```cmake
   target_compile_definitions(${PROJECT_NAME} PRIVATE
     STM32F411xE
   )
   ```

4. Guarda el archivo y reconfigura: `Ctrl + Shift + P` → **`CMake: Configure`**.
5. Verifica en `Src/main.c`: agrega `#include <stm32f4xx.h>` y comprueba que `RCC`, `GPIOA` y `GPIOC` ya no aparecen subrayados en rojo por IntelliSense.

> **Nota:** las rutas de arriba corresponden a una instalación local de `STM32Cube_FW_F4`. Si tu equipo instaló el paquete en otra carpeta, ajusta la ruta base, pero la estructura interna (`Drivers/CMSIS/Include` y `Drivers/CMSIS/Device/ST/STM32F4xx/Include`) es siempre la misma.

---

## 4. Cómo se manipulan bits en C

Los registros son de 32 bits, y casi siempre solo queremos tocar 1 o 2 bits sin alterar el resto. Para eso usamos máscaras:

| Operación | Operador | Qué hace | Ejemplo |
| :--- | :---: | :--- | :--- |
| **Set** (poner en 1) | `\|=` | Prende un bit, deja los demás igual | `RCC->AHB1ENR \|= (1 << 0);` |
| **Clear** (poner en 0) | `&= ~` | Apaga un bit (o un grupo), deja los demás igual | `GPIOA->MODER &= ~(0b11 << 10);` |
| **Toggle** (invertir) | `^=` | Invierte el bit actual | `GPIOA->ODR ^= (1 << 5);` |
| **Read** (leer) | `&` | Extrae el valor de un bit | `if (GPIOC->IDR & (1 << 13))` |

### Registros de 2 bits por pin (`MODER`, `OSPEEDR`, `PUPDR`)

Cada pin ocupa **2 bits**, así que el pin `N` empieza en el bit `N * 2`:

- Pin 5 → bits `[11:10]` → `5 * 2 = 10`
- Pin 13 → bits `[27:26]` → `13 * 2 = 26`

Para escribir un valor de 2 bits sin arrastrar basura, primero se limpia y luego se asigna:

```c
GPIOA->MODER &= ~(0b11 << (5 * 2)); // Limpia los 2 bits del pin 5
GPIOA->MODER |=  (0b01 << (5 * 2)); // Asigna 01 (salida) al pin 5
```

---

## 5. Los registros que vamos a tocar (RM0383)

### 5.1 Reloj de periféricos — `RCC->AHB1ENR`

En Cortex-M, **cada periférico empieza apagado** para ahorrar energía. Antes de tocar cualquier registro de un GPIO hay que activar su reloj en el bus AHB1.

- Base de `RCC`: `0x4002 3800` — registro `AHB1ENR`, offset `0x30` (RM0383, sección de relojes del capítulo RCC).

| Bit 3 | Bit 2 | Bit 1 | Bit 0 |
| :---: | :---: | :---: | :---: |
| `GPIODEN` | **`GPIOCEN`** (Puerto C — pulsador B1) | `GPIOBEN` | **`GPIOAEN`** (Puerto A — LED LD2) |

```c
RCC->AHB1ENR |= (1 << 0); // Reloj GPIOA
RCC->AHB1ENR |= (1 << 2); // Reloj GPIOC
```

### 5.2 Registros GPIO (capítulo GPIO de RM0383)

Cada puerto (`GPIOA`, `GPIOC`, ...) tiene su propio bloque de registros de 32 bits:

| Registro | Offset | Bits/pin | Función |
| :--- | :---: | :---: | :--- |
| `MODER` | `0x00` | 2 | Modo del pin: `00` entrada, `01` salida, `10` función alterna, `11` analógico |
| `OTYPER` | `0x04` | 1 | Tipo de salida: `0` push-pull, `1` open-drain |
| `OSPEEDR` | `0x08` | 2 | Velocidad de conmutación: `00` baja ... `11` muy alta |
| `PUPDR` | `0x0C` | 2 | Resistencia interna: `00` ninguna, `01` pull-up, `10` pull-down |
| `IDR` | `0x10` | 1 (solo lectura) | Valor lógico leído en el pin |
| `ODR` | `0x14` | 1 | Valor lógico a escribir en el pin |
| `BSRR` | `0x18` | 1 (set) + 1 (reset) | Escritura atómica: bits `[15:0]` ponen en 1, bits `[31:16]` ponen en 0 |

Direcciones base usadas en este taller:

- `GPIOA`: `0x4002 0000`
- `GPIOC`: `0x4002 0800`

---

## 6. Ejercicio 1 — "Hola Mundo": encender y titilar PA5 (LED LD2)

En la NUCLEO-F411RE, el LED verde **LD2** está soldado internamente al pin **PA5**.

### Paso a paso

1. **Habilitar el reloj del puerto A:**

   ```c
   RCC->AHB1ENR |= (1 << 0); // Enciende el reloj de GPIOA (bit 0)
   ```

2. **Configurar PA5 como salida** (`MODER5 = 01`):

   ```c
   GPIOA->MODER &= ~(0b11 << (5 * 2)); // Limpia bits 11:10
   GPIOA->MODER |=  (0b01 << (5 * 2)); // Modo salida
   ```

3. **Configurar el tipo de salida y la velocidad** (push-pull, baja velocidad, sin pull-up/down — la config. típica para manejar un LED):

   ```c
   GPIOA->OTYPER  &= ~(1 << 5);          // Push-Pull
   GPIOA->OSPEEDR &= ~(0b11 << (5 * 2)); // Baja velocidad
   GPIOA->PUPDR   &= ~(0b11 << (5 * 2)); // Sin resistencias internas
   ```

4. **Encender el LED** escribiendo en `ODR`:

   ```c
   GPIOA->ODR |= (1 << 5); // PA5 = 1 → 3.3V → LED encendido
   ```

5. **Titilar** con un `toggle` + retardo por software:

   ```c
   while (1) {
       GPIOA->ODR ^= (1 << 5);              // Invierte PA5
       for (volatile int i = 0; i < 500000; i++); // Retardo (bloqueante)
   }
   ```

> **Tip para explicar en clase:** en lugar de `ODR`, se puede usar `GPIOA->BSRR` para escrituras atómicas: `GPIOA->BSRR = (1 << 5);` enciende, y `GPIOA->BSRR = (1 << (5 + 16));` apaga. Es útil para mostrar por qué `ODR` puede tener condiciones de carrera en sistemas con interrupciones y `BSRR` no.

---

## 7. Ejercicio 2 (si alcanza el tiempo) — Leer el pulsador B1 en PC13

El pulsador azul **B1 USER** está conectado al pin **PC13**.

> **Lógica active-low:** B1 tiene una resistencia de pull-up **externa** en la placa.
> - Botón **suelto** → `PC13` lee **`1`** (3.3V).
> - Botón **presionado** → el circuito conecta a GND → `PC13` lee **`0`**.

### Paso a paso

1. **Habilitar el reloj de GPIOC:**

   ```c
   RCC->AHB1ENR |= (1 << 2); // Enciende el reloj de GPIOC (bit 2)
   ```

2. **Configurar PC13 como entrada** (`MODER13 = 00`, valor por defecto, pero se limpia explícitamente):

   ```c
   GPIOC->MODER &= ~(0b11 << (13 * 2)); // Bits 27:26 = 00 → Entrada
   GPIOC->PUPDR &= ~(0b11 << (13 * 2)); // Sin pull interna (ya tiene pull-up externa)
   ```

3. **Leer el registro `IDR`** y usarlo para controlar el LED de PA5:

   ```c
   if ((GPIOC->IDR & (1 << 13)) == 0) {
       GPIOA->ODR |= (1 << 5);   // Botón presionado → LED encendido
   } else {
       GPIOA->ODR &= ~(1 << 5);  // Botón suelto → LED apagado
   }
   ```

---

## 8. Código fuente completo

Reemplaza el contenido de `Src/main.c` con el siguiente código, que combina ambos ejercicios en un único `while(1)`:

```c
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Hola Mundo (PA5) y lectura de pulsador (PC13) con CMSIS
 * @board          : STM32F411RE (NUCLEO-F411RE)
 ******************************************************************************
 */

#include <stm32f4xx.h>
#include <stdint.h>

int main(void)
{
    /* PASO 1: Habilitar relojes de GPIOA y GPIOC (RCC->AHB1ENR) */
    RCC->AHB1ENR |= (1 << 0); // GPIOA (LED PA5)
    RCC->AHB1ENR |= (1 << 2); // GPIOC (pulsador PC13)

    /* PASO 2: PA5 como salida push-pull, baja velocidad, sin pull */
    GPIOA->MODER   &= ~(0b11 << (5 * 2));
    GPIOA->MODER   |=  (0b01 << (5 * 2));
    GPIOA->OTYPER  &= ~(1 << 5);
    GPIOA->OSPEEDR &= ~(0b11 << (5 * 2));
    GPIOA->PUPDR   &= ~(0b11 << (5 * 2));

    /* PASO 3: PC13 como entrada (pull-up externa ya presente en la placa) */
    GPIOC->MODER &= ~(0b11 << (13 * 2));
    GPIOC->PUPDR &= ~(0b11 << (13 * 2));

    while (1)
    {
        if ((GPIOC->IDR & (1 << 13)) == 0)
        {
            GPIOA->ODR |= (1 << 5);  // Botón presionado -> LED encendido
        }
        else
        {
            GPIOA->ODR &= ~(1 << 5); // Botón suelto -> LED apagado
        }
    }
}
```

Si solo hay tiempo para el Ejercicio 1, basta con dejar el `PASO 1` y `PASO 2`, y cambiar el `while(1)` por el bloque de titileo de la sección 6.

---

## 9. Checklist / errores comunes

- [ ] `#include <stm32f4xx.h>` marca error o no reconoce `RCC`/`GPIOA`/`GPIOC` → revisa las dos rutas de `target_include_directories` y corre `CMake: Configure` de nuevo.
- [ ] El proyecto compila pero el LED no prende → revisa que **primero** se habilitó el reloj en `RCC->AHB1ENR` antes de tocar `GPIOA`. Sin esto, escribir en `GPIOA->MODER`/`ODR` no tiene efecto.
- [ ] `MODER` quedó en un valor raro → asegúrate de **limpiar antes de asignar** (`&= ~(...)` seguido de `|= (...)`), nunca solo `|=` directo sobre un registro que puede tener basura de otro modo.
- [ ] Olvidaste la macro `STM32F411xE` en `target_compile_definitions` → sin ella, `stm32f4xx.h` no sabe qué variante compilar y puede fallar la compilación o generar mapeos de memoria incorrectos.
- [ ] El pulsador B1 "no responde" o queda invertido → recuerda que es **active-low**: presionado = `0`, suelto = `1`.
