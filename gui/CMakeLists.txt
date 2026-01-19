cmake_minimum_required(VERSION 3.10)
project(abby-gui VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Add AbbyCrypt library
add_subdirectory(../crypt ${CMAKE_CURRENT_BINARY_DIR}/AbbyCrypt)

# Add abby-client library
add_subdirectory(../client ${CMAKE_CURRENT_BINARY_DIR}/abby-client)

# Find FLTK
find_package(FLTK REQUIRED)

add_executable(abby-gui abby-gui-fltk.cpp)

target_link_libraries(abby-gui 
    AbbyCrypt
    abby-client
    ${FLTK_LIBRARIES}
    pthread
)

target_include_directories(abby-gui PRIVATE 
    ../crypt/include
    ../client/include
    ${FLTK_INCLUDE_DIRS}
)
