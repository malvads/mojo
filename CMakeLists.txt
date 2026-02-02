cmake_minimum_required(VERSION 3.10)

file(READ "VERSION" VER)
string(STRIP "${VER}" VER)
project(mojo VERSION ${VER})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Dependencies
find_package(CURL REQUIRED)
find_library(GUMBO_LIBRARY NAMES gumbo gumbo-parser REQUIRED)
find_path(GUMBO_INCLUDE_DIR NAMES gumbo.h REQUIRED)
find_package(Threads REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(yaml-cpp REQUIRED)
find_package(CLI11 REQUIRED)
find_library(WEBSOCKETS_LIBRARY NAMES websockets libwebsockets REQUIRED)

# Include Directories
include_directories(
    include
    include/vendor
    ${GUMBO_INCLUDE_DIR}
    ${CURL_INCLUDE_DIRS}
)

# Source Files
file(GLOB_RECURSE SOURCES "src/*.cpp")

# Executable
add_executable(mojo ${SOURCES})

# Handle yaml-cpp target differences across versions/OSs
if(TARGET yaml-cpp::yaml-cpp)
    set(YAML_CPP_LIB yaml-cpp::yaml-cpp)
elseif(TARGET yaml-cpp)
    set(YAML_CPP_LIB yaml-cpp)
else()
    set(YAML_CPP_LIB ${YAML_CPP_LIBRARIES})
endif()

# Linking Strategy
option(MOJO_STATIC_BUILD "Build with static linking for portability (Linux only)" OFF)

if(UNIX AND NOT APPLE AND MOJO_STATIC_BUILD)
    set(CMAKE_FIND_LIBRARY_SUFFIXES .a ${CMAKE_FIND_LIBRARY_SUFFIXES})

    find_package(OpenSSL REQUIRED)
    find_library(GUMBO_LIB NAMES libgumbo.a gumbo REQUIRED)
    find_library(YAML_LIB NAMES libyaml-cpp.a yaml-cpp REQUIRED)
    find_library(WEBSOCKETS_LIB NAMES libwebsockets.a websockets REQUIRED)
    
    find_library(CAP_LIB NAMES libcap.a cap)
    find_library(UV_LIB NAMES libuv.a uv)
    find_library(EV_LIB NAMES libev.a ev)
    find_library(Z_LIB NAMES libz.a z)

    target_link_libraries(mojo PRIVATE 
        CURL::libcurl
        ${GUMBO_LIB}
        ${WEBSOCKETS_LIB}
        OpenSSL::SSL
        OpenSSL::Crypto
        ${CAP_LIB} ${UV_LIB} ${EV_LIB} ${Z_LIB}
        nlohmann_json::nlohmann_json
        ${YAML_LIB}
        CLI11::CLI11
        Threads::Threads
    )
else()
    # Standard linking for development/macOS/Windows
    target_link_libraries(mojo PRIVATE 
        CURL::libcurl
        ${GUMBO_LIBRARY}
        ${WEBSOCKETS_LIBRARY}
        nlohmann_json::nlohmann_json
        ${YAML_CPP_LIB}
        CLI11::CLI11
        Threads::Threads
    )
endif()

# Installation
install(TARGETS mojo DESTINATION bin)

# Packaging (CPack)
set(CPACK_PACKAGE_NAME "mojo")
set(CPACK_PACKAGE_VENDOR "Miguel Alvarez")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "High-performance web crawler for AI and LLM data ingestion")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_CONTACT "Miguel Alvarez <mialvare@redhat.com>")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")

# Debian (.deb) Specifics
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Miguel Alvarez <mialvare@redhat.com>")
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS OFF)
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.17), libcurl4")

# Fedora (.rpm) Specifics
set(CPACK_RPM_PACKAGE_LICENSE "MIT")
set(CPACK_RPM_PACKAGE_AUTOREQ OFF)
set(CPACK_RPM_PACKAGE_REQUIRES "libcurl")

# Inclusion
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${PROJECT_VERSION}-${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_DEBIAN_FILE_NAME "mojo-${PROJECT_VERSION}-debian.deb")
set(CPACK_RPM_FILE_NAME "mojo-${PROJECT_VERSION}-rhel.rpm")

include(CPack)
