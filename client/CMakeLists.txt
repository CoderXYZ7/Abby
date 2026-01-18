cmake_minimum_required(VERSION 3.10)
project(abby-client)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(abby-client SHARED
    src/AbbyClient.cpp
)

target_include_directories(abby-client PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Install targets
install(TARGETS abby-client LIBRARY DESTINATION lib)
install(FILES 
    include/AbbyClient.hpp
    include/abby_client.h
    DESTINATION include/abby
)
