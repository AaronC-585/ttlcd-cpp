cmake_minimum_required(VERSION 3.20)
project(ttlcd VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUSB REQUIRED libusb-1.0)
pkg_check_modules(OPENCV REQUIRED opencv4)

add_executable(ttlcd
    main.cpp
    src/Widget.cpp
    src/Layout.cpp
    src/LCDController.cpp
    src/TempDir.cpp
)

target_include_directories(ttlcd PRIVATE
    include
    ${LIBUSB_INCLUDE_DIRS}
    ${OPENCV_INCLUDE_DIRS}
)

target_compile_options(ttlcd PRIVATE
    -O2 -Wall -Wextra -pedantic
    -Wno-deprecated-enum-enum-conversion
    ${LIBUSB_CFLAGS_OTHER}
    ${OPENCV_CFLAGS_OTHER}
)

target_link_libraries(ttlcd PRIVATE
    ${LIBUSB_LIBRARIES}
    ${OPENCV_LIBRARIES}
    pthread
)

install(TARGETS ttlcd DESTINATION bin)
