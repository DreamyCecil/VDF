cmake_minimum_required(VERSION 3.1)
project(vdf)

set(CMAKE_C_STANDARD 90)

if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall)
endif()

add_library(vdf STATIC keyvalues.c)
