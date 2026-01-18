cmake_minimum_required(VERSION 3.10)
project(AbbyCrypt VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include_directories(include)

find_package(PkgConfig REQUIRED)
pkg_check_modules(OPENSSL REQUIRED openssl)

add_library(AbbyCrypt STATIC
    src/CryptoEngine.cpp
    src/FileHandler.cpp
    src/HardwareID.cpp
    src/AbbyCrypt.cpp
)

target_link_libraries(AbbyCrypt 
    ${OPENSSL_LIBRARIES}
    pthread
    dl
)

target_include_directories(AbbyCrypt PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    ${OPENSSL_INCLUDE_DIRS}
)
