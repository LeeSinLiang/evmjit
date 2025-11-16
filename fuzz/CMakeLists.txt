# CMake configuration for EVMJIT fuzz targets
# This allows local building and testing with libFuzzer or AFL

cmake_minimum_required(VERSION 3.4.0)

# Check if fuzzing is enabled
if(NOT EVMJIT_FUZZING)
    message(STATUS "Fuzzing disabled. Use -DEVMJIT_FUZZING=ON to enable.")
    return()
endif()

message(STATUS "Building EVMJIT fuzz targets")

# Ensure we have a fuzzing engine
if(NOT DEFINED LIB_FUZZING_ENGINE)
    # Default to libFuzzer if not specified
    set(LIB_FUZZING_ENGINE "-fsanitize=fuzzer")
    message(STATUS "Using libFuzzer (default): ${LIB_FUZZING_ENGINE}")
endif()

# Common compiler flags for fuzzing
set(FUZZ_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O1 -fno-omit-frame-pointer")

# AddressSanitizer (recommended)
if(EVMJIT_FUZZ_SANITIZER STREQUAL "address" OR NOT DEFINED EVMJIT_FUZZ_SANITIZER)
    set(FUZZ_CXX_FLAGS "${FUZZ_CXX_FLAGS} -fsanitize=address")
    message(STATUS "Using AddressSanitizer")
endif()

# UndefinedBehaviorSanitizer
if(EVMJIT_FUZZ_SANITIZER STREQUAL "undefined")
    set(FUZZ_CXX_FLAGS "${FUZZ_CXX_FLAGS} -fsanitize=undefined")
    message(STATUS "Using UndefinedBehaviorSanitizer")
endif()

# MemorySanitizer
if(EVMJIT_FUZZ_SANITIZER STREQUAL "memory")
    set(FUZZ_CXX_FLAGS "${FUZZ_CXX_FLAGS} -fsanitize=memory")
    message(STATUS "Using MemorySanitizer")
endif()

# Function to create a fuzz target
function(add_fuzz_target target_name source_file)
    add_executable(${target_name} ${source_file})

    target_compile_options(${target_name} PRIVATE ${FUZZ_CXX_FLAGS})

    target_include_directories(${target_name} PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/evmc/include
        ${CMAKE_SOURCE_DIR}/libevmjit
        ${CMAKE_SOURCE_DIR}/libevmjit/preprocessor
        ${LLVM_INCLUDE_DIRECTORIES}
    )

    target_link_libraries(${target_name} PRIVATE
        evmjit
        ${LIB_FUZZING_ENGINE}
    )

    set_target_properties(${target_name} PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED ON
        FOLDER "fuzz"
    )
endfunction()

# Fuzz target 1: EVMC execute() - end-to-end execution
add_fuzz_target(fuzz_execute fuzz_execute.cpp)

# Fuzz target 2: Compiler::compile() - bytecode compilation
add_fuzz_target(fuzz_compiler fuzz_compiler.cpp)

# Fuzz target 3: Bytecode parser - low-level parsing
add_fuzz_target(fuzz_parser fuzz_parser.cpp)

# Custom mutator library
add_library(evm_mutator SHARED evm_mutator.cpp)
set_target_properties(evm_mutator PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED ON
    FOLDER "fuzz"
)

# Install fuzz targets (optional)
install(TARGETS fuzz_execute fuzz_compiler fuzz_parser evm_mutator
    RUNTIME DESTINATION bin/fuzz
    LIBRARY DESTINATION lib/fuzz
)

# Copy dictionary and corpus for local testing
install(FILES evm.dict DESTINATION bin/fuzz)
install(DIRECTORY corpus/ DESTINATION bin/fuzz/corpus)
