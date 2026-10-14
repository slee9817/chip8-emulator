cmake_minimum_required(VERSION 3.16)
project(chip8 CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(chip8 src/main.cpp src/chip8.cpp)

if(EMSCRIPTEN)
    # ---------------------------------------------------------------
    # Web build (Emscripten -> WebAssembly)
    #   emcmake cmake -B build-web -G Ninja -DCMAKE_BUILD_TYPE=Release
    #   cmake --build build-web
    # ---------------------------------------------------------------
    target_compile_options(chip8 PRIVATE --use-port=sdl2)
    target_link_options(chip8 PRIVATE
        --use-port=sdl2
        -sASYNCIFY                        # lets the normal while-loop + SDL_Delay run in a browser
        -sASYNCIFY_STACK_SIZE=65536
        -sSTACK_SIZE=4MB                  # the Chip8 object (64 KB memory) lives on the stack
        -sALLOW_MEMORY_GROWTH
        -sEXPORTED_RUNTIME_METHODS=ccall  # lets the web page call load_rom()
        "SHELL:--preload-file ${CMAKE_SOURCE_DIR}/web/roms@/roms"
        "SHELL:--shell-file ${CMAKE_SOURCE_DIR}/web/shell.html")
    set_target_properties(chip8 PROPERTIES OUTPUT_NAME "index" SUFFIX ".html")
    target_compile_definitions(chip8 PRIVATE CHIP8_ROM_DIR="/roms")
else()
    # ---------------------------------------------------------------
    # Desktop build (Visual Studio / SDL2 from vcpkg)
    # ---------------------------------------------------------------
    find_package(SDL2 REQUIRED)
    target_link_libraries(chip8 PRIVATE SDL2::SDL2)
    if(TARGET SDL2::SDL2main)
        target_link_libraries(chip8 PRIVATE SDL2::SDL2main)
    endif()
    target_compile_definitions(chip8 PRIVATE CHIP8_ROM_DIR="${CMAKE_SOURCE_DIR}/roms")

    if(MSVC)
        target_compile_options(chip8 PRIVATE /W4)
    else()
        target_compile_options(chip8 PRIVATE -Wall -Wextra)
    endif()
endif()
