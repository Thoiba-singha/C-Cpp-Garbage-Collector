# CMakeList.txt : CMake project for GarbageCollector, include source and define

cmake_minimum_required (VERSION 3.15)

project ("GarbageCollector" C CXX)

set(CMAKE_CXX_STANDARD 17)

file(GLOB C_FILES "gc/c/C_Ptr.cpp")

file(GLOB CPP_FILES "gc/cpp/Cpp_Ptr.hpp")


# Executable for C
# add_executable (${PROJECT_NAME} ${C_FILES} "test2.c")

# Executable for C++
add_executable (${PROJECT_NAME} ${CPP_FILES} "test1.cpp")


# TODO: Add tests and install targets if needed.
