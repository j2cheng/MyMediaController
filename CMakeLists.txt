cmake_minimum_required(VERSION 3.14)
project(newcsio VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED gstreamer-1.0)

# Shared library: libStreamController.so, built from the src/ sources.
add_library(StreamController SHARED
    src/CommandQueue.cpp
    src/Watchdog.cpp
    src/MediaSession.cpp
    src/StreamController.cpp
    src/RtspStreamInterface.cpp)

target_include_directories(StreamController
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${GST_INCLUDE_DIRS})

target_link_libraries(StreamController PRIVATE ${GST_LIBRARIES})
target_compile_options(StreamController PRIVATE ${GST_CFLAGS_OTHER})

find_package(Threads REQUIRED)
target_link_libraries(StreamController PUBLIC Threads::Threads)

# Unit tests (replaces the old main.cpp demo).
enable_testing()
add_executable(unit_test test/unit_test.cpp)
target_link_libraries(unit_test PRIVATE StreamController)
add_test(NAME unit_test COMMAND unit_test)

# --- Distribution package (libStreamController.so + public headers) ---------
include(GNUInstallDirs)

install(TARGETS StreamController
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

set(CPACK_PACKAGE_NAME "StreamController")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_FILE_NAME
    "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_NAME}")
include(CPack)
