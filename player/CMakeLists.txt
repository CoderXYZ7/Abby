cmake_minimum_required(VERSION 3.10)
project(AbbyPlayer VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Strip symbols in Release mode
if(CMAKE_BUILD_TYPE MATCHES "Release")
    add_link_options("-s")
endif()

add_subdirectory(../crypt ${CMAKE_CURRENT_BINARY_DIR}/AbbyCrypt)
add_subdirectory(../client ${CMAKE_CURRENT_BINARY_DIR}/abby-client)

include_directories(include)
include_directories(../crypt/include)
include_directories(../client/include)

add_executable(AbbyPlayer
    src/main.cpp
    src/AudioPlayer.cpp
    src/FrequencyAnalyzer.cpp
    src/ShaderVisualizer.cpp
    src/ResourceManager.cpp
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)

# Link libraries
target_link_libraries(AbbyPlayer 
    AbbyCrypt
    abby-client
    ${SDL2_LIBRARIES}
    OpenGL::GL
    GLEW::GLEW
    pthread
    dl
)

target_include_directories(AbbyPlayer PRIVATE 
    ${SDL2_INCLUDE_DIRS}
)

add_executable(encrypt_util
    src/encrypt_util.cpp
)

target_link_libraries(encrypt_util 
    AbbyCrypt
    pthread
    dl
)

target_include_directories(encrypt_util PRIVATE 
    include
)
