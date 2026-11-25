# Set the minimum version of CMake that can be used
cmake_minimum_required(VERSION 3.20)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)
set(CMAKE_C_COMPILER_FORCED TRUE)
set(CMAKE_CXX_COMPILER_FORCED TRUE)

# Set target definition variables required by the toolchain file
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/target.cmake)

# Set the toolchain file for cross-compilation
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/gcc-arm-none-eabi.cmake)

# Project definition
project(UART_DMA)

# Path to the STM32Cube firmware package (adjust the version if it changes)
set(CMSIS_ROOT "$ENV{HOME}/STM32Cube/Repository/STM32Cube_FW_F4_V1.28.3")

# Enable CMake support for ASM and C/CPP languages
enable_language(C ASM)

set(PROJECT_NAME UART_DMA)

# Create an executable object type
add_executable(${PROJECT_NAME})

# Include additional CMake files for flags, components, and files
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/flags.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/components.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/files.cmake)

# Add sources to executable
target_sources(${PROJECT_NAME} PRIVATE
    # Project files (main.c, startup, syscall.c and sysmem.c are already
    # listed by the extension in cmake/files.cmake — do not repeat them)
    ${CMAKE_SOURCE_DIR}/Core/Src/stm32f4xx_it.c
    ${CMAKE_SOURCE_DIR}/Core/Src/system_stm32f4xx.c

    # HAL drivers — one line per file copied in Step 4
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim_ex.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_cortex.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_uart.c
)

# Add include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/Core/Inc                             # stm32f4xx_hal_conf.h
    ${CMAKE_SOURCE_DIR}/Core/common                          # app.h, system.h, heartbeat.h, ...
    ${CMSIS_ROOT}/Drivers/STM32F4xx_HAL_Driver/Inc           # HAL headers   ← new
    ${CMSIS_ROOT}/Drivers/CMSIS/Core/Include                 # core_cm4.h
    ${CMSIS_ROOT}/Drivers/CMSIS/Device/ST/STM32F4xx/Include  # stm32f4xx.h
)

# Add additional defined symbols
target_compile_definitions(${PROJECT_NAME} PRIVATE
    STM32F411xE
)

# ---- Application selector ---------------------------------------------
# One fixed main.c, several applications: APP decides which app.c is compiled.
# Each new stage adds its name to the STRINGS list and one APP_DIR_<name> line.
set(APP "blinky" CACHE STRING "Application to build")
set_property(CACHE APP PROPERTY STRINGS blinky polling interrupt dma_idle pingpong)
set(APP_DIR_blinky    ${CMAKE_SOURCE_DIR}/Core/app_0_blinky)
set(APP_DIR_polling   ${CMAKE_SOURCE_DIR}/Core/app_1_polling)
set(APP_DIR_interrupt ${CMAKE_SOURCE_DIR}/Core/app_2_interrupt)
set(APP_DIR_dma_idle  ${CMAKE_SOURCE_DIR}/Core/app_3_dma_idle)
set(APP_DIR_pingpong  ${CMAKE_SOURCE_DIR}/Core/app_4_pingpong)
if(NOT DEFINED APP_DIR_${APP})
    message(FATAL_ERROR "Unknown APP='${APP}': there is no APP_DIR_${APP} line in CMakeLists.txt")
endif()
target_sources(${PROJECT_NAME} PRIVATE ${APP_DIR_${APP}}/app.c)
message(STATUS ">>> Building APP = ${APP}")

# ---- Common modules (shared by every stage) -----------------------------
target_sources(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/Core/common/system.c
    ${CMAKE_SOURCE_DIR}/Core/common/heartbeat.c
    ${CMAKE_SOURCE_DIR}/Core/common/cmd_parser.c
    ${CMAKE_SOURCE_DIR}/Core/common/commands.c
    ${CMAKE_SOURCE_DIR}/Core/common/pwm.c
)
