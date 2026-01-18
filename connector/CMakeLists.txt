cmake_minimum_required(VERSION 3.10)
project(AbbyConnector)

set(CMAKE_CXX_STANDARD 17)

# Dependencies
find_package(OpenSSL REQUIRED)
find_package(PkgConfig REQUIRED)

# Includes
include_directories(
    include
    ../../libs/abby-client/include
    ${OPENSSL_INCLUDE_DIR}
)

# Source files
file(GLOB SOURCES "src/*.cpp")

# JSON library (header only)
# We need to ensure nlohmann/json is available or included. 
# For now, assuming standard include path or we'll fetch it.
# Actually, let's just Assume we might need to download it or it is in the system.
# Checking if nlohmann/json is installed usually works via pkg-config or find_package if installed.
find_package(nlohmann_json QUIET)

# Executable
add_executable(AbbyConnector ${SOURCES})

# Link libraries
target_link_libraries(AbbyConnector
    PRIVATE
    OpenSSL::SSL
    OpenSSL::Crypto
    pthread
    # Link against the shared library we built earlier
    ${CMAKE_SOURCE_DIR}/../../libs/abby-client/build/libabby-client.so 
)

if(nlohmann_json_FOUND)
    target_link_libraries(AbbyConnector PRIVATE nlohmann_json::nlohmann_json)
endif()
