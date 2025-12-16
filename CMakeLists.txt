cmake_minimum_required(VERSION 3.16)

project(DroneSwarm LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wnon-virtual-dtor
    -Wimplicit-fallthrough
    -Werror
)
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-Wlifetime)
endif()
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(
        -fsanitize=address,undefined
        -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

file(GLOB_RECURSE COMMON_SRC src/common/*.cpp)
add_library(common STATIC ${COMMON_SRC})
target_include_directories(common PUBLIC src/common)

file(GLOB_RECURSE APP1_SRC src/main/*.cpp)
add_executable(DroneSwarm ${APP1_SRC})
target_link_libraries(DroneSwarm PRIVATE common)

file(GLOB_RECURSE APP2_SRC src/logger/*.cpp)
add_executable(logger ${APP2_SRC})
target_link_libraries(logger PRIVATE common)

