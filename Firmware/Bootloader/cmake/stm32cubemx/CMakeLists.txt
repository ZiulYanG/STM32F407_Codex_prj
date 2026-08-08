cmake_minimum_required(VERSION 3.22)

project(stm32cubemx)
add_library(stm32cubemx INTERFACE)

# Enable CMake support for ASM and C languages
enable_language(C ASM)

target_compile_definitions(stm32cubemx INTERFACE 
	USE_HAL_DRIVER 
	STM32F407xx
    $<$<CONFIG:Debug>:DEBUG>
)

target_include_directories(stm32cubemx INTERFACE
    ../../Core/Inc
    ../../Drivers/STM32F4xx_HAL_Driver/Inc
    ../../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy
    ../../Middlewares/Third_Party/FreeRTOS/Source/include
    ../../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2
    ../../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F
    ../../Drivers/CMSIS/Device/ST/STM32F4xx/Include
    ../../Drivers/CMSIS/Include
)

target_sources(stm32cubemx INTERFACE
    D:/ZzlWorkDir/STM32/STM32F407/Codex_STM32F407/Firmware/Bootloader/Core/Src/main.c
    D:/ZzlWorkDir/STM32/STM32F407/Codex_STM32F407/Firmware/Bootloader/Core/Src/gpio.c
    D:/ZzlWorkDir/STM32/STM32F407/Codex_STM32F407/Firmware/Bootloader/Core/Src/freertos.c
    D:/ZzlWorkDir/STM32/STM32F407/Codex_STM32F407/Firmware/Bootloader/Core/Src/sdio.c
    D:/ZzlWorkDir/STM32/STM32F407/Codex_STM32F407/Firmware/Bootloader/Core/Src/spi.c
    D:/ZzlWorkDir/STM32/STM32F407/Codex_STM32F407/Firmware/Bootloader/Core/Src/usart.c
    D:/ZzlWorkDir/STM32/STM32F407/Codex_STM32F407/Firmware/Bootloader/Core/Src/usb_otg.c
    D:/ZzlWorkDir/STM32/STM32F407/Codex_STM32F407/Firmware/Bootloader/Core/Src/stm32f4xx_it.c
    D:/ZzlWorkDir/STM32/STM32F407/Codex_STM32F407/Firmware/Bootloader/Core/Src/stm32f4xx_hal_msp.c
    D:/ZzlWorkDir/STM32/STM32F407/Codex_STM32F407/Firmware/Bootloader/Core/Src/main.c
    D:/ZzlWorkDir/STM32/STM32F407/Codex_STM32F407/Firmware/Bootloader/Core/Src/gpio.c
    D:/ZzlWorkDir/STM32/STM32F407/Codex_STM32F407/Firmware/Bootloader/Core/Src/freertos.c
    D:/ZzlWorkDir/STM32/STM32F407/Codex_STM32F407/Firmware/Bootloader/Core/Src/sdio.c
    D:/ZzlWorkDir/STM32/STM32F407/Codex_STM32F407/Firmware/Bootloader/Core/Src/spi.c
    D:/ZzlWorkDir/STM32/STM32F407/Codex_STM32F407/Firmware/Bootloader/Core/Src/usart.c
    D:/ZzlWorkDir/STM32/STM32F407/Codex_STM32F407/Firmware/Bootloader/Core/Src/usb_otg.c
    D:/ZzlWorkDir/STM32/STM32F407/Codex_STM32F407/Firmware/Bootloader/Core/Src/stm32f4xx_it.c
    D:/ZzlWorkDir/STM32/STM32F407/Codex_STM32F407/Firmware/Bootloader/Core/Src/stm32f4xx_hal_msp.c
    ../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc.c
    ../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc_ex.c
    ../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash.c
    ../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ex.c
    ../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ramfunc.c
    ../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c
    ../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma_ex.c
    ../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma.c
    ../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr.c
    ../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr_ex.c
    ../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_cortex.c
    ../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c
    ../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_exti.c
    ../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_sdmmc.c
    ../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_sd.c
    ../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_spi.c
    ../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim.c
    ../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim_ex.c
    ../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_uart.c
    ../../Core/Src/system_stm32f4xx.c
    ../../Middlewares/Third_Party/FreeRTOS/Source/croutine.c
    ../../Middlewares/Third_Party/FreeRTOS/Source/event_groups.c
    ../../Middlewares/Third_Party/FreeRTOS/Source/list.c
    ../../Middlewares/Third_Party/FreeRTOS/Source/queue.c
    ../../Middlewares/Third_Party/FreeRTOS/Source/stream_buffer.c
    ../../Middlewares/Third_Party/FreeRTOS/Source/tasks.c
    ../../Middlewares/Third_Party/FreeRTOS/Source/timers.c
    ../../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2/cmsis_os2.c
    ../../Middlewares/Third_Party/FreeRTOS/Source/portable/MemMang/heap_4.c
    ../../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F/port.c
    D:/ZzlWorkDir/STM32/STM32F407/Codex_STM32F407/Firmware/Bootloader/Core/Src/sysmem.c
    D:/ZzlWorkDir/STM32/STM32F407/Codex_STM32F407/Firmware/Bootloader/Core/Src/syscalls.c
    D:/ZzlWorkDir/STM32/STM32F407/Codex_STM32F407/Firmware/Bootloader/Core/Src/sysmem.c
    D:/ZzlWorkDir/STM32/STM32F407/Codex_STM32F407/Firmware/Bootloader/Core/Src/syscalls.c
    ../../startup_stm32f407xx.s
)

target_link_directories(stm32cubemx INTERFACE
)

target_link_libraries(stm32cubemx INTERFACE
)

# Validate that STM32CubeMX code is compatible with C standard
if(CMAKE_C_STANDARD LESS 11)
    message(ERROR "Generated code requires C11 or higher")
endif()


