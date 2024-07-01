cmake_minimum_required(VERSION 3.10)

# Set the project name and version
project(ClangWrapper VERSION 1.0)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_EXPORT_COMPILE_COMMANDS True)

# Include directories
include_directories(${PROJECT_SOURCE_DIR}/include)

# Set the output type to be a static binary
set(CMAKE_EXE_LINKER_FLAGS "-static")

# Add the executable
add_executable(clang-wrapper src/main.cpp src/sha256.cpp)

