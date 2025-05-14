cmake_minimum_required(VERSION 3.1)
project(vdf)

option(VDF_MANAGE_MEMORY "Allow specifying custom functions for memory management" OFF)

set(CMAKE_C_STANDARD 90)

if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall)
endif()

if(VDF_MANAGE_MEMORY)
  add_definitions("-DVDF_MANAGE_MEMORY=1")
endif()

add_library(vdf STATIC keyvalues.c)
