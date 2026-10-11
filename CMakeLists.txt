cmake_minimum_required(VERSION 3.16)
project(chip8 CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL2 REQUIRED)

add_executable(chip8 src/main.cpp src/chip8.cpp)
target_link_libraries(chip8 PRIVATE SDL2::SDL2)

if(TARGET SDL2::SDL2main)
    target_link_libraries(chip8 PRIVATE SDL2::SDL2main)
endif()

if(MSVC)
    target_compile_options(chip8 PRIVATE /W4)
else()
    target_compile_options(chip8 PRIVATE -Wall -Wextra)
endif()
target_compile_definitions(chip8 PRIVATE CHIP8_ROM_DIR="${CMAKE_SOURCE_DIR}/roms")