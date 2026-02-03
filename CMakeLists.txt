cmake_minimum_required(VERSION 3.10)
project(mojo VERSION 0.1.0)

if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()
if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
if(ENABLE_ASAN)
    message(STATUS "Enabling AddressSanitizer")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address,undefined -fno-omit-frame-pointer -g")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address,undefined")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=address,undefined")
endif()

find_package(Threads REQUIRED)
find_package(CURL REQUIRED)
find_package(Boost REQUIRED)
find_package(nlohmann_json REQUIRED)
include_directories(/opt/homebrew/include)
link_directories(/opt/homebrew/lib)

find_library(GUMBO_LIBRARY gumbo PATHS /opt/homebrew/lib /usr/local/lib)
find_package(yaml-cpp QUIET)
if (yaml-cpp_FOUND)
    if (TARGET yaml-cpp::yaml-cpp)
        set(YAML_CPP_LIBRARY yaml-cpp::yaml-cpp)
    elseif (TARGET yaml-cpp)
        set(YAML_CPP_LIBRARY yaml-cpp)
    endif()
else()
    find_library(YAML_CPP_LIBRARY yaml-cpp PATHS /opt/homebrew/lib /usr/local/lib /usr/lib /usr/lib64)
endif()

find_library(WEBSOCKETS_LIBRARY websockets PATHS /opt/homebrew/lib /usr/local/lib)

if(NOT GUMBO_LIBRARY)
    message(WARNING "Gumbo library not found!")
endif()

if(NOT YAML_CPP_LIBRARY)
    message(WARNING "yaml-cpp library not found!")
endif()

if(NOT WEBSOCKETS_LIBRARY)
    message(WARNING "websockets library not found!")
endif()

add_subdirectory(src/core)
add_subdirectory(src/utils)
add_subdirectory(src/network)
add_subdirectory(src/proxy)
add_subdirectory(src/browser)
add_subdirectory(src/engine)

enable_testing()
include(FetchContent)
FetchContent_Declare(
  cpp-httplib
  GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
  GIT_TAG        v0.14.3
)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
find_package(absl QUIET)
if(NOT absl_FOUND)
  message(STATUS "Abseil not found via find_package, using FetchContent...")
  FetchContent_Declare(
    abseil-cpp
    GIT_REPOSITORY https://github.com/abseil/abseil-cpp.git
    GIT_TAG        20240722.0
  )
  FetchContent_MakeAvailable(abseil-cpp)
endif()
FetchContent_Declare(
  robotstxt
  GIT_REPOSITORY https://github.com/google/robotstxt.git
  GIT_TAG        86d5836ba2d5a0b6b938ab49501be0e09d9c276c
)

FetchContent_MakeAvailable(googletest cpp-httplib)
FetchContent_GetProperties(robotstxt)
if(NOT robotstxt_POPULATED)
  FetchContent_Populate(robotstxt)
  add_library(robots STATIC
    ${robotstxt_SOURCE_DIR}/robots.cc
    ${robotstxt_SOURCE_DIR}/robots.h
  )
  target_include_directories(robots SYSTEM PUBLIC ${robotstxt_SOURCE_DIR})
  target_link_libraries(robots PUBLIC absl::strings absl::fixed_array absl::flat_hash_map)
  target_compile_options(robots PRIVATE -Wno-unused-parameter)
endif()

add_subdirectory(tests/integration)
add_subdirectory(tests/unit)

add_executable(mojo src/main.cpp)

target_link_libraries(mojo 
    PRIVATE 
    mojo_engine
    Threads::Threads
)
