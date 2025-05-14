cmake_minimum_required(VERSION 3.1)
project(samples)

option(VDF_MANAGE_MEMORY "Allow specifying custom functions for memory management" OFF)

if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall)
endif()

if(VDF_MANAGE_MEMORY)
  add_definitions("-DVDF_MANAGE_MEMORY=1")
endif()

macro(add_vdf_sample _NAME)
  add_executable("${_NAME}.out" "${_NAME}.c" "../keyvalues.c")
endmacro()

add_vdf_sample(access)
add_vdf_sample(contexts)
add_vdf_sample(iteration)
add_vdf_sample(reading)
add_vdf_sample(writing)
