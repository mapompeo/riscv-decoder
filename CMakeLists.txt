cmake_minimum_required(VERSION 3.16)
project(riscv_decoder LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# No MinGW, linkar a runtime estaticamente evita depender de DLLs do compilador
# (o executavel roda em qualquer maquina Windows sem instalar nada).
if(MINGW)
  add_link_options(-static -static-libgcc -static-libstdc++)
endif()

add_library(riscv_core
  src/parser.cpp
  src/decoder.cpp
  src/disassembler.cpp
  src/statistics.cpp
)
target_include_directories(riscv_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(riscv-decoder src/main.cpp)
target_link_libraries(riscv-decoder PRIVATE riscv_core)

enable_testing()
add_executable(test_decoder tests/test_decoder.cpp)
target_link_libraries(test_decoder PRIVATE riscv_core)
add_test(NAME testes_decodificador COMMAND test_decoder)
