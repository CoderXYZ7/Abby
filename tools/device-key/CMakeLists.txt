cmake_minimum_required(VERSION 3.10)
project(device-key)

set(CMAKE_CXX_STANDARD 17)

# Add AbbyCrypt library
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../device/AbbyCrypt 
                 ${CMAKE_CURRENT_BINARY_DIR}/AbbyCrypt)

add_executable(device-key device-key.cpp)

target_include_directories(device-key PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../device/AbbyCrypt/include
)

target_link_libraries(device-key 
    AbbyCrypt
)
