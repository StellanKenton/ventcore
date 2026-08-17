cmake_minimum_required(VERSION 3.20)

project(ventcore LANGUAGES C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_EXECUTABLE_SUFFIX ".elf")

set(MCU_FLAGS
    -mcpu=cortex-m4
    -mthumb
    -mfpu=fpv4-sp-d16
    -mfloat-abi=hard)

set(FREERTOS_SOURCES
    Firmware/FreeRTOS/croutine.c
    Firmware/FreeRTOS/event_groups.c
    Firmware/FreeRTOS/list.c
    Firmware/FreeRTOS/queue.c
    Firmware/FreeRTOS/stream_buffer.c
    Firmware/FreeRTOS/tasks.c
    Firmware/FreeRTOS/timers.c
    Firmware/FreeRTOS/portable/GCC/ARM_CM4F/port.c
    Firmware/FreeRTOS/portable/MemMang/heap_4.c)

add_executable(ventcore
    User/main.c
    User/app/taskmanager.c
    User/module/rtos/rtos.c
    User/module/rtos/portrtos.c
    Firmware/CMSIS/GD/GD32F4xx/Source/system_gd32f4xx.c
    Firmware/CMSIS/GD/GD32F4xx/Source/GCC/startup_gd32f470zetx.s
    ${FREERTOS_SOURCES})

target_include_directories(ventcore PRIVATE
    User
    User/app
    User/module/rtos
    Firmware/CMSIS
    Firmware/CMSIS/GD/GD32F4xx/Include
    Firmware/FreeRTOS/include
    Firmware/FreeRTOS/portable/GCC/ARM_CM4F)

target_compile_definitions(ventcore PRIVATE
    GD32F470)

target_compile_options(ventcore PRIVATE
    ${MCU_FLAGS}
    -ffunction-sections
    -fdata-sections
    -fno-common
    -Wall
    -Wextra
    -Wno-unused-parameter)

target_link_options(ventcore PRIVATE
    ${MCU_FLAGS}
    -T${CMAKE_CURRENT_SOURCE_DIR}/Firmware/Ld/GD32F470ZETX_FALSH.ld
    --specs=nano.specs
    --specs=nosys.specs
    -Wl,--gc-sections
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/ventcore.map
    -Wl,--print-memory-usage)

add_custom_command(TARGET ventcore POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:ventcore> ${CMAKE_CURRENT_BINARY_DIR}/ventcore.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:ventcore> ${CMAKE_CURRENT_BINARY_DIR}/ventcore.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:ventcore>
    COMMENT "Generating HEX/BIN images and reporting firmware size")
