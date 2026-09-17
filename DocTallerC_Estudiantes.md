# Taller 1 — Primer acercamiento al debugger en STM32F411 (NUCLEO-F411RE)

Monitoría de Electrónica Digital — 2026-2S

## 0. Objetivo del taller

Que el estudiante dé sus primeros pasos con la extensión de **STM32 para VS Code**
sobre la tarjeta **NUCLEO-F411RE**: crear un proyecto desde cero, entender la
estructura mínima de un programa en C (tipos de variables, tipos de funciones,
orden de ejecución) y aprender a usar el **debugger** paso a paso (breakpoints,
step, y ver variables en *Watch*/*Locals*) sobre un ejemplo sencillo: una
calculadora.

---

## 1. Crear un proyecto vacío

Antes de escribir una sola línea de código, se crea el proyecto en VS Code:

1. **Cerrar** cualquier proyecto que tengan abierto en VS Code.
2. En la extensión de **STM32**, ir a **"Create empty project"**.
3. Escribir el **nombre** del proyecto.
4. Seleccionar la board **`NUCLEO_F411RE`**.
5. Asegurarse de que el generador de proyecto sea **CMake**.
6. Indicar la **ruta de la carpeta** donde se va a guardar el proyecto.
7. Dar clic en **"Create project"**.
8. Elegir **"Open in this window"** para abrir el proyecto recién creado.

Con esto queda un proyecto vacío, ya configurado para compilar y depurar sobre
la NUCLEO-F411RE, listo para escribir el código del ejemplo.

---

## 2. Tipos de variables

No todas las variables se usan igual ni ocupan lo mismo en memoria. Los tipos
básicos de C:

| Tipo            | ¿Qué guarda?                               | Tamaño típico (32 bits)  | Rango aproximado                           | Cuándo usarlo |
|-----------------|--------------------------------------------|--------------------------|--------------------------------------------|---------------|
| `char`          | Un carácter (o un entero muy pequeño)      | 1 byte                   | -128 a 127                                 | Letras, símbolos (`'+'`, `'-'`), banderas pequeñas |
| `int`           | Un número entero, **con signo**            | 4 bytes                  | -2,147,483,648 a 2,147,483,647             | Contadores, resultados enteros, valores que pueden ser negativos |
| `unsigned int` (**uint**) | Un número entero, **sin signo**  | 4 bytes                  | 0 a 4,294,967,295                          | Cantidades que nunca son negativas (direcciones, contadores de ciclos, tamaños) |
| `float`         | Número con decimales, precisión simple     | 4 bytes                  | ±3.4 × 10³⁸ (≈ 6-7 cifras significativas)  | Cálculos con decimales donde no se necesita máxima precisión |
| `double`        | Número con decimales, precisión doble      | 8 bytes                  | ±1.7 × 10³⁰⁸ (≈ 15-16 cifras significativas) | Cálculos con decimales que requieren mayor precisión (más costoso en memoria/tiempo en un micro) |

Puntos clave para resaltar en el taller:

- `int` vs `unsigned int`: la diferencia es si se permiten valores negativos. Un
  `unsigned int` "gana" rango positivo a cambio de no poder representar negativos.
- `char` en C en realidad **es un número** por debajo (su código ASCII); por eso
  se puede comparar un `char` contra un carácter como `'+'` y también se puede
  usar en operaciones aritméticas.
- `float` vs `double`: ambos manejan decimales, la diferencia es precisión y
  tamaño en memoria. En un microcontrolador con recursos limitados, `float`
  suele ser suficiente y más eficiente.

### 2.1 Tipos de ancho fijo (`stdint.h`) — el estándar en firmware

`int` no tiene un tamaño garantizado por el estándar de C: puede ser de 2, 4 o
más bytes dependiendo del compilador y la plataforma. En firmware eso es un
problema, porque casi siempre se necesita saber **exactamente** cuántos bits
ocupa una variable — por ejemplo, para que coincida con el tamaño de un
registro de un periférico (un puerto GPIO de 16 bits, un ADC de 12 bits, un
contador de 32 bits, etc.). Por eso en programación de microcontroladores se
prefieren los tipos de **ancho fijo** definidos en `stdint.h`:

| Tipo       | Bits | Con/sin signo | Rango                          |
|------------|------|---------------|--------------------------------|
| `uint8_t`  | 8    | sin signo     | 0 a 255                        |
| `int8_t`   | 8    | con signo     | -128 a 127                     |
| `uint16_t` | 16   | sin signo     | 0 a 65,535                     |
| `int16_t`  | 16   | con signo     | -32,768 a 32,767               |
| `uint32_t` | 32   | sin signo     | 0 a 4,294,967,295              |
| `int32_t`  | 32   | con signo     | -2,147,483,648 a 2,147,483,647 |

El nombre ya dice el tamaño: `uint8_t` es **u**nsigned **int** de **8** bits.
Usarlos deja explícito, solo con leer la declaración, cuánta memoria ocupa la
variable y qué rango de valores puede tomar — algo que en `int`/`unsigned int`
queda implícito y puede cambiar de un compilador a otro.

---

## 3. Estructuras de control

Aparte de las funciones que uno mismo escribe (ver sección siguiente), C tiene
**estructuras de control** propias del lenguaje: no son funciones, sino
palabras clave que deciden qué código se ejecuta y cuántas veces.

### 3.1 `if` — ejecutar algo solo si se cumple una condición

```c
if (numero1 > 0)
{
    // se ejecuta solo si numero1 es mayor que 0
}
```

**`=` no es lo mismo que `==`.** En C, un solo `=` es el operador de
**asignación**: le da un valor a una variable (`numero1 = 5;` guarda 5 en
`numero1`). Dos signos `==` son el operador de **comparación**: pregunta si
dos valores son iguales, y el resultado es 1 (verdadero) o 0 (falso).
Confundirlos es un error muy común al empezar: `if (numero1 = 5)` no compara
nada — **asigna** 5 a `numero1`, y como 5 es distinto de 0, la condición
siempre se cumple (ver la sección 3.3 sobre por qué en C cualquier valor
distinto de 0 cuenta como verdadero). Lo correcto para comparar es
`if (numero1 == 5)`.

### 3.2 `else if` — encadenar condiciones excluyentes

```c
if (operacion == '+')
{
    // se ejecuta si operacion es '+'
}
else if (operacion == '-')
{
    // si no era '+', se revisa si es '-'
}
```
Solo se ejecuta **una** de las ramas: apenas una condición se cumple, las
siguientes `else if` ya no se evalúan.

### 3.3 `while` — repetir mientras la condición sea verdadera

```c
while (contador < 10)
{
    contador = contador + 1;   // se repite hasta que contador llegue a 10
}
```

**¿Por qué `while(1)` se repite para siempre?** En C no existe un tipo
booleano "puro": una condición se considera **verdadera** si su valor es
**distinto de 0**, y **falsa** solo si es exactamente `0`. El número `1` es
distinto de 0, así que `while(1)` equivale a "mientras se cumpla algo que
siempre es verdad" — por eso nunca termina, y es la forma estándar de escribir
el bucle infinito que mantiene vivo un programa embebido.

### 3.4 `for` — repetir un número conocido de veces

```c
for (int i = 0; i < 10; i = i + 1)
{
    // se repite 10 veces: i vale 0, 1, 2, ... hasta 9
}
```

El `for` empaqueta en una sola línea lo que un `while` necesitaría en varias:
un valor inicial (`i = 0`), una condición de parada (`i < 10`) y cómo cambia la
variable en cada vuelta (`i = i + 1`). Se usa cuando se sabe de antemano cuántas
veces se va a repetir algo (por ejemplo, recorrer una lista de 10 elementos).

---

## 4. Tipos de funciones

Toda función en C se describe por lo que **recibe** (parámetros) y lo que
**entrega** (tipo de retorno). Existen 4 combinaciones posibles:

### 4.1 No recibe nada y no retorna nada (`void` → `void`)

```c
void parpadearLed(void);   // cabecera

void parpadearLed(void)    // definición
{
    // hace algo, pero no recibe datos ni devuelve nada
}
```

### 4.2 No recibe nada, pero sí retorna algo (`void` → tipo)

```c
int leerContador(void);    // cabecera

int leerContador(void)     // definición
{
    return 42;              // devuelve un valor, aunque no recibió ninguno
}
```

### 4.3 Recibe algo, pero no retorna nada (tipo → `void`)

```c
void imprimirResultado(int valor);   // cabecera

void imprimirResultado(int valor)    // definición
{
    // usa "valor" internamente, pero no devuelve nada
}
```

### 4.4 Recibe algo y también retorna algo (tipo → tipo)

```c
int sumar(int a, int b);     // cabecera

int sumar(int a, int b)      // definición
{
    return a + b;             // usa los parámetros y devuelve un resultado
}
```

**Idea central:** los parámetros son la "entrada" de la función (lo que necesita
para trabajar) y el tipo de retorno es su "salida" (lo que le devuelve a quien
la llamó). Una función puede tener cualquier combinación de ambas, incluyendo
ninguna entrada y ninguna salida.

---

## 5. Estructura de un programa en C

Algo clave para entender **antes** de programar: **el código se ejecuta una sola
vez, de arriba hacia abajo** (salvo el `while(1)` del `main`, que es el único
bucle infinito que "mantiene vivo" el programa — ver sección 3.3 sobre por qué
`1` nunca deja de ser verdadero). Por eso el **orden** en el que se escriben las
cosas importa: si una función se usa antes de haber sido declarada, el
compilador no la conoce todavía y falla.

Para evitar ese problema, un programa en C se organiza siempre en este orden:

```c
// 1. Includes: librerías que se van a usar
#include "main.h"

// 2. Definición de variables (globales)
int numero1;
int numero2;

// 3. Cabeceras (prototipos) de funciones
int sumar(int a, int b);
void configurarReloj(void);

// 4. main() con su while(1)
int main(void)
{
    // configuración inicial (se ejecuta una sola vez)

    while (1)
    {
        // código que se repite para siempre
    }
}

// 5. Definición completa de las funciones
int sumar(int a, int b)
{
    return a + b;
}

void configurarReloj(void)
{
    // ...
}
```

**¿Por qué van primero las cabeceras (prototipos)?** Porque el compilador lee de
arriba hacia abajo: si en el `main` se llama a `sumar(...)` pero la función
`sumar` está definida más abajo, el compilador necesita haber "visto" antes su
firma (nombre, tipo de retorno y parámetros) para saber que esa llamada es
válida. El prototipo es justamente esa promesa: "esta función existe, se ve así,
y su cuerpo completo viene más adelante".

---

## 6. Ejemplo integrador: la calculadora

Siguiendo la estructura de la sección 5, el ejemplo completo queda así. Los dos
números y la operación están **fijos en el código** (no se leen de UART ni de
botones): la idea es que el estudiante pueda cambiarlos, recompilar, y ver en el
debugger cómo cambia el resultado.

```c
#include <stdint.h>

/* ---------- Variables ---------- */
uint8_t numero1 = 10;
uint8_t numero2 = 3;
char operacion = '*';   // puede ser '+', '-', '*' o '/'
uint8_t resultado;

/* ---------- Cabeceras de funciones ---------- */
uint8_t sumar(uint8_t a, uint8_t b);
uint8_t restar(uint8_t a, uint8_t b);
uint8_t multiplicar(uint8_t a, uint8_t b);
uint8_t dividir(uint8_t a, uint8_t b);

/* ---------- main ---------- */
int main(void)
{
    while (1)
    {
        if (operacion == '+')
        {
            resultado = sumar(numero1, numero2);
        }
        else if (operacion == '-')
        {
            resultado = restar(numero1, numero2);
        }
        else if (operacion == '*')
        {
            resultado = multiplicar(numero1, numero2);
        }
        else if (operacion == '/')
        {
            if (numero2 != 0)
            {
                resultado = dividir(numero1, numero2);
            }
        }
    }
}

/* ---------- Funciones ---------- */
uint8_t sumar(uint8_t a, uint8_t b)
{
    return a + b;
}

uint8_t restar(uint8_t a, uint8_t b)
{
    return a - b;
}

uint8_t multiplicar(uint8_t a, uint8_t b)
{
    return a * b;
}

uint8_t dividir(uint8_t a, uint8_t b)
{
    return a / b;
}
```

Notas para el taller:

- El `while(1)` recalcula `resultado` en cada vuelta; como `numero1`, `numero2`
  y `operacion` no cambian, el valor será siempre el mismo — esto es intencional,
  para poder poner un breakpoint dentro del `while` y ver siempre la misma
  ejecución mientras se explica el debugger.
- Cambiar `operacion` a otro carácter (por ejemplo `'-'`) y volver a compilar es
  el primer ejercicio propuesto a los estudiantes.
- `numero1`, `numero2` y `resultado` son `uint8_t` en vez de `int`: los valores
  del ejemplo (10 y 3) caben perfectamente en 8 bits, así que esto ejemplifica en
  código real la ventaja de fijar el ancho de bits vista en la sección 2.1 — cada
  variable ocupa 1 byte en vez de los 4 de un `int`.
- `dividir` hace división **entera** (`a / b`), no de `float`: en este primer
  acercamiento no se usa `float` para no tener que hablar todavía de la unidad de
  punto flotante (FPU) del micro. Eso significa que el resultado se trunca (por
  ejemplo, `10 / 3` da `3`, no `3.33`).
- Antes de dividir se revisa `if (numero2 != 0)`: dividir entre 0 con enteros es
  un error grave en el microcontrolador (puede producir un `HardFault`), así que
  si `numero2` fuera 0 esa rama simplemente no actualiza `resultado` en esa
  vuelta — se salta la operación en vez de ejecutarla.

---

## 7. Depuración paso a paso

### 7.1 Iniciar el debugger

1. Ir al panel **"Run and Debug"** de VS Code (ícono de play con un bug, en la
   barra lateral izquierda).
2. En el selector de configuración, escoger **`STM32Cube: STM32Launch STLink GDB Server`**.

   **¿Qué es esto?** Un *GDB server* es un programa que corre en el computador y
   hace de **puente** entre el debugger de VS Code (que habla el protocolo GDB)
   y el chip STM32F411 físico, comunicándose con él a través del programador/
   depurador integrado en la NUCLEO (**ST-Link**). En otras palabras: VS Code no
   habla directamente con el microcontrolador — le pide al servidor GDB que lo
   haga por él, y el servidor traduce esas órdenes (poner un breakpoint, leer una
   variable, hacer un step) en comandos que el ST-Link entiende y ejecuta sobre
   el chip real.
3. Presionar el botón de **play** para compilar, flashear y arrancar la sesión
   de debug.

### 7.2 Breakpoints y control de ejecución

- Poner un **breakpoint** haciendo clic a la izquierda del número de línea
  (por ejemplo, en la línea del `if (operacion == '+')`). El programa se
  detendrá justo antes de ejecutar esa línea.
- Botones típicos de control:
  - **Continue**: sigue ejecutando hasta el próximo breakpoint.
  - **Step Over**: ejecuta la línea actual sin entrar en el detalle de las
    funciones que llama.
  - **Step Into**: si la línea actual llama a una función (como `sumar(...)`),
    entra a verla instrucción por instrucción.
  - **Step Out**: termina de ejecutar la función actual y vuelve a quien la
    llamó.
  - **Restart** / **Stop**: reinicia o termina la sesión de debug.

### 7.3 Ver el valor de las variables

- Panel **Variables / Locals**: muestra automáticamente todas las variables
  visibles en el punto donde está detenida la ejecución (por ejemplo, dentro de
  `sumar`, se verían `a` y `b`).
- Panel **Watch**: se agregan manualmente las variables que se quieren vigilar
  todo el tiempo, aunque el programa pase de una función a otra (por ejemplo,
  agregar `resultado`, `numero1`, `numero2` y `operacion` para verlas siempre,
  sin importar en qué función esté detenida la ejecución).
- Ejercicio sugerido: con un breakpoint justo después de cada rama del
  `if / else if`, hacer **Step Over** y observar en *Watch* cómo `resultado`
  cambia según la operación — esto conecta visualmente el código con lo que
  pasa "de verdad" en la memoria del microcontrolador.
