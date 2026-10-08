# Taller Semáforo Máquina de Estados (STM32F411 / NUCLEO-F411RE)

Monitoría de Electrónica Digital — 2026-2S

## 0. Objetivo del taller

En el taller anterior se aprendió a usar el debugger sobre código sencillo. Ahora
se construye un programa de firmware "de verdad": un **semáforo** hecho con tres
LEDs que cambia de estado por sí solo, sin que el procesador se quede
esperando. Para lograrlo se combinan tres ideas:

1. **Timer (TIM2)**: un contador por hardware que mide el tiempo.
2. **Interrupción**: el timer avisa al procesador cuando se cumple el tiempo.
3. **Máquina de estados**: el programa recuerda en qué parte del ciclo del
   semáforo está y decide qué hacer en cada aviso.

Al terminar el estudiante debe poder explicar por qué el `main` casi no hace
nada mientras el semáforo funciona, y usar el debugger para ver cómo cambian
`estado_actual`, `parpadeos` y los registros `TIM2->ARR` y `TIM2->CNT`.

---

## 1. Montaje

Tres LEDs en la protoboard, cada uno con una **resistencia en serie**
(típicamente entre 220 Ω y 330 Ω), conectados a la NUCLEO-F411RE así:

| LED      | Pin  |
|----------|------|
| Rojo     | PB6  |
| Verde    | PB8  |
| Amarillo | PB9  |

El otro extremo de cada LED va a GND. El pin entrega 3.3 V cuando está en alto,
así que el LED se **enciende con un 1** y se **apaga con un 0**.

El proyecto se crea igual que en el taller anterior (extensión de STM32 para VS
Code, proyecto **CMake**, board `NUCLEO_F411RE`). El código de este taller va
en un archivo `.c` dentro de `Src/`.

---

## 2. Qué es un timer

Un timer es un **contador** que el hardware incrementa solo, al ritmo de un
reloj, sin gastar instrucciones del procesador. En este taller se usa el
**TIM2**. Tres registros definen cuánto tarda en "cumplirse el tiempo":

| Registro    | Qué hace                                                                 |
|-------------|--------------------------------------------------------------------------|
| `TIM2->PSC` | **Prescaler**: divide el reloj. El timer cuenta a `f_reloj / (PSC + 1)`.  |
| `TIM2->ARR` | **Auto-reload**: valor máximo al que llega el contador antes de volver a 0. |
| `TIM2->CNT` | El **contador** en sí: su valor actual.                                  |

Cuando `CNT` llega a `ARR` y vuelve a 0 ocurre un **evento de actualización**
(*update event*). Ese evento levanta una bandera, `UIF`, en el registro
`TIM2->SR`, y si la interrupción está habilitada, también avisa al procesador.

### 2.1 Cuenta del tiempo

Después de un reset, el micro corre con su reloj interno de **16 MHz** y el
TIM2 recibe esos mismos 16 MHz (este taller no configura otro reloj).

```
f_timer = 16 000 000 Hz / (PSC + 1)
T       = (ARR + 1) / f_timer
```

Con `PSC = 16000 - 1`:

```
f_timer = 16 000 000 / 16 000 = 1000 Hz   →  cada cuenta dura 1 ms
```

Con `ARR = 1000 - 1`:

```
T = 1000 cuentas × 1 ms = 1 s
```

El "`- 1`" aparece porque los dos registros cuentan desde 0: un `ARR` de 999
significa 1000 cuentas (de 0 a 999).

---

## 3. Qué es una interrupción

Sin interrupciones, el programa tendría que **preguntar** una y otra vez si ya
pasó el tiempo (*polling*), gastando todo el procesador en esa pregunta. Con una
interrupción ocurre al revés: el hardware **avisa** y el procesador deja
momentáneamente lo que hacía para atender el aviso.

Para que el aviso del TIM2 llegue al procesador hay tres "interruptores" que
deben estar activados:

1. En el timer: `TIM2->DIER |= TIM_DIER_UIE;` — "avísame en cada evento de actualización".
2. En el NVIC (el controlador de interrupciones del micro): `__NVIC_EnableIRQ(TIM2_IRQn);`
3. El timer debe estar contando: `TIM2->CR1 |= TIM_CR1_CEN;`

Cuando el aviso llega, el procesador ejecuta la **rutina de servicio de
interrupción** (ISR). Su nombre no se escoge libremente: para el TIM2 se llama
exactamente `TIM2_IRQHandler`, porque así está registrada en la tabla de
vectores del archivo de arranque (`startup_stm32f411xx.S`). Por eso nadie la
llama desde el `main`, y por eso no necesita cabecera.

Reglas de oro de una ISR:

- Debe ser **corta**: mientras corre, el programa principal está detenido.
- Debe **borrar la bandera** (`TIM2->SR &= ~TIM_SR_UIF;`). Si no se borra, el
  procesador cree que el aviso sigue pendiente y vuelve a entrar a la ISR sin parar.
- Lo pesado se hace en el `main`. La ISR solo deja una **bandera de aviso**.

En este taller la ISR solo hace una cosa: pone `cambio = 1`. El `main` revisa esa
variable y hace el trabajo.

---

## 4. Máquina de estados

Un semáforo tiene un ciclo: rojo, verde, verde parpadeando, amarillo, y de
nuevo rojo. En cada aviso del timer hay que saber **en qué punto del ciclo
estamos** para decidir qué luz toca. Esa "memoria" es el **estado**, y el
programa que cambia de estado según lo que ocurre es una **máquina de estados**.

En C se escribe con un `enum` (nombres legibles para los estados) y un `switch`
(una rama por estado):

```c
typedef enum {
    Rojo,
    VerdeE,    // verde "encendido" (fijo)
    VerdeP,    // verde "parpadeando"
    Amarillo
} EstadoSemaforo;

EstadoSemaforo estado_actual = Rojo;
```

Un `enum` le pone nombre a números enteros (`Rojo` = 0, `VerdeE` = 1,
`VerdeP` = 2, `Amarillo` = 3). Es más claro escribir `case VerdeP:` que `case 2:`.
Los estados están declarados en el **mismo orden del ciclo** del semáforo, que es
el mismo orden de los `case` del `switch`: así el `enum` se lee como el recorrido
completo.

Cada vez que el `main` atiende un aviso, ejecuta **la rama del estado actual** y
al final **escribe el estado siguiente**:

```
 ┌──────────┐     ┌──────────┐     ┌──────────────────────┐     ┌──────────────┐
 │  Rojo    │────►│  VerdeE  │────►│       VerdeP         │────►│   Amarillo   │
 │ rojo ON  │     │ verde ON │     │ verde alterna (×10)  │     │ amarillo ON  │
 └──────────┘     └──────────┘     │ ARR = 250-1          │     └──────┬───────┘
      ▲                            └──────────────────────┘            │
      └────────────────────────────────────────────────────────────────┘
```

> El orden en que se declaran los estados no cambia el funcionamiento: lo que
> define el ciclo son las líneas `estado_actual = ...;` de cada `case`. Aun así,
> conviene escribir el `enum` y el `switch` en el mismo orden del ciclo.

El estado `VerdeP` se queda en sí mismo varios avisos seguidos: en cada aviso
alterna el LED verde y cuenta en `parpadeos`. Con `parpadeos == 5*2` (10
cambios = 5 parpadeos completos de encendido y apagado) pasa a `Amarillo`.

**El `default` del `switch`.** Al final del `switch` está el caso `default:`. Se
ejecuta cuando `estado_actual` trae un valor que no corresponde a ninguno de los
cuatro estados (por ejemplo, si una variable se corrompe o si se agrega un
estado al `enum` y se olvida su `case`). Aquí manda la máquina de vuelta a
`Rojo`, de modo que el semáforo se recupera solo en lugar de quedarse
sin hacer nada. En el funcionamiento normal nunca se ejecuta, pero es una buena
costumbre ponerlo siempre.

---

## 5. Manipular bits de un registro

Todo el código de este taller usa tres patrones sobre registros. Se aplican con
las **máscaras** que define el archivo `stm32f411xe.h` (por ejemplo,
`GPIO_ODR_OD6` es un número con un 1 solo en el bit 6).

| Operación          | Código                          | Efecto                              |
|--------------------|---------------------------------|-------------------------------------|
| Poner un bit en 1  | `REG \|= MASCARA;`              | Enciende ese bit, deja los demás igual |
| Poner un bit en 0  | `REG &= ~MASCARA;`              | Apaga ese bit, deja los demás igual |
| Invertir un bit    | `REG ^= MASCARA;`               | Si era 0 pasa a 1, y viceversa      |

Ejemplos del taller:

```c
GPIOB->ODR |=  GPIO_ODR_OD6;   // enciende el LED rojo (PB6 = 1)
GPIOB->ODR &= ~GPIO_ODR_OD8;   // apaga el LED verde  (PB8 = 0)
GPIOB->ODR ^=  GPIO_ODR_OD8;   // alterna el LED verde (parpadeo)
```

`~MASCARA` invierte todos los bits de la máscara, de modo que el `&=` pone en 0
**solo** el bit deseado y no toca los otros LEDs.

---

## 6. El código completo

Sigue el orden de siempre: includes, variables globales, cabeceras de
funciones, `main()` con su `while(1)` y, al final, la definición de las funciones.

```c
#include <stdint.h>
#include "stm32f411xe.h"
#include <stm32f4xx.h>

//Definición de variables
volatile uint8_t cambio = 0; // Variable para indicar el cambio de estado del semáforo
uint8_t parpadeos = 0;

//Cabecera de funciones
void init_GPIO(void);
void init_timers(void);

typedef enum {
    Rojo,
    VerdeE,
    VerdeP,
    Amarillo
} EstadoSemaforo;

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
                    GPIOB->ODR |= GPIO_ODR_OD6;  // Encender LED rojo
                    GPIOB->ODR &= ~GPIO_ODR_OD8; // Apagar LED verde
                    GPIOB->ODR &= ~GPIO_ODR_OD9; // Apagar LED amarillo

                    estado_actual = VerdeE;
                    break;

                case VerdeE:
                    GPIOB->ODR |= GPIO_ODR_OD8;  // Encender LED verde
                    GPIOB->ODR &= ~GPIO_ODR_OD9; // Apagar LED amarillo
                    GPIOB->ODR &= ~GPIO_ODR_OD6; // Apagar LED rojo

                    estado_actual = VerdeP;
                    break;

                case VerdeP:
                    TIM2->ARR = 250 - 1;
                    GPIOB->ODR ^= GPIO_ODR_OD8;  // Alternar LED verde
                    parpadeos++;

                    if (parpadeos == 5*2) {
                        parpadeos = 0;
                        estado_actual = Amarillo;
                        TIM2->ARR = 1000 - 1;
                    }
                    break;

                case Amarillo:
                    GPIOB->ODR |= GPIO_ODR_OD9;  // Encender LED amarillo
                    GPIOB->ODR &= ~GPIO_ODR_OD6; // Apagar LED rojo
                    GPIOB->ODR &= ~GPIO_ODR_OD8; // Apagar LED verde

                    TIM2->ARR = 2000 - 1;
                    estado_actual = Rojo;
                    break;

                default:                       // cualquier valor que no sea un estado válido
                    estado_actual = Rojo;      // vuelve al inicio del ciclo
                    break;
            }
        }
    }
    return 0;
}

//Funciones
void init_GPIO(void){

    //Señal de reloj GPIOB
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;

    //Verde PB8, Amarillo PB9, Rojo PB6.
    GPIOB->MODER &= ~(GPIO_MODER_MODE8 | GPIO_MODER_MODE9 | GPIO_MODER_MODE6);   // Limpiar bits de modo
    GPIOB->MODER |= (GPIO_MODER_MODE8_0 | GPIO_MODER_MODE9_0 | GPIO_MODER_MODE6_0); // Salida de propósito general
    GPIOB->OTYPER &= ~(GPIO_OTYPER_OT8 | GPIO_OTYPER_OT9 | GPIO_OTYPER_OT6);     // Push-pull
    GPIOB->OSPEEDR &= ~(GPIO_OSPEEDR_OSPEED8 | GPIO_OSPEEDR_OSPEED9 | GPIO_OSPEEDR_OSPEED6); // Baja velocidad
    GPIOB->PUPDR &= ~(GPIO_PUPDR_PUPD8 | GPIO_PUPDR_PUPD9 | GPIO_PUPDR_PUPD6);   // Sin pull-up/pull-down
    GPIOB->ODR &= ~(GPIO_ODR_OD8 | GPIO_ODR_OD9 | GPIO_ODR_OD6);                 // Todos en bajo
}

void init_timers(void){
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN; // Habilitar el reloj para TIM2
    TIM2->PSC = 16000 - 1;              // Prescaler: cuenta a 1 kHz
    TIM2->ARR = 1000 - 1;               // Auto-reload: 1000 cuentas = 1 s
    TIM2->CNT = 0;                      // Contador en 0

    TIM2->CR1 &= ~TIM_CR1_DIR;          // Cuenta hacia arriba
    TIM2->CR1 |= TIM_CR1_ARPE;          // ARR con precarga (ver sección 7.3)

    TIM2->SR &= ~TIM_SR_UIF;            // Limpiar la bandera de actualización
    TIM2->DIER |= TIM_DIER_UIE;         // Habilitar la interrupción de actualización

    __NVIC_EnableIRQ(TIM2_IRQn);        // Habilitar la interrupción de TIM2 en el NVIC

    TIM2->CR1 |= TIM_CR1_CEN;           // Arrancar el temporizador
}

void TIM2_IRQHandler(void){
    if (TIM2->SR & TIM_SR_UIF) {        // ¿La bandera de actualización está activa?
        TIM2->SR &= ~TIM_SR_UIF;        // Limpiarla

        cambio = 1;                     // Avisar al main
    }
}
```

---

## 7. Recorrido por el código

### 7.1 Variables globales

- `cambio`: la ISR la pone en 1; el `main` la lee y la vuelve a poner en 0. Es la
  **comunicación** entre la interrupción y el programa principal.
- `parpadeos`: cuenta los cambios del LED verde dentro del estado `VerdeP`.
- `estado_actual`: la memoria de la máquina de estados.

Las tres son `uint8_t` o un `enum`, y son **globales** porque deben conservar su
valor entre un aviso y el siguiente (una variable local desaparecería al salir
de la función).

> **`volatile`:** una variable que cambia dentro de una ISR y se lee en el
> `main` se declara `volatile`, como `cambio`. Le dice al
> compilador que el valor puede cambiar "por fuera" del flujo normal del
> código, para que no lo guarde en un registro de la CPU y siga usando una copia
> vieja cuando se activan las optimizaciones.

### 7.2 `init_GPIO`

1. Activa el reloj del puerto B (`RCC->AHB1ENR`). Un periférico sin reloj no
   responde a ninguna escritura.
2. Configura PB6, PB8 y PB9 como **salidas** (`MODER`), de tipo **push-pull**
   (`OTYPER`), velocidad baja (`OSPEEDR`) y sin resistencias internas (`PUPDR`).
3. Deja los tres LEDs apagados (`ODR`).

Cada pin ocupa **2 bits** en `MODER`, `OSPEEDR` y `PUPDR`; por eso las máscaras
`GPIO_MODER_MODE8` ocupan 2 bits y para "salida" se pone el bit bajo de ese par
(`GPIO_MODER_MODE8_0`).

### 7.3 `init_timers`

Prepara el contador (`PSC`, `ARR`, `CNT`), habilita la interrupción (`UIE` en el
timer y `__NVIC_EnableIRQ` en el NVIC) y por último arranca el timer (`CEN`).
Arrancar de último evita que el timer cuente mientras se está configurando.

**`ARPE` (precarga de `ARR`):** con `ARPE` en 1, un valor nuevo escrito en `ARR`
no se aplica de inmediato, sino **en el siguiente evento de actualización**.
Así el período en curso termina completo y el cambio no corta una cuenta a
medias. Consecuencia: el `TIM2->ARR = 250 - 1;` del estado `VerdeP` solo afecta al
período **siguiente** al aviso donde se escribió.

### 7.4 `main`

Es un bucle infinito que solo reacciona cuando `cambio == 1`. Mientras no hay
aviso, el procesador está libre (aquí solo da vueltas, pero en un programa
más grande podría hacer otras tareas). Cuando llega el aviso:

1. Baja la bandera (`cambio = 0`) para no atender dos veces el mismo aviso.
2. Ejecuta la rama del `switch` que corresponde a `estado_actual`.
3. Esa rama deja programado el **estado siguiente** y, si hace falta, cambia el
   `ARR` para que el próximo aviso llegue antes o después.

### 7.5 `TIM2_IRQHandler`

Verifica que la causa fue la actualización del TIM2 (`UIF`), borra la bandera y
levanta `cambio`. Son tres líneas a propósito.

---

## 8. Depuración paso a paso

Se usa la configuración `STM32Cube: STM32Launch STLink GDB Server`, igual que en
el taller anterior.

### 8.1 Qué observar

Agregar al panel **Watch** (o mirar en **Variables**):

- `estado_actual`, `parpadeos`, `cambio`
- `TIM2->CNT`, `TIM2->ARR`, `TIM2->PSC`
- `GPIOB->ODR`

El panel de **Peripherals / SFRs** de la extensión de STM32 permite abrir
`TIM2` y `GPIOB` y ver cada registro desglosado por bits.

### 8.2 Experimentos guiados

1. **Breakpoint en `cambio = 1;`** (dentro de la ISR). Dar *Continue*: el programa
   se detiene una vez por segundo. Comprobar que `TIM2->CNT` está cerca de 0
   y que se cumple lo que dice la cuenta de la sección 2.1.
2. **Breakpoint en la primera línea del `switch`**. En cada parada, anotar el
   valor de `estado_actual` y comprobar que sigue el ciclo del diagrama.
3. **Breakpoint dentro de `case VerdeP`**. Observar cómo sube `parpadeos` de 1
   a 10 y cómo `GPIOB->ODR` alterna el bit 8 en cada parada.
4. **Seguir `TIM2->ARR`** a lo largo de un ciclo completo. Predecir antes de
   mirar: ¿en qué aviso se ve reflejado cada cambio de `ARR` en el tiempo real
   entre luces? (Pista: `ARPE`, sección 7.3.)
5. **Pausar con el botón *Pause*** en un momento al azar y ver en qué parte del
   código quedó el procesador. Casi siempre estará en el `while(1)`, esperando.

> Al detener el programa con el debugger, el procesador se frena pero el timer
> **puede seguir contando** (depende de la configuración del debugger). Por eso,
> si se pausa mucho tiempo, es posible que al continuar llegue un aviso de inmediato.

---

## 9. Ejercicios

1. Cambiar la duración del rojo a 3 segundos. ¿Qué registro y qué valor hay que tocar?
2. Hacer que el verde parpadee 3 veces en vez de 5 (¿qué número cambia?).
3. Agregar un estado `AmarilloP` en el que el amarillo parpadee antes de pasar a
   rojo. ¿Qué hay que agregar al `enum`, al `switch` y al diagrama?
4. Medir con el debugger cuánto vale `TIM2->CNT` justo antes del aviso, y
   explicar por qué nunca supera `ARR`.
5. Borrar la línea `TIM2->SR &= ~TIM_SR_UIF;` de la ISR y describir qué pasa
   (usar un breakpoint en la ISR). Volver a ponerla.
