# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Qué es este repositorio

Este repo es el material de un curso universitario de sistemas embebidos sobre STM32F411RE (placa NUCLEO-F411RE, núcleo ARM Cortex-M4). El contenido se organiza **por rama de git, una por semana** (`week-01` … `week-15`), no por carpetas dentro de `master`; la rama actual (`week-02`) corresponde a la semana 2.

En la raíz existe `AGENTS.md` (compilado a partir de `ai-config/RULES.md`, `KNOWLEDGE.md` y `CODESTYLE.md`), que configura una persona de **tutor de IA orientado al estudiante** para Copilot CLI. Esos archivos ya están cargados como instrucciones del sistema en esta sesión — no los repitas ni los resumas de nuevo, y ten en cuenta dos cosas:

- `ai-config/KNOWLEDGE.md` está *acoplado a la semana activa*: describe exactamente qué domina el estudiante y qué tema está viendo en la rama actual. Si te piden preparar contenido de otra semana, ese archivo debe actualizarse para reflejarlo (normalmente al cambiar de rama).
- La regla del `README.md` de "no modificar `AGENTS.md` ni `ai-config/`" es una regla **para los estudiantes que hacen fork del repo**, no para ti trabajando aquí como monitor: cuando el usuario (un monitor) te pida ajustar esos archivos para una nueva semana, sí debes editarlos.

`resources/EJERCICIOS_TAREA_semana00-03.md` y `HOMEWORK_EXERCISES_week00-03.md` son ejercicios de tarea que abarcan varias semanas y no están atados a una sola rama.

## Compilación

Cada proyecto de taller (p. ej. `TallerRegistros_F/`) es un proyecto **bare-metal CMSIS** generado con la extensión "STM32 for VS Code" — usa CMake + Ninja + `arm-none-eabi-gcc`, **no** es un proyecto nativo de STM32CubeIDE. No hay HAL hasta la semana 7 (ver `ai-config/CODESTYLE.md`); antes de eso todo el acceso a periféricos es por registros CMSIS.

Desde la carpeta del proyecto (ej. `TallerRegistros_F/`):

```bash
cmake --preset Debug          # o Release — configura en build/<preset>/
cmake --build --preset Debug  # compila con Ninja
```

No hay linter ni suite de pruebas automatizadas en este repo (es material docente, no una librería); los warnings de compilación (`-Wall -Wextra -Wpedantic`, definidos en `cmake/flags.cmake`) cumplen ese rol. Flashear y depurar se hace desde la UI de la extensión de VS Code (ST-Link), no por línea de comandos.

Las rutas de include de CMSIS en `CMakeLists.txt` apuntan a una instalación local absoluta de `STM32Cube_FW_F4` — cualquier proyecto nuevo generado por la extensión copiará ese mismo patrón de ruta absoluta; ajústala si cambia la instalación de STM32Cube.

## Patrón de cada taller semanal (arquitectura clave)

Cada taller sigue un patrón de **dos carpetas**, evidenciado por el par `TallerRegistros/` → `TallerRegistros_F/`:

- **`<Taller>/`** — el punto de partida que se entrega a los estudiantes: solo el scaffold del proyecto STM32-for-VS-Code (`Src/`, archivo de arranque `.S`, linker script, `CMakeLists.txt`/`CMakePresets.json`/`cmake/*.cmake`, `.vscode/`), sin guía ni código de solución.
- **`<Taller>_F/`** — la versión final/de referencia del mismo taller, que añade sobre el mismo scaffold:
  - El `main.c` resuelto.
  - Una guía paso a paso `GUIA_TALLER_*.md` en español para los estudiantes, siguiendo la estructura ya usada en `GUIA_TALLER_STM32_CMSIS.md`: objetivos → configuración del entorno → conexión de includes en CMake → chuleta de operaciones de bits → tabla de registros (con direcciones/offsets del RM0383) → ejercicios numerados con código incremental → `main.c` consolidado → checklist de errores comunes.
  - Una carpeta `web/` con un artifact interactivo estático (`index.html` + `style.css` + `script.js`, sin build ni dependencias) para que los monitores presenten el taller: incluye una placa NUCLEO simulada (botón/LED clicables), un inspector de registros en vivo bit a bit, y pestañas que reflejan las secciones de la guía.

Este par es la plantilla a replicar para cada semana nueva: arma el scaffold base + `_F`, escribe el `main.c` al nivel de acceso a registros que corresponda al alcance de `ai-config/KNOWLEDGE.md` de esa semana, escribe la guía, y construye la pieza de presentación interactiva (como carpeta `web/` siguiendo el estilo existente, o como un Artifact de Claude Code si así se pide).
