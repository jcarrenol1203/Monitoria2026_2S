# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Qué es este repositorio

Material del **Taller 1** de la Monitoría de Electrónica Digital 2026-2S: primer
acercamiento al debugger en el microcontrolador **STM32F411** (tarjeta
**NUCLEO-F411RE**) usando la extensión oficial de **STM32 para VS Code**. Es un
repositorio de contenido pedagógico, no de firmware ni de código de producción.

## Archivos y su relación

- `documento.md` — fuente de verdad del contenido del taller, en Markdown y en
  español. Cualquier cambio de fondo (nuevas secciones, ejemplos, correcciones)
  se hace primero aquí.
- El **Artifact** publicado (página web con pestañas/acordeón) es una
  presentación visual del mismo contenido de `documento.md`, pensada para
  compartir con los estudiantes. Al editar uno de los dos, el otro debe
  actualizarse para que no queden desincronizados.

## Alcance pedagógico del taller (no desviarse al agregar contenido)

- **Hardware**: NUCLEO-F411RE (STM32F411). **Toolchain**: extensión STM32 para
  VS Code, proyecto tipo **CMake**, STM32CubeCLT, servidor GDB de **ST-Link**
  (configuración de debug: `STM32Cube: STM32Launch STLink GDB Server`).
- **Nivel**: primer contacto con el debugger. No se usan periféricos (UART,
  GPIO/botones, timers) ni RTOS — eso queda para talleres posteriores.
- **Ejemplo integrador**: una calculadora con dos números y un `char` de
  operación **hardcodeados en el código** (sin entrada física ni por UART). El
  objetivo es practicar breakpoints, step y los paneles Watch/Locals, no leer
  periféricos. Las variables numéricas (`numero1`, `numero2`, `resultado`) son
  `uint8_t` — ejemplo concreto de la ventaja de los tipos de ancho fijo. No se
  usa `float` en este ejemplo (para no introducir la FPU del micro todavía);
  `dividir` hace división entera, y la rama de `/` valida `numero2 != 0` antes
  de operar, saltando la división en vez de ejecutarla entre 0.
- **Idioma**: todo el contenido (documento y artifact) va en español, la
  audiencia son estudiantes de la monitoría.
- **Tipos de variables orientados a firmware**: además de `int`/`unsigned int`/
  `float`/`char`/`double`, el taller cubre explícitamente los tipos de ancho
  fijo de `stdint.h` (`uint8_t`, `int8_t`, `uint16_t`, `int16_t`, `uint32_t`,
  `int32_t`) porque en firmware importa fijar el número de bits de cada
  variable. No quitar esta sección al editar el documento.
- **Estructuras de control**: `if` / `else if` / `while` / `for` se explican en
  una sección propia, separada de "Tipos de funciones" (que es solo para
  funciones personalizadas) y **antes** de ella — el usuario pidió este orden
  explícitamente porque le facilita la explicación. Incluye la explicación de
  por qué `while(1)` es infinito (en C, todo valor distinto de 0 es verdadero)
  y la diferencia entre `=` (asignación) y `==` (comparación), un error común
  de principiantes.

## Orden del contenido (documento.md y pestañas del Artifact)

El documento va en este orden — los tipos de variables, las estructuras de
control y los tipos de funciones se explican **antes** de la sección
"Estructura de un programa en C" (no después), porque son prerrequisito para
entenderla. Las estructuras de control van **antes** de los tipos de funciones
(y no después, como en una versión anterior del taller) porque eso facilita
la explicación en clase:

1. Crear un proyecto vacío
2. Tipos de variables (incluye `stdint.h`)
3. Estructuras de control (`if`/`else if`/`while`/`for`)
4. Tipos de funciones (funciones personalizadas)
5. Estructura de un programa en C
6. Ejemplo integrador: la calculadora
7. Depuración paso a paso

No se agrega una sección de cierre/resumen final — el usuario pidió quitarla
explícitamente; no volver a añadirla salvo que se solicite.

## Convención de estructura de código en los ejemplos C

Todo ejemplo en C que se agregue al taller debe seguir este orden, que es en sí
mismo uno de los puntos de enseñanza del documento (ver sección 5 de
`documento.md`):

1. Includes
2. Definición de variables (globales)
3. Cabeceras/prototipos de funciones
4. `main()` con su `while(1)`
5. Definición completa de las funciones

Justificación pedagógica: el código corre una sola vez de arriba hacia abajo, por
lo que una función debe estar prototipada antes de usarse en el `main`.
