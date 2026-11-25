# Tarea 3 — UART + DMA + comandos (NUCLEO-F411RE, HAL)

Proyecto `UART_DMA/`, hecho siguiendo la guía "UART + DMA + Commands" (etapas 0 a 5).
Parte de `HAL_Blinky` (HAL en `Drivers/`, `CMSIS_ROOT` en `$HOME/STM32Cube/Repository/STM32Cube_FW_F4_V1.28.3`).

Se elige la etapa con `-DAPP=<nombre>` en `.vscode/settings.json` (`cmake.configureArgs`) y luego
**CMake: Configure**. Revisar en la salida la línea `>>> Building APP = ...`.

| Etapa | APP         | Qué hace |
|-------|-------------|----------|
| 0     | `blinky`    | Latido en LD2 (TIM2), driver UART compilado, selector de app |
| 1     | `polling`   | `HAL_UART_Transmit` / `HAL_UART_Receive` bloqueantes |
| 2     | `interrupt` | RX por interrupción, anillo, líneas y comandos |
| 3     | `dma_idle`  | RX por DMA terminada por línea IDLE, `HAL_UART_MspInit` |
| 4     | `pingpong`  | Dos bloques RX, TX por DMA, comandos y `pwm` (etapa 5) |

Serial: `/dev/ttyACM0`, 115200 8N1. PWM de la etapa 5: PA6 (D12), TIM3 CH1, 1 kHz.
