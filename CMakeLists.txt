cmake_minimum_required(VERSION 3.22)

# Setup Project
project(McROM C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# CREATE THE TARGET
add_executable(${CMAKE_PROJECT_NAME})

# Add the CubeMX subdirectory
add_subdirectory(cmake/stm32cubemx)

# Handle the ROM binary
set(ROM_BINARY "${PROJECT_SOURCE_DIR}/rom.bin")
set(ROM_OBJECT "${CMAKE_CURRENT_BINARY_DIR}/rom.o")

add_custom_command(
    OUTPUT "${ROM_OBJECT}"
    COMMAND arm-none-eabi-objcopy -I binary -O elf32-littlearm -B arm 
            --rename-section .data=.6502_rom_section 
            "${ROM_BINARY}" "${ROM_OBJECT}"
    DEPENDS "${ROM_BINARY}"
    VERBATIM
)

# Add custom objects and libraries to the target
target_sources(${CMAKE_PROJECT_NAME} PRIVATE "${ROM_OBJECT}")

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    "$ENV{HOME}/Dev/Dev-STM/ms_stm32lib/Inc"
)
