cmake_minimum_required(VERSION 3.16)
project(rng_bench CXX)

option(RNG_BENCH_ENABLE_MARCH_NATIVE "Enable -march=native" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (MSVC)
  add_compile_options(/O2 /DNOMINMAX)
else()
  add_compile_options(-O3 -fno-exceptions -fno-rtti)
  if (RNG_BENCH_ENABLE_MARCH_NATIVE)
    add_compile_options(-march=native)
  endif()
endif()

add_executable(rng_bench
  src/main.cpp
)

target_include_directories(rng_bench PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

if (WIN32)
  target_link_libraries(rng_bench PRIVATE ws2_32)
endif()
