cmake_minimum_required(VERSION 3.13)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(generatorPico C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

message(STATUS "C Compiler: ${CMAKE_C_COMPILER}")

pico_sdk_init()

set(SOURCE_FILES
  generatorPico.c
)

add_executable(${PROJECT_NAME}
  ${SOURCE_FILES}
)

pico_add_extra_outputs(${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(${PROJECT_NAME} pico_stdlib hardware_adc)

pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 0)

