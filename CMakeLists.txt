cmake_minimum_required(VERSION 3.5)
project(Distributed)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -pedantic")

include_directories(headers)
include_directories(headers/pa1_starter_code)
include_directories(headers/pa2345_starter_code)

link_directories(library)
link_libraries(runtime)

set(SOURCE_FUNCTIONS src/functions.c)

foreach (LV 2 3 vt)
    set(SOURCE_FILES${LV} src/bank_robbery.c)
endforeach (LV)

foreach (LV 4)
    set(SOURCE_FILES${LV} src/queue.c)
endforeach (LV)

foreach (LV 1 2 3 4 5)
    set(SOURCE_FILES${LV} ${SOURCE_FILES${LV}} ${SOURCE_FUNCTIONS} src/${LV}lab.c)
    add_executable(${LV}lab ${SOURCE_FILES${LV}})
endforeach (LV)

foreach (LV vt)
    set(SOURCE_FILES${LV} ${SOURCE_FILES${LV}} src/functions_vt.c src/${LV}lab.c)
    add_executable(${LV}lab ${SOURCE_FILES${LV}})
endforeach (LV)

set(SOURCE_FILES_TEST src/test.c)
add_executable(myTest ${SOURCE_FILES_TEST})