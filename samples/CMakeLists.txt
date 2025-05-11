cmake_minimum_required(VERSION 3.1)
project(samples)

if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall)
endif()

macro(add_vdf_sample _NAME)
  add_executable("${_NAME}.out" "${_NAME}.c" "../keyvalues.c")
endmacro()

add_vdf_sample(access)
add_vdf_sample(contexts)
add_vdf_sample(iteration)
add_vdf_sample(reading)
add_vdf_sample(writing)
