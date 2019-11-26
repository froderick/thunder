cmake_minimum_required(VERSION 3.10.2)
project(thunder C)

set(CMAKE_C_STANDARD 11)

include_directories(/usr/local/include)
link_directories(/usr/local/lib)

add_definitions("-DDEBUG=1")

find_library(corefoundation_lib CoreFoundation)
find_library(cfnetwork_lib CFNetwork)
find_library(iokit IOKit)

add_executable(thunder thunder.c errors.c)
target_link_libraries(thunder ${cfnetwork_lib} ${corefoundation_lib} ${iokit})

add_executable(ps4-arrows ps4-arrows.c errors.c)
target_link_libraries(ps4-arrows ${cfnetwork_lib} ${corefoundation_lib} ${iokit})

add_executable(ps4 ps4.c errors.c)
target_link_libraries(ps4 ${cfnetwork_lib} ${corefoundation_lib} ${iokit})

