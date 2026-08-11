cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME Application)

# Include toolchain file
include("cmake/gcc-arm-none-eabi.cmake")

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME} C ASM)
message("Build type: " ${CMAKE_BUILD_TYPE})

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Relink when the protected APP memory layout changes. The actual -T option is
# set by the toolchain file so it is also present during CMake compiler checks.
set(APPLICATION_LINKER_SCRIPT
    "${CMAKE_CURRENT_SOURCE_DIR}/linker/STM32F407ZGTx_APP_FLASH.ld"
)
set_property(
    TARGET ${CMAKE_PROJECT_NAME}
    APPEND PROPERTY LINK_DEPENDS "${APPLICATION_LINKER_SCRIPT}"
)

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    app/app_main.c
    app/app_console.c
    app/system_mode.c
    bsp/bsp_board.c
    bsp/bsp_i2c1.c
    bsp/bsp_spi1.c
    bsp/bsp_uart1.c
    components/drivers/eeprom_24c02.c
    components/drivers/spi_nor.c
    components/logging/app_log.c
    components/storage/storage.c
    components/storage/storage_eeprom_24c02.c
    components/storage/storage_partition.c
    components/update/ymodem_storage.c
    components/update/update_session.c
    components/storage/storage_spi_nor.c
    ../Common/serial/serial_manager.c
    ../Common/console/command_console.c
    ../Common/ymodem/ymodem.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    app
    bsp
    components/drivers
    components/logging
    components/storage
    components/update
    ../Common/ymodem
    ../Common/serial
    ../Common/console
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    USER_VECT_TAB_ADDRESS
    VECT_TAB_OFFSET=0x00040000U
    $<$<CONFIG:Debug>:APP_ENABLE_SPI_NOR_SELF_TEST=1>
    $<$<NOT:$<CONFIG:Debug>>:APP_ENABLE_SPI_NOR_SELF_TEST=0>
    $<$<CONFIG:Debug>:APP_ENABLE_EEPROM_SELF_TEST=1>
    $<$<NOT:$<CONFIG:Debug>>:APP_ENABLE_EEPROM_SELF_TEST=0>
)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)
