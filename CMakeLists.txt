cmake_minimum_required(VERSION 3.16)

project(DroneSwarm LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


file(GLOB_RECURSE SRC_FILES src/*.cpp)

# Create executable
add_executable(${PROJECT_NAME} ${SRC_FILES})
target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wnon-virtual-dtor
    -Wimplicit-fallthrough
    -Werror
)
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE -Wlifetime)
endif()

target_include_directories(${PROJECT_NAME} PRIVATE src)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(${PROJECT_NAME} PRIVATE
        -fsanitize=address,undefined
        -fno-omit-frame-pointer)
    target_link_options(${PROJECT_NAME} PRIVATE -fsanitize=address,undefined)
endif()
