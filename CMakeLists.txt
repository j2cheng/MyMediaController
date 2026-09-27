cmake_minimum_required(VERSION 3.14)
project(newcsio LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED gstreamer-1.0)

add_library(newcsio_core
    src/CommandQueue.cpp
    src/Watchdog.cpp
    src/MediaSession.cpp
    src/StreamController.cpp
    src/RtspStreamInterface.cpp)

target_include_directories(newcsio_core
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${GST_INCLUDE_DIRS})

target_link_libraries(newcsio_core PRIVATE ${GST_LIBRARIES})
target_compile_options(newcsio_core PRIVATE ${GST_CFLAGS_OTHER})

find_package(Threads REQUIRED)
target_link_libraries(newcsio_core PUBLIC Threads::Threads)

add_executable(newcsio_demo src/main.cpp)
target_link_libraries(newcsio_demo PRIVATE newcsio_core)
