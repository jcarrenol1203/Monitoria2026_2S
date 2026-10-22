# Taller PWM + Modo Encoder + EXTI (STM32F411 / NUCLEO-F411RE)

Monitoría de Electrónica Digital — 2026-2S

---

## 0. Objetivo

Generar una señal **PWM** en el pin PA8 (TIM1) y verla en el **osciloscopio** y en un **LED**, donde hay dos modos de edición:

- **Modo CCR** (el de arranque): al **girar el encoder KY040** cambia el **ancho del pulso** (`CCR1`), de 10 en 10 ticks.
- **Modo ARR**: al girar el encoder cambia el **periodo** (`ARR`), de 20 en 20 ticks.
- Al **presionar el pulsador** del encoder se pasa de un modo al otro.
- El LED de la tarjeta (**PH1**) muestra el modo: parpadea a **4 Hz** en modo CCR y a **2 Hz** en modo ARR.

Todo se programa a nivel de **registros con CMSIS**, sin HAL, igual que los talleres anteriores.

---

## 1. Montaje

| KY040 | Nucleo | Función |
|---|---|---|
| CLK | PA0 | TIM2_CH1 (AF1) |
| DT | PA1 | TIM2_CH2 (AF1) |
| SW | PB0 | EXTI0 (pulsador, activo en bajo) |
| + | 3V3 | Alimentación |
| GND | GND | Tierra |

| Salida | Pin | Conexión |
|---|---|---|
| PWM (TIM1_CH1, AF1) | PA8 | LED + resistencia de 330 Ω a GND, y/o osciloscopio |
| Blinky | PH1 | LED de la tarjeta (ya está conectado), indica el modo: 4 Hz = CCR, 2 Hz = ARR |

**Rebote del pulsador.** Este taller **no** filtra el rebote del pulsador (sección 4.1): si un click cambia el modo dos veces, es el rebote y no un error de código. Un condensador de 100 nF entre PB0 y GND lo reduce sin tocar el programa.

> **Nota:** Cable rojo a VCC (3V3) y negro a GND. Revisen la alimentación **antes** de energizar.

---

## 2. Qué es un PWM

Es una señal digital que se repite cada **periodo** T. En cada periodo la salida está en alto un tiempo y en bajo el resto. La fracción en alto es el **duty cycle**.

```
        |<------- T ------->|
   ____                      ____
  |    |                    |    |
__|    |____________________|    |______
  |<-t_on->|
  duty = t_on / T
```

El timer lo genera con tres registros:

| Registro | Qué hace |
|---|---|
| `PSC` | Divide el reloj del timer: `f_tick = 16 MHz / (PSC + 1)` |
| `ARR` | Define el periodo: `T = (ARR + 1) / f_tick` |
| `CCR1` | Define el ancho del pulso: `duty = CCR1 / (ARR + 1)` |

En **PWM modo 1** la salida está en alto mientras `CNT < CCR1` y en bajo cuando `CNT >= CCR1`.

Con `PSC = 15` el tick es de 1 µs, entonces **ARR y CCR1 se leen directamente en microsegundos**:

```
periodo     = (ARR + 1) µs        frecuencia = 1 MHz / (ARR + 1)
ancho pulso = CCR1 µs
```

| ARR | Periodo | Frecuencia |
|---|---|---|
| 1999 (inicial) | 2000 µs | 500 Hz (con `CCR1` inicial = 1000: duty de 50 %) |
| 999 | 1000 µs | 1 kHz |
| 99 (mínimo) | 100 µs | 10 kHz |

El código no usa tablas ni porcentajes: cada paso del encoder **suma o resta** al valor que se está editando.

| Modo | Cada paso del encoder | Límites |
|---|---|---|
| `MODE_CCR` | `CCR1 ± 10` | 0 (siempre bajo) a `ARR + 1` (siempre alto) |
| `MODE_ARR` | `ARR ± 20` | `ARR_MIN = 99` a `ARR_MAX = 65534` |

Si el ARR baja por debajo del CCR1, el CCR1 se recorta a `ARR + 1` para que la señal no se pierda.

> **Pregunta guía:** en `MODE_ARR` el ARR cambia pero el CCR1 no. ¿Qué le pasa al ancho del pulso y al duty en el osciloscopio? ¿Y qué pasa si el CCR1 queda mayor que `ARR + 1`?

> **Nota:** TIM1 es un timer **avanzado**: además de habilitar el canal (`CC1E`) hay que activar `MOE` en `BDTR`, si no la señal nunca sale al pin. Es de 16 bit, por eso `ARR_MAX = 65534`: así `ARR + 1`, el mayor valor del `CCR1`, también cabe en 16 bit.

---

## 3. Modo encoder

El KY040 tiene dos contactos (CLK y DT) que generan dos señales desfasadas 90° (cuadratura). El orden en que cambian indica el sentido de giro:

```
Giro horario (CW):         Giro antihorario (CCW):
CLK  __|‾‾|__|‾‾|__        CLK  ____|‾‾|__|‾‾|_
DT   ____|‾‾|__|‾‾|_       DT   __|‾‾|__|‾‾|____
     (CLK adelanta a DT)        (DT adelanta a CLK)
```

El TIM2 puede leer esas dos señales por sí solo: **no hay interrupciones ni código** para contar. El hardware cuenta hacia arriba o hacia abajo.

- `CC1S = 01` y `CC2S = 01`: los canales 1 y 2 son **entradas** conectadas a PA0 y PA1.
- `IC1F` / `IC2F`: filtro digital de entrada (ver la sección siguiente).
- `SMS = 011`: **encoder mode 3**, cuenta con los flancos de **ambos** canales, por eso se obtienen **4 cuentas por paso**. De ahí el `/ 4` en el código.
- `ARR = 0xFFFFFFFF`: TIM2 es de 32 bit. Si se gira hacia atrás desde 0, el contador pasa a `0xFFFFFFFF`. Visto como `int32_t` es **-1**: el contador se lee como un número **con signo** (positivo = horario, negativo = antihorario).

**Cómo se usa la cuenta en el código.** Un paso son 4 cuentas, entonces `pasos = CNT / 4` (división entera, con signo). `encoder_HasSteps()` solo consulta si hay al menos un paso; `encoder_TakeSteps()` devuelve los pasos y **descuenta** `pasos * 4` del contador, así `CNT` queda con el residuo (de -3 a 3 cuentas) y el siguiente giro lo suma. El contador ya no se satura: los límites los pone `pwm_ApplySteps()` sobre el ARR y el CCR1.

### 3.1 El filtro de entrada `ICxF`

Cada canal de captura del timer tiene un filtro digital. Toma muestras de la entrada y **solo acepta un cambio de nivel si N muestras seguidas coinciden**. Un pulso más corto que eso se descarta.

```
Entrada TIx : ___|‾|______|‾‾‾‾‾‾‾‾‾‾|___
                  ^ glitch (corto)  ^ pulso real
Salida filtro: __________________|‾‾‾‾‾‾‾‾‾‾|___   (el pulso real llega con retraso de N muestras)
```

Con `ICxF = 1111` (el máximo) el filtro muestrea a `f_DTS / 32` y exige `N = 8`. Con `CKD = 00`, `f_DTS` = 16 MHz:

```
f_SAMPLING = 16 MHz / 32 = 500 kHz   (una muestra cada 2 µs)
tiempo mínimo = 8 x 2 µs = 16 µs
```

| ICxF | f_SAMPLING | N | Pulso mínimo (16 MHz) |
|---|---|---|---|
| 0000 | sin filtro | – | 0 |
| 0011 | f_CK_INT | 8 | 500 ns |
| 0111 | f_DTS/4 | 8 | 2 µs |
| 1011 | f_DTS/16 | 6 | 6 µs |
| 1111 | f_DTS/32 | 8 | 16 µs |

> **Pregunta guía:** el rebote de un contacto mecánico dura entre 1 y 10 ms. ¿Un filtro de 16 µs lo elimina? ¿Entonces para qué sirve? ¿Y por qué el encoder funciona bien aunque sus contactos reboten?

---

## 4. El pulsador con EXTI

El pulsador es el mismo caso del botón **PC13** del taller de EXTI, pero en un pin que ustedes cablean al SW del KY040. En reposo el pin lee 1 (pull-up) y al presionar baja a 0, por eso se interrumpe en **flanco de bajada**.

| Elemento | PC13 (botón de la placa) | PB0 (SW del KY040) |
|---|---|---|
| Reloj | `GPIOCEN` | `GPIOBEN` |
| `PUPDR` | sin pull (la placa ya tiene pull-up) | Pull-Up (`01`) |
| `SYSCFG->EXTICR` | `EXTICR[3]`, `EXTI13_PC` | `EXTICR[0]`, `EXTI0_PB` |
| Bits de EXTI | `TR13`, `PR13`, `MR13` | `TR0`, `PR0`, `MR0` |
| NVIC | `EXTI15_10_IRQn` | `EXTI0_IRQn` |
| ISR | `EXTI15_10_IRQHandler` | `EXTI0_IRQHandler` |
| Rebote | Filtro RC en la placa | Sin filtro (sección 4.1) |

Orden de configuración (el mismo del taller de EXTI):

```
Reloj GPIOB -> PB0 entrada con Pull-Up -> reloj SYSCFG -> EXTICR -> FTSR/RTSR -> PR -> IMR -> NVIC
```

La ISR solo hace tres cosas: comprueba que fue la línea 0, limpia `PR` (escribiendo 1) y levanta la bandera `button_flag`. `get_next_event()` la convierte en el evento `EV_BUTTON_PRESS` y la FSM lo atiende (sección 5). **No hay esperas dentro de la ISR.**

> **Nota:** `EXTI->PR` y `TIM->SR` se limpian distinto: `PR` se limpia **escribiendo 1**; `SR` se limpia escribiendo 0 en el bit (`&= ~TIM_SR_UIF`).

### 4.1 Rebote del pulsador (no se filtra)

Un contacto mecánico rebota entre 1 y 10 ms, así que **un solo click puede generar varios flancos de bajada** y levantar `button_flag` varias veces:

```
PB0      ‾‾‾‾‾|_|‾|_|‾|__________________|‾‾‾‾‾‾   (un solo click)
flancos       1  2  3                      <- la ISR levanta button_flag 3 veces
```

Este taller deja el pulsador **sin antirrebote** para centrarse en el encoder y el PWM. Consecuencia: a veces un click cambia el modo dos veces (el blinky queda como estaba). No es un error del código; es el contacto. Un condensador de 100 nF entre PB0 y GND lo reduce, y en el ejercicio 5 se piensa cómo resolverlo por software.

---

## 5. Máquina de estados finitos (FSM)

Hasta ahora el programa se escribió como una lista de tareas sueltas. Una **FSM** ordena el programa en tres ideas (presentación *Finite State Machines in Embedded C*):

| Concepto | Qué es | En este taller |
|---|---|---|
| **Estado** | La condición del sistema en un instante | `STATE_IDLE`, `STATE_UPDATE_PWM`, `STATE_CHANGE_MODE`, `STATE_BLINK` |
| **Evento** | El disparador (ISR, timer, sensor) | `EV_ENCODER_CHANGED`, `EV_BUTTON_PRESS`, `EV_BLINK_TICK` |
| **Transición y acción** | El movimiento al siguiente estado y lo que se ejecuta | Cambiar el ARR o el CCR1, cambiar de modo, cambiar el LED |

> **Moore y Mealy:** en Moore la salida depende del **estado** (la acción se ejecuta al entrar a él). En Mealy depende de **estado + evento** (la acción va en la transición). Este taller usa Moore: la acción está en el `[ENTER]` de cada estado.

### 5.1 Paso 1: modelar antes de programar

Dibujen primero el diagrama y la tabla. El código solo los traduce.

```
                 EV_ENCODER_CHANGED
          +---------------------------------->  STATE_UPDATE_PWM
          |                                       [ENTER] pwm_ApplySteps()
          |   +------------------------------------+
          |   |  (incondicional)
          |   v
   +------------+  EV_BUTTON_PRESS   +-------------------+
   | STATE_IDLE | -----------------> | STATE_CHANGE_MODE |
   |  (espera)  | <----------------- | [ENTER] sig. modo |
   +------------+  (incondicional)   +-------------------+
          |   ^
          |   |  (incondicional)
          |   +------------------------------------+
          |  EV_BLINK_TICK                         |
          +---------------------------------->  STATE_BLINK
                                                [ENTER] led_Toggle()
```

| Estado actual | Evento | Estado siguiente | Acción `[ENTER]` |
|---|---|---|---|
| `STATE_IDLE` | `EV_ENCODER_CHANGED` | `STATE_UPDATE_PWM` | – |
| `STATE_IDLE` | `EV_BUTTON_PRESS` | `STATE_CHANGE_MODE` | – |
| `STATE_IDLE` | `EV_BLINK_TICK` | `STATE_BLINK` | – |
| `STATE_UPDATE_PWM` | (incondicional) | `STATE_IDLE` | `pwm_ApplySteps(encoder_TakeSteps())` |
| `STATE_CHANGE_MODE` | (incondicional) | `STATE_IDLE` | `mode_Toggle()` |
| `STATE_BLINK` | (incondicional) | `STATE_IDLE` | `led_Toggle()` |
| `STATE_ERROR` / `default` | (cualquiera) | `STATE_IDLE` | recuperación |

### 5.2 Paso 2: estados y eventos como `enum`

```c
typedef enum {
	STATE_IDLE, STATE_UPDATE_PWM, STATE_CHANGE_MODE, STATE_BLINK, STATE_ERROR,
	NUM_STATES		// centinela: cuántos estados hay
} State_t;

typedef enum {
	EV_NONE, EV_ENCODER_CHANGED, EV_BUTTON_PRESS, EV_BLINK_TICK,
	NUM_EVENTS		// centinela: cuántos eventos hay
} Event_t;
```

`typedef enum` evita asignar enteros cualquiera a un estado: el compilador avisa. `NUM_STATES` y `NUM_EVENTS` permiten validar rangos.

### 5.3 Paso 3: el ciclo infinito (patrón switch-case)

```c
while (1)
{
	current_event = get_next_event();     // 1. ¿pasó algo?
	switch (current_state)                // 2. ¿en qué estado estoy?
	{
		case STATE_IDLE:  /* ... */ break;  // 3. reacciono según el evento
		/* ... */
		default: current_state = STATE_IDLE; break;
	}
}
```

> **Regla de no bloqueo:** en la FSM **no hay `delay()` ni `for()` de espera**. Una espera detiene toda la máquina: no se leería el encoder ni el pulsador mientras tanto. Cada pasada del `while(1)` atiende a lo sumo **un** evento y **una** transición.

### 5.4 Entry, Do y Exit

```
[ENTER]  ->  [DO]  ->  [EXIT]
 una vez      libre     una vez
```

Se controlan con la bandera `state_entry_flag`: vale 1 al llegar a un estado, el `[ENTER]` la baja a 0 y así corre **una sola vez**, aunque el `while(1)` pase miles de veces por ese `case`.

```c
case STATE_BLINK:
	if (state_entry_flag)        // [ENTER] solo la primera pasada
	{
		led_Toggle();
		state_entry_flag = 0;
	}
	current_state = STATE_IDLE;  // transición
	state_entry_flag = 1;        // para que IDLE ejecute su [ENTER]
	break;
```

### 5.5 La FSM y las interrupciones

- La ISR **solo registra el evento y regresa**: baja la bandera del periférico y levanta una variable `volatile` (`button_flag`, `blink_flag`). **No hay lógica de la FSM dentro de la ISR.**
- `volatile` es obligatorio: la variable cambia fuera del flujo normal del programa y, sin `volatile`, el compilador podría dejarla guardada en un registro y no volver a leerla.
- `get_next_event()` lee y baja la bandera **dentro de una sección crítica** (`__disable_irq()` / `__enable_irq()`). Si la ISR llegara entre la lectura y el borrado, el evento se perdería.
- El encoder no genera interrupción: el TIM2 cuenta solo. `get_next_event()` pregunta con `encoder_HasSteps()` si ya hay un paso completo y, si lo hay, genera `EV_ENCODER_CHANGED`. Los pasos se toman después, en el `[ENTER]` de `STATE_UPDATE_PWM`.
- Prioridad cuando hay varios eventos a la vez: pulsador, encoder y blinky. Los que no se atienden quedan pendientes para la siguiente pasada.
- Cada fuente se evalúa con su propio `if` (no una cadena `if / else if`), así que ninguna tapa a las demás.
- Las entradas llevan Pull-Up interno (~40 kΩ, débil): usen un cable corto y, si hay ruido, un Pull-Up externo de 10 kΩ a 3V3 en PB0.
- **Los eventos solo se consumen en `STATE_IDLE`.** Los otros estados duran una sola pasada y no usan el evento. Si se leyera en ellos, una bandera quedaría borrada sin que nadie la atienda.

### 5.6 Trampas de hardware

| Trampa | Cómo se evita aquí |
|---|---|
| Estado corrupto (pila, ruido) | Antes del `switch` se valida `current_state < NUM_STATES` y `current_event < NUM_EVENTS`; si no, `STATE_ERROR`. Además, `default:` recupera a `STATE_IDLE` |
| Banderas de ISR sin `volatile` | `button_flag` y `blink_flag` son `volatile` |
| Mezclar el QUÉ con el CÓMO | La FSM decide **qué** pasa. Las funciones `pwm_ApplySteps`, `encoder_TakeSteps`, `mode_Toggle`, `led_Toggle` hacen **cómo** se toca el hardware |
| Variable de estado visible en todo el programa | `current_state` y `state_entry_flag` son `static` |

### 5.7 ¿Y la FSM con tabla?

Existe otra forma: en vez de `switch`, una tabla `const State_t transition_table[NUM_STATES][NUM_EVENTS]` y una sola línea `current_state = transition_table[current_state][current_event];`. Es más rápida (tiempo constante) y sirve para máquinas grandes (protocolos), pero gasta más memoria y las acciones se escriben aparte.

| Patrón | Complejidad | Memoria | Velocidad | Cuándo |
|---|---|---|---|---|
| Switch-case | Baja a media | Baja | O(N) | Menús simples, **menos de 10 estados** |
| Table-driven | Media | Alta (matriz en ROM) | O(1) | Parsers de protocolos, muchos estados |

Este taller tiene 5 estados, por eso usa **switch-case**. Armar la tabla de este taller es el ejercicio 6.

### 5.8 Proceso para cualquier FSM

```
1. MODELAR   -> diagrama y tabla estado x evento (papel)
2. MAPEAR    -> ¿cada estado tiene respuesta para cada evento?
3. ASIGNAR   -> typedef enum + variable de estado
4. EJECUTAR  -> while(1) + switch, sin bloquear
```

---

## 6. Crear el proyecto desde cero

1. En VS Code: `Ctrl+Shift+P` → **STM32: Create a New Project**.
2. Elegir la placa **NUCLEO-F411RE**.
3. Elegir **Empty** (proyecto vacío) y como sistema de build **CMake**. Nombre del proyecto: `Taller_PWM_Encoder`.
4. Al terminar debe quedar esta estructura:

```
Taller_PWM_Encoder/
  CMakeLists.txt  CMakePresets.json  project-description.json
  stm32f411xe_flash.ld  .clangd  .gitignore
  .settings/  .vscode/
  cmake/{components,files,flags,gcc-arm-none-eabi,target}.cmake
  Inc/                       (vacío)
  Src/{main.c, startup_stm32f411xx.S, syscall.c, sysmem.c}
```

5. Reemplacen **todo el contenido** de `Src/main.c` por el código de la sección 7.
6. Abrir `CMakeLists.txt` y agregar las rutas de los drivers **CMSIS** y la definición del micro. Ajusten el usuario y la versión del repositorio de STM32Cube (es lo único que cambia entre computadores):

```cmake
target_include_directories(${PROJECT_NAME} PRIVATE
"/home/USUARIO/STM32Cube/Repository/STM32Cube_FW_F4_V1.28.3/Drivers/CMSIS/Include"
"/home/USUARIO/STM32Cube/Repository/STM32Cube_FW_F4_V1.28.3/Drivers/CMSIS/Device/ST/STM32F4xx/Include"

  # User defined include directories
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
STM32F411xE
  # Add additional defined symbols here
)
```

7. En `main.c` se incluye:

```c
#include <stdint.h>
#include <stm32f4xx.h>
```

8. **No hay `SystemClock_Config`**: el micro arranca con el reloj interno **HSI de 16 MHz** y los timers reciben esos 16 MHz. Todas las cuentas (PSC, ARR) suponen 16 MHz.
9. Compilar (**Build**) y cargar (**Run/Debug**).

> **Nota:** Si el compilador no encuentra `stm32f4xx.h`, la ruta del `target_include_directories` está mal. Si no encuentra `TIM1` o los bits, falta `STM32F411xE`.

**Checklist antes de preguntar:**

- [ ] Compila sin warnings.
- [ ] Dibujaron el diagrama de estados y la tabla **antes** de programar.
- [ ] PH1 parpadea a 4 Hz al arrancar (si no, el programa no arranca).
- [ ] El pulsador del KY040 (SW) está cableado a PB0 y GND; con él PH1 pasa a 2 Hz y vuelve a 4 Hz.
- [ ] El LED de PA8 tiene su resistencia.
- [ ] CLK en PA0, DT en PA1 y GND común.

---

## 7. El código completo

```c
/**
 ******************************************************************************
 * @file           : main.c
 * @author         : Miguel A. Bedoya Gonzalez --> mibedoyag@unal.edu.co
 * @brief          : Taller PWM - Encoder KY040 (TIM2) cambia el ARR o el CCR1 del
 *                   PWM (TIM1) y el pulsador (EXTI0) elige cuál de los dos,
 *                   organizado como una Máquina de Estados Finitos (FSM)
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 *
 *** Mapeo de Pines:
 ** PWM TIM1 - AF01:
 * - CH1 (PWM)  -> PA8
 *
 ** ENCODER TIM2 - AF01:
 * - CH1 (KY040: CLK) -> PA0
 * - CH2 (KY040: DT)  -> PA1
 *
 ** PULSADOR KY040 (SW), activo en bajo, sin antirrebote:
 * - SW -> PB0 (EXTI0)
 *
 ** BLINKY TIM4 -> PH1 (4 Hz en modo CCR, 2 Hz en modo ARR)
 *
 *
 * ===================== FUNCIONAMIENTO ===================== *
 *
 * - TIM2 en modo encoder cuenta los flancos de CLK y DT (4 cuentas por paso).
 *   Cada paso del encoder sube o baja en una cantidad fija el valor que se esté
 *   editando.
 *
 * - TIM1 genera la señal PWM en PA8. Con PSC = 15 cada tick dura 1 us:
 *   periodo = (ARR + 1) us, ancho del pulso = CCR1 us. Arranca con ARR = 1999 y
 *   CCR1 = 1000: 500 Hz con duty de 50 %.
 *
 * - Hay dos modos de edición y cada flanco de bajada del pulsador cambia de uno a otro:
 *   MODE_CCR (inicial): cada paso del encoder suma o resta 10 al CCR1 (ancho del pulso).
 *   MODE_ARR          : cada paso del encoder suma o resta 20 al ARR (periodo).
 *   El ARR se satura entre 99 y 65534, el CCR1 entre 0 y ARR + 1.
 *
 * - TIM4 es la base de tiempo: interrumpe cada 1 ms y cuenta milisegundos. El LED PH1 muestra
 *   el modo: parpadea a 4 Hz (cambia cada 125 ms) en MODE_CCR y a 2 Hz (cambia cada 250 ms) en MODE_ARR.
 *
 * - Reloj del sistema: HSI 16 MHz (sin PLL), los timers reciben 16 MHz.
 *
 *
 * ===================== FSM (presentación 05-FSM) ===================== *
 *
 * Patrón 1: Switch-Case. La FSM decide QUÉ pasa (política), las funciones
 * driver hacen el CÓMO (hardware). Las ISR solo levantan banderas volatile,
 * get_next_event() las convierte en un EVENTO y el while(1) nunca se bloquea.
 *
 *   Estado actual      Evento               Estado siguiente    Acción [ENTER]
 *   -----------------  -------------------  ------------------  --------------------------
 *   STATE_IDLE         EV_ENCODER_CHANGED   STATE_UPDATE_PWM    -
 *   STATE_IDLE         EV_BUTTON_PRESS      STATE_CHANGE_MODE   -
 *   STATE_IDLE         EV_BLINK_TICK        STATE_BLINK         -
 *   STATE_UPDATE_PWM   (incondicional)      STATE_IDLE          pwm_ApplySteps(encoder_TakeSteps())
 *   STATE_CHANGE_MODE  (incondicional)      STATE_IDLE          mode_Toggle()
 *   STATE_BLINK        (incondicional)      STATE_IDLE          led_Toggle()
 *   STATE_ERROR        (cualquiera)         STATE_IDLE          recuperación
 *
 ******************************************************************************
 */

#include <stdint.h> //Tipos de ancho fijo (uint8_t, int32_t...) para que el tamaño de cada variable sea conocido
#include <stm32f4xx.h> //CMSIS del STM32F4: define los registros (RCC, GPIOx, TIMx, EXTI...) y sus máscaras de bits

/* Constantes */
#define COUNTS_PER_STEP	4 //El modo TI12 cuenta 4 flancos por cada paso mecánico del encoder
#define ARR_INIT		1999 //ARR inicial: (1999 + 1) x 1 us = 2000 us -> 500 Hz
#define CCR_INIT		((ARR_INIT + 1) / 2) //CCR1 inicial: la mitad del periodo -> 1000 us de 2000 us -> duty de 50 %
#define ARR_STEP		20 //Lo que suma o resta al ARR cada paso del encoder en MODE_ARR
#define ARR_MIN			99 //ARR mínimo: (99 + 1) x 1 us = 100 us -> 10 kHz (con ARR = 0 el contador queda bloqueado)
#define ARR_MAX			65534 //ARR máximo: periodo de unos 65 ms. TIM1 es de 16 bit y ARR + 1 (el CCR1 máximo) debe caber en 16 bit: con 65535 daría 65536, que se trunca a 0
#define CCR_STEP		10 //Lo que suma o resta al CCR1 cada paso del encoder en MODE_CCR
#define BLINK_CCR_MS	125 //PH1 cambia cada 125 ms en MODE_CCR: periodo de 250 ms -> 4 Hz
#define BLINK_ARR_MS	250 //PH1 cambia cada 250 ms en MODE_ARR: periodo de 500 ms -> 2 Hz

/* Máquina de estados finitos (FSM) */

/* Paso 1 (FSM): mapear el vocabulario a memoria. Estados y eventos son enum (type safety)
 * y NUM_STATES / NUM_EVENTS son centinelas para validar rangos. */

//MODOS DE EDICIÓN: qué valor del PWM modifica el encoder
typedef enum { //Lista de modos posibles
	MODE_CCR, //El encoder cambia el CCR1 (ancho del pulso), blinky a 4 Hz
	MODE_ARR //El encoder cambia el ARR (periodo), blinky a 2 Hz
} Mode_t; //Nombre del tipo: edit_mode solo puede recibir valores de este enum

//ESTADOS: la condición en la que está el sistema en un instante dado
typedef enum { //Lista de estados posibles, cada nombre vale un entero (0, 1, 2...)
	STATE_IDLE, //Reposo: espera un evento sin hacer nada más (no bloquea)
	STATE_UPDATE_PWM, //Atiende el giro del encoder: suma o resta al ARR o al CCR1 del PWM según el modo
	STATE_CHANGE_MODE, //Atiende el pulsador: cambia entre editar el CCR y editar el ARR
	STATE_BLINK, //Atiende el tick del TIM4: cambia el estado del LED PH1
	STATE_ERROR, //Estado inválido (trampa de hardware): se recupera hacia STATE_IDLE
	NUM_STATES //Centinela: cantidad de estados, sirve para validar el rango
} State_t; //Nombre del tipo: current_state solo puede recibir valores de este enum

//EVENTOS: los disparadores (ISR o lecturas) que provocan transiciones entre estados
typedef enum { //Lista de eventos posibles, cada nombre vale un entero (0, 1, 2...)
	EV_NONE, //No ha ocurrido nada
	EV_ENCODER_CHANGED, //El encoder avanzó o retrocedió al menos un paso
	EV_BUTTON_PRESS, //El pulsador del KY040 fue presionado (flanco de bajada)
	EV_BLINK_TICK, //Venció el medio periodo del blinky (125 ms en MODE_CCR, 250 ms en MODE_ARR)
	NUM_EVENTS //Centinela: cantidad de eventos, sirve para validar el rango
} Event_t; //Nombre del tipo: current_event solo puede recibir valores de este enum

/* Variables */

//Estado actual de la FSM, static: solo se accede desde este módulo (scope mínimo) y solo se modifica en main()
static State_t current_state = STATE_IDLE; //Siempre inicia en reposo
//Bandera de ENTRY: vale 1 al llegar a un estado para que su acción [ENTER] se ejecute exactamente una vez
static uint8_t state_entry_flag = 1; //Inicia en 1 para que el primer estado también ejecute su [ENTER]

volatile uint8_t button_flag = 0; //Bandera que deja la ISR del pulsador (volatile: cambia fuera del flujo normal del programa)
volatile uint8_t blink_flag = 0; //Bandera que deja la ISR del TIM4 (volatile por la misma razón)
volatile uint16_t blink_half_period_ms = BLINK_CCR_MS; //Milisegundos entre dos cambios del LED: la ISR del TIM4 lo lee y el main lo cambia (volatile)

static Mode_t edit_mode = MODE_CCR; //Modo de edición actual, inicia editando el CCR
uint32_t arr_value = ARR_INIT; //ARR que tiene el TIM1 (periodo = ARR + 1 ticks de 1 us)
uint32_t ccr_value = CCR_INIT; //CCR1 que tiene el TIM1 (ancho del pulso en ticks de 1 us), inicia en la mitad del periodo: duty de 50 %

/* Definición de funciones */

//Drivers de la FSM (el CÓMO)
static Event_t get_next_event(void); //Lee (y limpia) las banderas de las ISR y la lectura del encoder, y las convierte en un evento
static uint8_t encoder_HasSteps(void); //Devuelve 1 si el TIM2 acumuló al menos un paso completo (no modifica nada)
static int32_t encoder_TakeSteps(void); //Devuelve los pasos acumulados (con signo) y los descuenta del TIM2
static void pwm_ApplySteps(int32_t steps); //Suma steps x ARR_STEP al ARR o steps x CCR_STEP al CCR1 según el modo, con saturación
static void mode_Toggle(void); //Cambia entre MODE_CCR y MODE_ARR y ajusta la velocidad del blinky
static void led_Toggle(void); //Cambia el estado del LED PH1

//Inicialización del hardware
static void gpio_Init(void); //GPIOH PH1 como salida (blinky)
static void tim4_Init(void); //TIM4 con interrupción cada 1 ms (base de tiempo del blinky)
static void tim1_pwm_Init(void); //GPIOA PA8 + TIM1 CH1 en modo PWM 1
static void tim2_encoder_Init(void); //GPIOA PA0, PA1 + TIM2 en modo encoder
static void exti0_Init(void); //GPIOB PB0 + EXTI0 (flanco de bajada)

/* Main */
int main(void) //Punto de entrada: el startup lo llama después de copiar .data y limpiar .bss
{
	Event_t current_event = EV_NONE; //Evento que se evalúa en cada pasada del ciclo infinito

	gpio_Init(); //Configura PH1 (blinky)
	tim1_pwm_Init(); //Configura el PWM en PA8
	tim2_encoder_Init(); //Configura el encoder
	exti0_Init(); //Configura el pulsador por EXTI0 en PB0
	tim4_Init(); //Último: las interrupciones del blinky empiezan con todo el hardware listo

	/* Patrón 1 (FSM): Switch-Case. El ciclo infinito nunca termina: lee un evento, evalúa el estado y reacciona.
	 * Regla de NO bloqueo: no hay delay(), cada pasada procesa a lo sumo UN evento y UNA transición. */
	while (1) //El firmware nunca termina, gira todo el tiempo evaluando eventos
	{
		if (current_state == STATE_IDLE) //Solo en reposo se consumen eventos: los otros estados duran una pasada y no usan el evento
		{
			current_event = get_next_event(); //Consulta (sin bloquear) si hay un evento pendiente
		}
		else //En un estado de atención no se toca ninguna bandera
		{
			current_event = EV_NONE; //Si se consumiera aquí, el evento se perdería sin que nadie lo atienda
		}

		//Safeguard: valida estado y evento antes de usarlos, si alguno está fuera de rango se recupera con STATE_ERROR
		if ((current_state >= NUM_STATES) || (current_event >= NUM_EVENTS)) //Un valor fuera de rango sólo pasa por corrupción de memoria
		{
			current_state = STATE_ERROR; //Se fuerza el estado de error para recuperarse de forma segura
			state_entry_flag = 1; //El estado de error también ejecuta su [ENTER]
		}

		switch (current_state) //Despacha al código del estado actual
		{

		case STATE_IDLE: //Estado de reposo
			//[ENTER] Corre una sola vez al llegar al estado (aquí no hay nada que preparar)
			if (state_entry_flag) //Solo es 1 en la primera pasada después de llegar al estado
			{
				state_entry_flag = 0; //Se baja para que [ENTER] no vuelva a ejecutarse mientras se siga en este estado
			}

			//[DO] Corre libremente en cada pasada: en reposo no hace nada, solo espera un evento sin bloquear

			//Transiciones: cada evento lleva a un estado distinto
			if (current_event == EV_ENCODER_CHANGED) //Se giró el encoder
			{
				current_state = STATE_UPDATE_PWM; //[EXIT] No hay acción de salida en este estado
				state_entry_flag = 1; //Se marca para que el estado nuevo ejecute su [ENTER]
			}
			else if (current_event == EV_BUTTON_PRESS) //Se presionó el pulsador
			{
				current_state = STATE_CHANGE_MODE; //Pasa a atender el cambio de modo
				state_entry_flag = 1; //Se marca para que el estado nuevo ejecute su [ENTER]
			}
			else if (current_event == EV_BLINK_TICK) //Venció el medio periodo del blinky
			{
				current_state = STATE_BLINK; //Pasa a cambiar el LED
				state_entry_flag = 1; //Se marca para que el estado nuevo ejecute su [ENTER]
			}
			break; //Sale del switch, la pasada termina y el while(1) vuelve a empezar

		case STATE_UPDATE_PWM: //Estado que atiende el giro del encoder
			//[ENTER] Acción de entrada (Moore): cambia el ARR o el CCR1, exactamente una vez por cada evento del encoder
			if (state_entry_flag) //Solo la primera pasada después de llegar a este estado
			{
				pwm_ApplySteps(encoder_TakeSteps()); //Toma los pasos del encoder y los aplica al valor que indica edit_mode
				state_entry_flag = 0; //Baja la bandera: la acción ya se ejecutó
			}
			//Transición incondicional: la tarea ya fue atendida, se regresa al reposo
			current_state = STATE_IDLE; //Vuelve a esperar eventos
			state_entry_flag = 1; //IDLE ejecutará su [ENTER]
			break; //Fin del estado

		case STATE_CHANGE_MODE: //Estado que atiende el pulsador
			//[ENTER] Acción de entrada (Moore): cambia de modo, exactamente una vez por cada pulsación
			if (state_entry_flag) //Solo la primera pasada después de llegar a este estado
			{
				mode_Toggle(); //MODE_CCR <-> MODE_ARR y velocidad del blinky (4 Hz o 2 Hz)
				state_entry_flag = 0; //Baja la bandera: la acción ya se ejecutó
			}
			//Transición incondicional: la tarea ya fue atendida, se regresa al reposo
			current_state = STATE_IDLE; //Vuelve a esperar eventos
			state_entry_flag = 1; //IDLE ejecutará su [ENTER]
			break; //Fin del estado

		case STATE_BLINK: //Estado que atiende el tick del blinky
			//[ENTER] Acción de entrada (Moore): cambia el LED, exactamente una vez por cada evento del blinky
			if (state_entry_flag) //Solo la primera pasada después de llegar a este estado
			{
				led_Toggle(); //Invierte el estado de PH1
				state_entry_flag = 0; //Baja la bandera: la acción ya se ejecutó
			}
			//Transición incondicional: la tarea ya fue atendida, se regresa al reposo
			current_state = STATE_IDLE; //Vuelve a esperar eventos
			state_entry_flag = 1; //IDLE ejecutará su [ENTER]
			break; //Fin del estado

		case STATE_ERROR: //STATE_ERROR comparte código con default porque la recuperación es la misma
		default: //Trampa (default:): cualquier estado desconocido se recupera de forma segura hacia el reposo
			current_state = STATE_IDLE; //Reinicia la FSM en un estado conocido
			state_entry_flag = 1; //IDLE ejecutará su [ENTER]
			break; //Fin del estado
		}
	}
}

/* Funciones driver de la FSM (el CÓMO) */

/*
 * get_next_event
 * Convierte las banderas que dejan las ISR (y el avance del encoder) en UN evento.
 * Prioridad: pulsador > encoder > blinky. Cada fuente se evalúa con su propio if.
 * Las banderas que no se atienden en esta llamada quedan pendientes para la siguiente pasada.
 */
static Event_t get_next_event(void)
{
	Event_t ev = EV_NONE; //Si nada ocurrió, se devuelve EV_NONE

	__disable_irq(); //Sección crítica: leer y limpiar una bandera no puede ser interrumpido, o el evento podría perderse
	if (button_flag == 1) //La ISR del pulsador dejó un evento pendiente
	{
		button_flag = 0; //Se baja la bandera: ya se atendió el evento
		ev = EV_BUTTON_PRESS; //Evento más prioritario
	}

	if ((ev == EV_NONE) && encoder_HasSteps()) //El encoder no genera interrupción: el TIM2 cuenta solo y aquí se consulta si ya hay un paso completo
	{
		ev = EV_ENCODER_CHANGED; //Los pasos se toman en el [ENTER] de STATE_UPDATE_PWM
	}

	if ((ev == EV_NONE) && (blink_flag == 1)) //Sin otro evento: la ISR del TIM4 dejó el evento del blinky pendiente
	{
		blink_flag = 0; //Se baja la bandera: ya se atendió el evento
		ev = EV_BLINK_TICK; //Evento menos prioritario
	}
	__enable_irq(); //Fin de la sección crítica: las ISR pueden volver a ejecutarse

	return ev; //Devuelve el evento a la FSM
}

/*
 * encoder_HasSteps
 * Devuelve 1 si el TIM2 acumuló al menos un paso completo (4 cuentas) hacia cualquiera de los dos lados.
 * Solo consulta, no modifica el contador.
 */
static uint8_t encoder_HasSteps(void)
{
	int32_t raw_count = (int32_t) TIM2->CNT; //CNT es de 32 bit sin signo: al girar en CCW desde 0 pasa a 0xFFFFFFFF, que con signo es -1

	return (uint8_t) ((raw_count >= COUNTS_PER_STEP) || (raw_count <= -COUNTS_PER_STEP)); //1 si hay un paso o más, CW (positivo) o CCW (negativo)
}

/*
 * encoder_TakeSteps
 * Devuelve los pasos completos acumulados (positivo = CW, negativo = CCW) y los descuenta del TIM2.
 * Las cuentas que no completan un paso (-3 a 3) se quedan en CNT para sumarse al siguiente giro.
 */
static int32_t encoder_TakeSteps(void)
{
	int32_t raw_count = (int32_t) TIM2->CNT; //Cuentas acumuladas con signo
	int32_t steps = raw_count / COUNTS_PER_STEP; //La división entera trunca hacia 0: 7 cuentas = 1 paso, -7 cuentas = -1 paso

	TIM2->CNT = (uint32_t) (raw_count - (steps * COUNTS_PER_STEP)); //Se descuentan solo las cuentas que se usaron, queda el residuo

	return steps; //Pasos con signo
}

/*
 * pwm_ApplySteps
 * MODE_CCR: CCR1 += steps x CCR_STEP, saturado entre 0 y ARR + 1.
 * MODE_ARR: ARR  += steps x ARR_STEP, saturado entre ARR_MIN y ARR_MAX.
 * Si el ARR baja por debajo del CCR1, el CCR1 se recorta a ARR + 1 (salida siempre en alto).
 * Con ARPE = 1 y OC1PE = 1 los valores nuevos entran al terminar el periodo actual.
 */
static void pwm_ApplySteps(int32_t steps)
{
	int32_t new_arr = (int32_t) arr_value; //Se calcula con signo para poder detectar que se pasó de los límites
	int32_t new_ccr = (int32_t) ccr_value; //Igual que el ARR

	if (edit_mode == MODE_CCR) //El encoder edita el ancho del pulso
	{
		new_ccr += steps * CCR_STEP; //Cada paso suma o resta CCR_STEP ticks
	}
	else //El encoder edita el periodo
	{
		new_arr += steps * ARR_STEP; //Cada paso suma o resta ARR_STEP ticks
	}

	if (new_arr < ARR_MIN) //Validación: el ARR nunca baja del mínimo
	{
		new_arr = ARR_MIN; //Se satura en el mínimo
	}
	else if (new_arr > ARR_MAX) //Validación: el ARR nunca pasa del máximo
	{
		new_arr = ARR_MAX; //Se satura en el máximo
	}

	if (new_ccr < 0) //El CCR1 nunca es negativo
	{
		new_ccr = 0; //Se satura en 0: salida siempre en bajo
	}
	else if (new_ccr > (new_arr + 1)) //El CCR1 no puede pasar de ARR + 1 (el contador cuenta de 0 a ARR)
	{
		new_ccr = new_arr + 1; //Se satura en ARR + 1: salida siempre en alto
	}

	arr_value = (uint32_t) new_arr; //Guarda los valores ya validados
	ccr_value = (uint32_t) new_ccr;

	TIM1->ARR = arr_value; //Escribe el periodo nuevo: (ARR + 1) x 1 us
	TIM1->CCR1 = ccr_value; //Escribe el ancho del pulso nuevo: CCR1 x 1 us
}

/*
 * mode_Toggle
 * Cambia el valor que edita el encoder y muestra el modo con la velocidad del blinky:
 * MODE_CCR -> 4 Hz (cambia cada 125 ms), MODE_ARR -> 2 Hz (cambia cada 250 ms).
 */
static void mode_Toggle(void)
{
	if (edit_mode == MODE_CCR) //Estaba editando el CCR
	{
		edit_mode = MODE_ARR; //Pasa a editar el ARR
		blink_half_period_ms = BLINK_ARR_MS; //Blinky a 2 Hz
	}
	else //Estaba editando el ARR
	{
		edit_mode = MODE_CCR; //Pasa a editar el CCR
		blink_half_period_ms = BLINK_CCR_MS; //Blinky a 4 Hz
	}
}

/*
 * led_Toggle
 * Cambia el estado del LED de la tarjeta (PH1).
 */
static void led_Toggle(void)
{
	GPIOH->ODR ^= GPIO_ODR_OD1; //XOR con 1: invierte el bit sin tocar los otros pines del puerto
}

/* Funciones de inicialización del hardware */

/*
 * gpio_Init
 * PH1 como salida push-pull, LED de la tarjeta (blinky)
 */
static void gpio_Init(void)
{
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOHEN; //Activa la señal de reloj del GPIOH (bus AHB1), sin reloj el puerto ignora las escrituras

	GPIOH->MODER &= ~GPIO_MODER_MODER1; //Limpia los 2 bits de modo del pin 1 antes de asignar
	GPIOH->MODER |= GPIO_MODER_MODER1_0; //01: salida de propósito general
	GPIOH->OTYPER &= ~GPIO_OTYPER_OT1; //0: push-pull (el pin maneja alto y bajo)
	GPIOH->OSPEEDR &= ~GPIO_OSPEEDER_OSPEEDR1; //00: baja velocidad, suficiente para un LED
	GPIOH->PUPDR &= ~GPIO_PUPDR_PUPDR1; //00: sin pull-up ni pull-down, el pin es salida
	GPIOH->ODR &= ~GPIO_ODR_OD1; //El LED inicia apagado
}

/*
 * tim4_Init
 * Evento de actualización cada 1 ms: base de tiempo del blinky
 * PSC = 15 -> tick de 1 us, ARR = 999 -> (999 + 1) x 1 us = 1 ms
 * ARR nunca puede ser 0: con ARR = 0 el contador queda bloqueado y no hay interrupciones
 */
static void tim4_Init(void)
{
	RCC->APB1ENR |= RCC_APB1ENR_TIM4EN; //Activa la señal de reloj del TIM4 (bus APB1)

	TIM4->PSC = 15; //16 MHz / (15 + 1) = 1 MHz -> cada tick dura 1 us
	TIM4->ARR = 999; //El contador cuenta de 0 a 999 -> 1000 us = 1 ms por interrupción (ARR = 0 bloquearía el contador)
	TIM4->CNT = 0; //Reinicia el contador en 0

	TIM4->EGR |= TIM_EGR_UG; //Fuerza un evento de actualización: el PSC solo se carga a su registro activo en un update
	TIM4->SR &= ~TIM_SR_UIF; //El UG anterior levanta UIF, se limpia para que no entre una ISR falsa al habilitar el NVIC

	TIM4->DIER |= TIM_DIER_UIE; //Habilita la interrupción por actualización (primer interruptor)
	__NVIC_EnableIRQ(TIM4_IRQn); //Habilita la interrupción del TIM4 en el NVIC (segundo interruptor)

	TIM4->CR1 |= TIM_CR1_CEN; //Arranca el contador, siempre de último con todo ya configurado
}

/*
 * tim1_pwm_Init
 * PA8 -> TIM1_CH1 (AF1), PWM modo 1, 500 Hz, CCR1 = 1000 (duty de 50 %)
 * Clock TIM1: 16 MHz (APB2 sin división). PSC = 15 -> tick de 1 us
 * Periodo = (ARR + 1) x 1 us = 2000 us -> 500 Hz
 */
static void tim1_pwm_Init(void)
{
	/* GPIOA PA8 como función alterna AF1 */
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN; //Activa la señal de reloj del GPIOA (bus AHB1)

	GPIOA->MODER &= ~GPIO_MODER_MODER8; //Limpia los 2 bits de modo del pin 8
	GPIOA->MODER |= GPIO_MODER_MODER8_1; //10: función alterna, el pin lo controla un periférico (el TIM1)

	GPIOA->OTYPER &= ~GPIO_OTYPER_OT8; //0: push-pull, el timer fuerza alto y bajo

	GPIOA->OSPEEDR &= ~GPIO_OSPEEDER_OSPEEDR8; //Limpia los 2 bits de velocidad del pin 8
	GPIOA->OSPEEDR |= GPIO_OSPEEDER_OSPEEDR8_1; //10: alta velocidad, flancos más limpios en el osciloscopio

	GPIOA->PUPDR &= ~GPIO_PUPDR_PUPDR8; //00: sin pull-up ni pull-down, el timer maneja el nivel

	GPIOA->AFR[1] &= ~GPIO_AFRH_AFSEL8; //Limpia los 4 bits de función alterna del pin 8 (los pines 8-15 están en AFR[1] o AFRH)
	GPIOA->AFR[1] |= GPIO_AFRH_AFSEL8_0; //0001: AF1, conecta PA8 con TIM1_CH1

	/* Base de tiempo TIM1 */
	RCC->APB2ENR |= RCC_APB2ENR_TIM1EN; //Activa la señal de reloj del TIM1 (bus APB2, los TIM2 a TIM5 están en APB1)

	TIM1->PSC = 15; //16 MHz / (15 + 1) = 1 MHz -> cada tick dura 1 us
	TIM1->ARR = ARR_INIT; //1999: el contador cuenta de 0 a 1999 -> 2000 us -> 500 Hz
	TIM1->CR1 &= ~TIM_CR1_DIR; //0: el contador cuenta hacia arriba
	TIM1->CR1 |= TIM_CR1_ARPE; //ARR con preload: el cambio de periodo se aplica al terminar el periodo, sin pulsos cortados

	/* Canal 1 en PWM modo 1 */
	TIM1->CCMR1 &= ~TIM_CCMR1_CC1S; //00: el canal 1 es una salida (no una captura)
	TIM1->CCMR1 &= ~TIM_CCMR1_OC1M; //Limpia los 3 bits del modo de salida
	TIM1->CCMR1 |= (TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2); //110: PWM modo 1, alto mientras CNT < CCR1
	TIM1->CCMR1 |= TIM_CCMR1_OC1PE; //CCR1 con preload: el ancho nuevo entra solo al inicio de un periodo

	TIM1->CCR1 = CCR_INIT; //Ancho inicial 1000 us: duty de 50 % al arrancar

	TIM1->CCER &= ~TIM_CCER_CC1P; //0: polaridad activa en alto
	TIM1->CCER |= TIM_CCER_CC1E; //Habilita la salida del canal 1 hacia el pin

	TIM1->BDTR |= TIM_BDTR_MOE; //TIM1 es un timer avanzado: sin MOE (Main Output Enable) las salidas quedan apagadas aunque todo lo demás esté bien

	TIM1->EGR |= TIM_EGR_UG; //Fuerza un update: copia PSC, ARR y CCR1 desde sus preload a los registros activos

	TIM1->CR1 |= TIM_CR1_CEN; //Arranca el contador, siempre de último
}

/*
 * tim2_encoder_Init
 * PA0 -> TIM2_CH1 (CLK), PA1 -> TIM2_CH2 (DT), ambos AF1 con Pull-Up
 * Modo encoder 3 (TI12): cuenta en los flancos de los dos canales
 * -> 4 cuentas por paso mecánico del KY040
 *
 * IC1F e IC2F: filtro digital de la entrada. Toma muestras a f_DTS/32 y solo
 * acepta un cambio si 8 muestras seguidas coinciden: 8 x 2 us = 16 us (f_DTS = 16 MHz)
 */
static void tim2_encoder_Init(void)
{
	/* GPIOA PA0 y PA1 como función alterna AF1 */
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN; //Activa la señal de reloj del GPIOA (bus AHB1)

	GPIOA->MODER &= ~(GPIO_MODER_MODER0 | GPIO_MODER_MODER1); //Limpia los bits de modo de PA0 y PA1
	GPIOA->MODER |= (GPIO_MODER_MODER0_1 | GPIO_MODER_MODER1_1); //10: función alterna, los pines los lee el TIM2

	GPIOA->PUPDR &= ~(GPIO_PUPDR_PUPDR0 | GPIO_PUPDR_PUPDR1); //Limpia los bits de pull de PA0 y PA1
	GPIOA->PUPDR |= (GPIO_PUPDR_PUPDR0_0 | GPIO_PUPDR_PUPDR1_0); //01: Pull-Up, el KY040 cierra a GND y sin pull-up la entrada flotaría entre pasos

	GPIOA->AFR[0] &= ~(GPIO_AFRL_AFSEL0 | GPIO_AFRL_AFSEL1); //Limpia los bits de función alterna de PA0 y PA1 (pines 0-7 están en AFR[0] o AFRL)
	GPIOA->AFR[0] |= (GPIO_AFRL_AFSEL0_0 | GPIO_AFRL_AFSEL1_0); //0001: AF1, conecta PA0 con TIM2_CH1 y PA1 con TIM2_CH2

	/* Base de tiempo TIM2 (32 bit) */
	RCC->APB1ENR |= RCC_APB1ENR_TIM2EN; //Activa la señal de reloj del TIM2 (bus APB1)

	TIM2->PSC = 0; //Sin división: se quiere contar cada flanco del encoder
	TIM2->ARR = 0xFFFFFFFF; //Rango completo de 32 bit: el contador no se desborda en uso normal
	TIM2->CNT = 0; //El encoder inicia en 0

	/* Canales 1 y 2 como entradas */
	TIM2->CCMR1 &= ~(TIM_CCMR1_CC1S | TIM_CCMR1_CC2S); //Limpia la selección de entrada de ambos canales
	TIM2->CCMR1 |= (TIM_CCMR1_CC1S_0 | TIM_CCMR1_CC2S_0); //01: CC1 lee TI1 y CC2 lee TI2 (conexión directa)

	TIM2->CCMR1 &= ~(TIM_CCMR1_IC1F | TIM_CCMR1_IC2F); //Limpia los 4 bits de filtro de cada canal
	TIM2->CCMR1 |= (TIM_CCMR1_IC1F | TIM_CCMR1_IC2F); //1111: filtro máximo, f_DTS/32 con N = 8 (rechaza glitches de menos de 16 us)

	TIM2->CCER &= ~(TIM_CCER_CC1P | TIM_CCER_CC1NP | TIM_CCER_CC2P | TIM_CCER_CC2NP); //Sin inversión de polaridad (si cuenta al revés se activa CC1P)
	TIM2->CCER |= (TIM_CCER_CC1E | TIM_CCER_CC2E); //Habilita la captura de los dos canales

	/* Modo encoder */
	TIM2->SMCR &= ~TIM_SMCR_SMS; //Limpia los 3 bits del modo esclavo
	TIM2->SMCR |= (TIM_SMCR_SMS_0 | TIM_SMCR_SMS_1); //011: encoder mode 3, cuenta con los flancos de TI1 y TI2 (4 cuentas por paso)

	TIM2->CR1 |= TIM_CR1_CEN; //Arranca el contador, siempre de último
}

/*
 * exti0_Init
 * PB0 (SW del KY040) -> EXTI0, flanco de bajada. Es la misma secuencia del
 * botón PC13 (EXTI13), cambia el pin, la línea y el registro EXTICR.
 * PC13 ya tiene pull-up en la tarjeta; PB0 es un cable al KY040, por eso
 * se activa el Pull-Up interno.
 */
static void exti0_Init(void)
{
	/* GPIOB PB0 como entrada con Pull-Up */
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN; //Activa la señal de reloj del GPIOB (bus AHB1)

	GPIOB->MODER &= ~GPIO_MODER_MODER0; //00: entrada

	GPIOB->PUPDR &= ~GPIO_PUPDR_PUPDR0; //Limpia los bits de pull de PB0
	//GPIOB->PUPDR |= GPIO_PUPDR_PUPDR0_0; //01: Pull-Up, en reposo el pin lee 1 y al presionar baja a 0

	/* Conexión de PB0 a la línea EXTI0 */
	RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN; //Activa el reloj del SYSCFG (bus APB2), el multiplexor de EXTI vive ahí

	SYSCFG->EXTICR[0] &= ~SYSCFG_EXTICR1_EXTI0; //Limpia la selección de puerto de la línea 0
	SYSCFG->EXTICR[0] |= SYSCFG_EXTICR1_EXTI0_PB; //0001: la línea EXTI0 toma el pin 0 del puerto B

	EXTI->RTSR &= ~EXTI_RTSR_TR0; //Sin disparo por flanco de subida (al soltar)
	EXTI->FTSR |= EXTI_FTSR_TR0; //Disparo por flanco de bajada (el pulsador conecta PB0 a GND)

	EXTI->PR = EXTI_PR_PR0; //Limpia una posible bandera pendiente (se limpia escribiendo 1)
	EXTI->IMR |= EXTI_IMR_MR0; //Desenmascara la línea 0: deja que llegue la interrupción al NVIC

	__NVIC_EnableIRQ(EXTI0_IRQn); //Habilita la interrupción de la línea 0 en el NVIC
}

/* Funciones ISR (solo registran el evento y regresan, sin lógica de la FSM) */

/* Base de tiempo: cada 1 ms cuenta el milisegundo y cada blink_half_period_ms deja la bandera del blinky */
void TIM4_IRQHandler(void)
{
	static uint16_t blink_count = 0; //Milisegundos desde el último cambio del LED (static: conserva su valor entre interrupciones)

	if (TIM4->SR & TIM_SR_UIF) //Confirma que la interrupción fue por actualización
	{
		TIM4->SR &= ~TIM_SR_UIF; //Baja la bandera (escribiendo 0) para que la interrupción se pueda volver a generar
		blink_count++; //Un milisegundo más
		if (blink_count >= blink_half_period_ms) //Se cumplió el medio periodo: 125 ms (4 Hz) o 250 ms (2 Hz) según el modo
		{
			blink_count = 0; //Reinicia la cuenta para el próximo medio periodo
			blink_flag = 1; //Deja el evento: la FSM lo convierte en EV_BLINK_TICK
		}
	}
}

/* Pulsador por EXTI0 */
void EXTI0_IRQHandler(void)
{
	if (EXTI->PR & EXTI_PR_PR0) //Confirma que la línea 0 generó la interrupción
	{
		EXTI->PR = EXTI_PR_PR0; //Baja la bandera escribiendo 1 (en EXTI->PR se limpia con 1, no con 0)
		button_flag = 1; //Deja el evento: la FSM lo convierte en EV_BUTTON_PRESS
	}
}
```

---

## 8. Recorrido por el código

### 8.1 Variables, enums y FSM

| Elemento | Para qué |
|---|---|
| `State_t`, `Event_t` | Estados y eventos de la FSM (con `NUM_STATES` y `NUM_EVENTS` como centinelas) |
| `Mode_t`, `edit_mode` | Qué valor edita el encoder: `MODE_CCR` o `MODE_ARR` |
| `current_state` | Estado actual, `static`: solo lo modifica el `main` |
| `state_entry_flag` | Vale 1 al llegar a un estado, para que su `[ENTER]` corra una sola vez |
| `button_flag`, `blink_flag` | `volatile`: las ISR las escriben y `get_next_event()` las lee |
| `blink_half_period_ms` | `volatile`: el `main` lo cambia (125 o 250 ms) y la ISR del TIM4 lo lee |
| `arr_value`, `ccr_value` | El ARR y el CCR1 que tiene el TIM1 en este momento |
| `ARR_STEP`, `CCR_STEP` | Cuánto suma o resta cada paso del encoder (20 y 10) |
| `ARR_INIT`, `ARR_MIN`, `ARR_MAX` | ARR inicial (1999) y límites (99 y 65534) |

Funciones por rol (así se separa el QUÉ del CÓMO):

| Rol | Funciones |
|---|---|
| Política (el QUÉ) | El `switch` del `main` |
| Drivers (el CÓMO) | `get_next_event`, `encoder_HasSteps`, `encoder_TakeSteps`, `pwm_ApplySteps`, `mode_Toggle`, `led_Toggle` |
| Inicialización | `gpio_Init`, `tim4_Init`, `tim1_pwm_Init`, `tim2_encoder_Init`, `exti0_Init` |
| ISR (solo banderas) | `TIM4_IRQHandler`, `EXTI0_IRQHandler` |

### 8.2 `gpio_Init` y `tim4_Init` (blinky)

PH1 como salida push-pull. TIM4 con `PSC = 15` (tick de 1 µs) y `ARR = 999` (interrupción cada 1 ms; con `ARR = 0` el contador queda bloqueado y nunca interrumpe). En `TIM4_IRQHandler` se limpia `UIF`, se cuenta un milisegundo y, al llegar a `blink_half_period_ms`, se reinicia la cuenta y se levanta `blink_flag`; el LED lo alterna la FSM con `^=`. Cambiar entre 125 ms (4 Hz) y 250 ms (2 Hz) es cambiar una variable: el TIM4 no se reconfigura. Antes de habilitar la interrupción se fuerza un `UG` para cargar el `PSC` y luego se limpia `UIF`, si no entraría una interrupción falsa.

### 8.3 `tim1_pwm_Init`

| Paso | Registro | Por qué |
|---|---|---|
| Reloj | `RCC->AHB1ENR`, `RCC->APB2ENR` | Sin reloj el periférico no responde (TIM1 está en el APB2) |
| PA8 alterna | `MODER`, `AFR[1]` | AF1 conecta PA8 con TIM1_CH1 (pines 8-15 usan `AFR[1]`) |
| Tick 1 µs | `PSC = 15` | 16 MHz / 16 |
| Periodo y ancho | `ARR = ARR_INIT`, `CCR1 = CCR_INIT` | 500 Hz con duty de 50 % al inicio |
| Preloads | `CR1.ARPE`, `CCMR1.OC1PE` | Los cambios se aplican al terminar el periodo, sin pulsos recortados |
| PWM 1 | `CCMR1.OC1M = 110` | Alto mientras `CNT < CCR1` |
| Salida | `CCER.CC1E` y `BDTR.MOE` | Habilita el canal y la salida principal |
| Cargar | `EGR.UG` | Pasa los valores de los preloads a los registros activos |
| Arranque | `CR1.CEN` | Siempre de último |

### 8.4 `tim2_encoder_Init`

PA0/PA1 en AF1 con Pull-Up (`AFR[0]`), canales como entrada (`CC1S`, `CC2S`), filtros `IC1F`/`IC2F`, sin inversión de polaridad, y `SMS = 011`. `CEN` al final.

### 8.5 `exti0_Init`

Ver la sección 4. Es la misma secuencia de PC13 con otro pin, otra línea y otro registro `EXTICR`.

### 8.6 `main` y la FSM

1. Se inicializa el hardware (el blinky de último, para que sus interrupciones empiecen con todo listo).
2. En cada pasada del `while(1)`: si el estado es `STATE_IDLE` se pide un evento con `get_next_event()`; en los demás estados el evento es `EV_NONE` (si se leyera ahí, se perdería).
3. Se valida el estado y el evento. Si alguno está fuera de rango se pasa a `STATE_ERROR`.
4. El `switch` ejecuta el estado:
   - `STATE_IDLE`: no hace nada y solo mira el evento para decidir el estado siguiente.
   - `STATE_UPDATE_PWM`: en el `[ENTER]` toma los pasos del encoder y los aplica al ARR o al CCR1 según `edit_mode`.
   - `STATE_CHANGE_MODE`: en el `[ENTER]` alterna `edit_mode` y la velocidad del blinky.
   - `STATE_BLINK`: en el `[ENTER]` cambia el LED.
   - `STATE_ERROR` y `default`: vuelven a `STATE_IDLE`.
5. Los estados de atención regresan a `STATE_IDLE` en la misma pasada.

### 8.7 `get_next_event` y los drivers del encoder y del PWM

`get_next_event` entra en sección crítica y revisa en este orden: pulsador, encoder (`encoder_HasSteps`), blinky. Devuelve **un** evento por llamada. Las banderas que no se atendieron siguen en 1 para la siguiente pasada.

`encoder_TakeSteps` convierte las cuentas del TIM2 en pasos con signo y deja el residuo en `CNT`. `pwm_ApplySteps` suma `pasos * CCR_STEP` al CCR1 o `pasos * ARR_STEP` al ARR, lo calcula en `int32_t` para detectar que se pasó de los límites, satura, recorta el CCR1 a `ARR + 1` y escribe `TIM1->ARR` y `TIM1->CCR1`. `mode_Toggle` alterna el modo y el periodo del blinky.

---

## 9. Depuración paso a paso

Poner estas variables/registros en **Live Watch**:

| Variable | Qué esperar |
|---|---|
| `TIM2->CNT` | Sube/baja de 4 en 4 por cada paso del encoder y se descuenta al aplicarlo (queda de -3 a 3) |
| `edit_mode` | 0 (`MODE_CCR`) o 1 (`MODE_ARR`), cambia con el pulsador |
| `ccr_value`, `TIM1->CCR1` | Cambia de 10 en 10 en `MODE_CCR`, entre 0 y `ARR + 1` |
| `arr_value`, `TIM1->ARR` | Empieza en 1999, cambia de 20 en 20 en `MODE_ARR`, entre 99 y 65534 |
| `blink_half_period_ms` | 125 en `MODE_CCR`, 250 en `MODE_ARR` |
| `current_state` | Casi siempre `STATE_IDLE` (0); los otros estados duran una pasada |
| `button_flag`, `blink_flag` | 1 apenas la ISR corre, 0 cuando `get_next_event()` la atiende |
| `EXTI->PR` | Bit 0 en 1 solo mientras la ISR no ha corrido |

Experimentos:

1. Giren un paso en `MODE_CCR`: `TIM1->CCR1` sube 10 y en el osciloscopio el pulso se ensancha 10 µs.
2. Presionen el pulsador y giren un paso: ahora cambia `TIM1->ARR` en 20 y el periodo en 20 µs. Cuenten los parpadeos de PH1 en un segundo: 4 en `MODE_CCR`, 2 en `MODE_ARR`.
3. Suban el CCR1 hasta `ARR + 1` (señal siempre en alto) y bajen el ARR en `MODE_ARR`: el CCR1 se recorta para no quedar mayor que `ARR + 1`.
4. En `MODE_CCR` giren hacia atrás hasta que `ccr_value` llegue a 0 y sigan girando: debe quedarse en 0 (saturado), no dar la vuelta. Arranca en 1000 (50 %).
5. Quiten `TIM1->BDTR |= TIM_BDTR_MOE;` y observen qué pasa en PA8. ¿Por qué?
6. Pongan un breakpoint en `EXTI0_IRQHandler` y cuenten cuántos flancos genera **un solo click**. ¿Por qué a veces el modo cambia dos veces?
7. Cambien `IC1F`/`IC2F` a `0000` y giren despacio. ¿Cambia algo en la cuenta? ¿Y si giran rápido?
8. Pongan un breakpoint en `led_Toggle()` y verifiquen que la ISR del TIM4 **no** cambia el LED: solo deja `blink_flag` y el LED lo cambia la FSM.
9. Dentro de `STATE_IDLE` agreguen un `for(;;);` de prueba por un segundo y observen qué pasa con el encoder y el pulsador. Por eso la FSM no se bloquea.
10. Si el LED no cambia de brillo al girar, revisen en orden: ¿`AFR` del pin?, ¿`CC1E`?, ¿`MOE`?, ¿`CEN`?, ¿`CCR1` distinto de 0?

> **Errores comunes:** olvidar el reloj de un periférico, escribir el `AFR` equivocado (`AFR[0]` es pines 0-7, `AFR[1]` pines 8-15), activar `CEN` antes de configurar, usar `=` en lugar de `|=` en registros compartidos, limpiar `EXTI->PR` con `&= ~` en vez de escribir 1, olvidar `volatile` en una bandera de ISR, escribir lógica de la FSM dentro de la ISR, y olvidar poner `state_entry_flag = 1` al cambiar de estado.

---

## 10. Ejercicios

0. Cambien `ARR_STEP` y `CCR_STEP` (por ejemplo a 50 y 5). ¿Qué cambia en el osciloscopio por cada paso del encoder?
1. Con `ARR_MIN = 9` el PWM llega a 100 kHz. ¿Cuántos ticks de 1 µs le quedan al pulso? ¿Cómo se ve la señal en el osciloscopio y en el LED?
2. Cambien el blinky para que sea de **1 Hz** en `MODE_CCR` y de **8 Hz** en `MODE_ARR`. ¿Qué constantes tocan y cuáles no?
3. Si el encoder cuenta al revés, inviertan el sentido **sin cambiar el cableado** (pista: `CC1P`).
4. Muevan el cable del SW a **PB1** (línea EXTI1). ¿Qué registros, bits y nombres cambian respecto a PB0? (pista: `EXTICR`, `TR`, `PR`, `MR`, el `IRQn` y el nombre del handler).
5. El pulsador rebota y a veces un click cambia el modo dos veces. Cuenten los flancos por click con un breakpoint y propongan (en papel) cómo ignorar los flancos que llegan muy seguidos. ¿Dónde pondrían la decisión, en la ISR o en `get_next_event()`? ¿Qué necesitarían medir?
6. **Reto FSM con tabla:** construyan `transition_table[NUM_STATES][NUM_EVENTS]` para esta máquina (primero en papel, como la tabla de la sección 5.1) y reemplacen el `if` de `STATE_IDLE` por una consulta a la tabla.
7. Agreguen un estado `STATE_RESET_PWM` al que se llegue con un nuevo evento `EV_BUTTON_LONG` (pulsador mantenido) y que ponga el CCR1 en 0. Dibujen primero el diagrama.
8. Agreguen una acción `[EXIT]` a `STATE_CHANGE_MODE` que apague el LED de PA8 un instante. ¿Dónde tiene que ir y por qué no en el `[ENTER]`?
