cmake_minimum_required(VERSION 3.10)
project(basic_lang2 CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find LLVM (adjust version as needed)
find_package(LLVM REQUIRED CONFIG)

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# Add your source files here
add_executable(basic_lang2
    main.cpp
    lexer.cpp
    parser.cpp
    # add other .cpp files as needed
)

# Link against LLVM libraries
llvm_map_components_to_libnames(llvm_libs support core orcjit native)
target_link_libraries(basic_lang2 ${llvm_libs})

# Optionally, print LLVM info
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")