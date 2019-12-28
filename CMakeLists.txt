cmake_minimum_required(VERSION 3.10.2)
project(thunder C CXX)

set(CMAKE_C_STANDARD 11)

include_directories(/usr/local/include)
link_directories(/usr/local/lib)

# list(APPEND CMAKE_PREFIX_PATH /usr/local)

add_definitions("-DDEBUG=1")

find_library(corefoundation_lib CoreFoundation)
find_library(cfnetwork_lib CFNetwork)
find_library(iokit IOKit)
find_library(usb_lib usb-1.0)
find_package( OpenCV REQUIRED )

add_executable(thunder thunder.c errors.c)
target_link_libraries(thunder ${cfnetwork_lib} ${corefoundation_lib} ${iokit})

add_executable(ps4-arrows ps4-arrows.c errors.c)
target_link_libraries(ps4-arrows ${cfnetwork_lib} ${corefoundation_lib} ${iokit})

add_executable(ps4 ps4.c errors.c)
target_link_libraries(ps4 ${cfnetwork_lib} ${corefoundation_lib} ${iokit})

add_executable(find find.c errors.c)
target_link_libraries(find usb-1.0)

set(CMAKE_CXX_STANDARD 11)

add_library(capture capture.cpp)
target_link_libraries(capture ${OpenCV_LIBS})

add_executable(cap-1 cap-1.cpp)
target_link_libraries(cap-1 ${OpenCV_LIBS})

set(CMAKE_C_STANDARD 11)

add_executable(capture-main capture-main.c)
target_link_libraries(capture-main capture)

