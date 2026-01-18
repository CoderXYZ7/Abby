cmake_minimum_required(VERSION 3.10)
project(AbbyConnector)

set(CMAKE_CXX_STANDARD 17)

# Dependencies
find_package(OpenSSL REQUIRED)
find_package(PkgConfig REQUIRED)

# Find libsystemd for sd-bus (BLE D-Bus support)
pkg_check_modules(SYSTEMD REQUIRED libsystemd)

# Includes
include_directories(
    include
    ../client/include
    ${OPENSSL_INCLUDE_DIR}
    ${SYSTEMD_INCLUDE_DIRS}
)

# Source files
set(SOURCES
    src/main.cpp
    src/JwtValidator.cpp
    src/ContentCatalog.cpp
    src/BleServer.cpp
    src/PlaylistManager.cpp
)

# JSON library (header only)
find_package(nlohmann_json QUIET)

# Executable
add_executable(AbbyConnector ${SOURCES})

# Link libraries
target_link_libraries(AbbyConnector
    PRIVATE
    OpenSSL::SSL
    OpenSSL::Crypto
    pthread
    ${SYSTEMD_LIBRARIES}
    # Link against the shared library
    ${CMAKE_SOURCE_DIR}/../client/build/libabby-client.so 
)

if(nlohmann_json_FOUND)
    target_link_libraries(AbbyConnector PRIVATE nlohmann_json::nlohmann_json)
endif()
