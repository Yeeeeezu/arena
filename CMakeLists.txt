cmake_minimum_required(VERSION 3.16)
project(arena CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(arena INTERFACE)
target_include_directories(arena INTERFACE include)

# tests
enable_testing()
add_executable(test_arena tests/test_arena.cpp)
target_link_libraries(test_arena PRIVATE arena)
add_test(NAME arena COMMAND test_arena)
