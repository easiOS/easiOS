cmake_minimum_required(VERSION 2.8.7)
project (easiOS)

include_directories(include)

ENABLE_LANGUAGE(ASM)

set(CMAKE_C_COMPILER i686-elf-gcc)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -D __EASIOS__ -g -std=gnu99 -ffreestanding -Wall -fdiagnostics-color=always -fstack-protector-all -Wno-unused-function")
set(CMAKE_CXX_COMPILER i686-elf-g++)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -g -ffreestanding -Wall -fdiagnostics-color=always -fstack-protector-all -fno-exceptions -fno-rtti")

set(CMAKE_ASM_COMPILE_OBJECT "nasm -felf32 -o <OBJECT> <SOURCE>")

set(CMAKE_C_LINK_EXECUTABLE "i686-elf-gcc -T ${CMAKE_SOURCE_DIR}/linker.ld -o <TARGET> -ffreestanding -O2 -nostdlib <OBJECTS>")
set(CMAKE_CXX_LINK_EXECUTABLE "i686-elf-g++ -T ${CMAKE_SOURCE_DIR}/linker.ld -o <TARGET> -ffreestanding -O2 -nostdlib <OBJECTS>")

file(GLOB SOURCES_C_L1 "*/*.c")
file(GLOB SOURCES_C_L2 "*/*/*.c")
file(GLOB SOURCES_A "kern/*.s")

set_source_files_properties(kern/fs/fat32.c PROPERTIES COMPILE_FLAGS -w)

add_executable(easios.bin ${SOURCES_A} ${SOURCES_C_L1} ${SOURCES_C_L2} ${SOURCES_CPP})
