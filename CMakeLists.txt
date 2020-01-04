cmake_minimum_required(VERSION 3.10.2)
project(thunder C CXX)

set(CMAKE_C_STANDARD 11)

include_directories(/usr/local/include)
link_directories(/usr/local/lib)

# list(APPEND CMAKE_PREFIX_PATH /usr/local)

add_definitions("-DDEBUG=1")

find_library(foundation_lib Foundation)
find_library(corefoundation_lib CoreFoundation)
find_library(cfnetwork_lib CFNetwork)
find_library(avfoundation AVFoundation)
find_library(iokit IOKit)
find_library(appkit AppKit)
find_library(usb_lib usb-1.0)
find_package( OpenCV REQUIRED )

set(CMAKE_CXX_STANDARD 11)

add_library(capture-ps3eye ps3eye.cpp capture-ps3eye.cpp)
target_link_libraries(capture-ps3eye usb-1.0 ${OpenCV_LIBS})

set(CMAKE_C_STANDARD 11)

add_executable(core errors.c core.c controller.c launcher.c face-capture.c)
target_link_libraries(core ${cfnetwork_lib} ${corefoundation_lib} ${iokit} usb-1.0 ${OpenCV_LIBS} capture-ps3eye)

add_executable(test test.m)
target_link_libraries(test ${foundation_lib} ${appkit} ${avfoundation})

