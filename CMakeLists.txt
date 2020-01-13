cmake_minimum_required(VERSION 3.5)
project(Distributed)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -pedantic")

include_directories(headers)
include_directories(headers/pa1_starter_code)
include_directories(headers/pa2345_starter_code)

link_directories(library)
link_libraries(runtime)

foreach (LV 2 3 vt)
    set(SOURCE_FILES${LV} src/bank_robbery.c)
endforeach (LV)

foreach (LV 4)
    set(SOURCE_FILES${LV} src/queue.c)
endforeach (LV)

foreach (LV 5)
    set(SOURCE_FILES${LV} src/phil.c)
endforeach (LV)

foreach (LV 1 2 3 4 5 vt)
    set(SOURCE_FILES${LV} ${SOURCE_FILES${LV}} src/functions.c src/${LV}lab.c)
    add_executable(${LV}lab ${SOURCE_FILES${LV}})
endforeach (LV)

foreach (LV vt)
    TARGET_COMPILE_DEFINITIONS(${LV}lab PUBLIC _VT)
endforeach (LV)

set(SOURCE_FILES_TEST src/test.c)
add_executable(myTest ${SOURCE_FILES_TEST})