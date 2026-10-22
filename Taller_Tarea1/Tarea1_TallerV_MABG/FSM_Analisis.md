# Análisis de la FSM — Tarea 1 (7 segmentos + fotocompuertas)

Archivo analizado: `Src/main.c`. Patrón: **switch-case** (Pattern 1 de `05-FSM_Embedded_2026-02.pdf`), modelo **Moore** (la acción está ligada al estado), loop **no bloqueante**.

## 1. Visión general

```
 ISR (rápido)                 main loop (FSM)                    Drivers (CÓMO)
 ─────────────                ───────────────                    ──────────────
 EXTI0  → increment_counter=1 ┐
 EXTI2  → increment_counter=2 ├→ get_next_event() → switch(state) → update_digits()
 TIM3   → request_isrtim=1    ┘                                     mostrardigitos()
```

- Las ISR solo **levantan una bandera `volatile`** y salen. No hay lógica de aplicación en ellas.
- `get_next_event()` convierte esas banderas en un `Event_t` y **limpia la bandera consumida**.
- La FSM decide **qué** hacer (política); `update_digits()`, `mostrardigitos()` y `set_segments()` hacen el **cómo**.
- Cada pasada del `while(1)` procesa **a lo sumo un evento y una transición**.

## 2. Eventos

| Evento | Origen (ISR) | Bandera que la ISR escribe | Significado |
|---|---|---|---|
| `EV_NONE` | — | ninguna activa | No pasó nada |
| `EV_F2_FALLING` | `EXTI0_IRQHandler` (PA0, F2, flanco de **bajada**) | `increment_counter = 1` | Cuenta **−1** |
| `EV_F1_RISING` | `EXTI2_IRQHandler` (PC2, F1, flanco de **subida**) | `increment_counter = 2` | Cuenta **+1** |
| `EV_TIM3_TICK` | `TIM3_IRQHandler` (cada 6 ms: PSC=1600−1, ARR=60−1) | `request_isrtim = 1` | Toca refrescar **un dígito** |

Prioridad en `get_next_event()`: `increment_counter == 1` → `== 2` → `request_isrtim`. Se entrega un solo evento por llamada.

## 3. Estados

| Estado | Qué hace | Lee | Modifica | Siguiente |
|---|---|---|---|---|
| `STATE_IDLE` | **Do:** `update_digits()` en cada pasada. Espera un evento. | `counter_exti` | `digits[0..3]`, `current_state`, `state_entry_flag` | según evento (tabla 4) |
| `STATE_COUNT_DOWN` | **Entry:** `counter_exti--` | `counter_exti` | `counter_exti`, `current_state`, `state_entry_flag` | `IDLE` (incondicional) |
| `STATE_COUNT_UP` | **Entry:** `counter_exti++` | `counter_exti` | `counter_exti`, `current_state`, `state_entry_flag` | `IDLE` (incondicional) |
| `STATE_REFRESH` | **Entry:** `mostrardigitos()` | `digits[]`, `currentdigit` | `currentdigit`, pines `ODR` (transistores y segmentos), `current_state`, `state_entry_flag` | `IDLE` (incondicional) |
| `STATE_ERROR` / `default:` | Recuperación segura | — | `current_state`, `state_entry_flag` | `IDLE` |

## 4. Tabla de transiciones

| Estado \ Evento | `EV_NONE` | `EV_F2_FALLING` | `EV_F1_RISING` | `EV_TIM3_TICK` |
|---|---|---|---|---|
| `IDLE` | IDLE | **COUNT_DOWN** | **COUNT_UP** | **REFRESH** |
| `COUNT_DOWN` | IDLE | IDLE | IDLE | IDLE |
| `COUNT_UP` | IDLE | IDLE | IDLE | IDLE |
| `REFRESH` | IDLE | IDLE | IDLE | IDLE |
| `ERROR` | IDLE | IDLE | IDLE | IDLE |

## 5. Variables

| Variable | Tipo | Quién la escribe | Función |
|---|---|---|---|
| `counter_exti` | `uint16_t` | FSM (COUNT_UP/DOWN) | Valor a mostrar |
| `increment_counter` | `volatile uint8_t` | ISR EXTI0/EXTI2; `get_next_event()` la limpia | Bandera 1 = F2, 2 = F1 |
| `request_isrtim` | `volatile uint8_t` | ISR TIM3; `get_next_event()` la limpia | Bandera de refresco |
| `digits[4]` | `volatile uint8_t[4]` | `update_digits()` (IDLE) | Millar, centena, decena, unidad |
| `currentdigit` | `volatile uint8_t` | `mostrardigitos()` (REFRESH) | Dígito 0..3 que toca encender; da la vuelta tras 3 |
| `current_state` | `static State_t` | FSM | Estado actual |
| `state_entry_flag` | `static uint8_t` | FSM | Hace que la acción **Entry** corra una vez por visita al estado |

## 6. Traza de ejemplo

| Pasada | Evento leído | Estado al entrar | Acción | Estado al salir |
|---|---|---|---|---|
| 1 | `EV_NONE` | IDLE | `update_digits()` | IDLE |
| 2 | `EV_F1_RISING` | IDLE | `update_digits()` | COUNT_UP |
| 3 | `EV_NONE` | COUNT_UP | `counter_exti++` | IDLE |
| 4 | `EV_TIM3_TICK` | IDLE | `update_digits()` | REFRESH |
| 5 | `EV_NONE` | REFRESH | `mostrardigitos()` | IDLE |

El contador se refleja en los dígitos en la pasada siguiente, cuando `IDLE` ejecuta `update_digits()`.

## 7. Puntos de atención

1. **Eventos que se pierden en estados transitorios.** `get_next_event()` se llama (y limpia la bandera) al inicio de **cada** pasada, pero `COUNT_DOWN`, `COUNT_UP` y `REFRESH` ignoran `current_event`. Si una ISR levanta una bandera durante esa pasada, el evento se consume y se descarta: un conteo no contado o un refresco de dígito omitido. Es poco probable, pero posible.
2. **Sin sección crítica.** Se quitaron `__disable_irq()/__enable_irq()` de `get_next_event()` a petición. Una ISR podría escribir la bandera entre la lectura y el borrado y perder ese evento.
3. **`state_entry_flag` es redundante** aquí: los estados transitorios duran una pasada y `IDLE` no tiene acción de entrada. Se mantiene por ser la forma Entry/Do/Exit del PDF; puede eliminarse sin cambiar el comportamiento.
4. **Código heredado sin corregir** (por indicación): `&&` en lugar de `&` en las ISR de TIM2/TIM3/EXTI, `counter_exti` sin signo (da la vuelta al restar desde 0) y comentario de `TIM2->ARR` que no coincide con el valor.
