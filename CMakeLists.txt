cmake_minimum_required(VERSION 3.6)
project(1lab)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -pedantic")

include_directories(headers)
include_directories(headers/pa1_starter_code)

set(SOURCE_FILES src/main.c)
add_executable(1lab ${SOURCE_FILES})

set(SOURCE_FILES_TEST src/test.c)
add_executable(myTest ${SOURCE_FILES_TEST})