# Set minimum required version of CMake
cmake_minimum_required(VERSION 3.12)

# Include build functions from Pico SDK
include(C:\\Users\\dcoop\\AppData\\Local\\VisualGDB\\PicoSDK\\1.5.1-Package\\external\\pico_sdk_import.cmake)

# Set name of project (as PROJECT_NAME) and C/C   standards
project(blink C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

add_subdirectory(FatFs_SPI build)

# Creates a pico-sdk subdirectory in our project for the libraries
pico_sdk_init()

# Tell CMake where to find the executable source file
add_executable(${PROJECT_NAME} 
    main.cpp
    LEDController.cpp
    hardware.cpp
    videoFileReading.cpp
    ledControl.cpp
)

pico_generate_pio_header(${PROJECT_NAME}  ${CMAKE_CURRENT_LIST_DIR}/spi.pio)

# Create map/bin/hex/uf2 files
pico_add_extra_outputs(${PROJECT_NAME})

# Link to pico_stdlib (gpio, time, etc. functions)
target_link_libraries(${PROJECT_NAME}
    FatFs_SPI
    pico_stdlib
    hardware_gpio
    hardware_pio
    pico_multicore
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
  PICO_DEFAULT_UART=0
  PICO_DEFAULT_UART_TX_PIN=0
  PICO_DEFAULT_UART_RX_PIN=1
)

# Enable uart output, disable usb output
pico_enable_stdio_usb(${PROJECT_NAME} 0)
pico_enable_stdio_uart(${PROJECT_NAME} 1)