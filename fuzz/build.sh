#!/bin/bash -eu
# Copyright 2025 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
################################################################################

# Build script for OSS-Fuzz integration with EVMJIT

cd $SRC/evmjit

# Initialize git submodules (EVMC)
git submodule update --init --recursive

# Create build directory
mkdir -p build
cd build

# Configure CMake with fuzzing flags
# Use static libraries and disable RTTI for better fuzzing performance
cmake .. \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_C_COMPILER=$CC \
    -DCMAKE_CXX_COMPILER=$CXX \
    -DCMAKE_C_FLAGS="$CFLAGS" \
    -DCMAKE_CXX_FLAGS="$CXXFLAGS" \
    -DBUILD_SHARED_LIBS=OFF \
    -DEVMJIT_TESTS=OFF

# Build the main evmjit library
cmake --build . --config RelWithDebInfo -j$(nproc)

# Now build the fuzz targets
cd $SRC/evmjit

# Fuzz target 1: execute() function
$CXX $CXXFLAGS -std=c++11 \
    -I$SRC/evmjit/include \
    -I$SRC/evmjit/evmc/include \
    -I$SRC/evmjit/libevmjit \
    fuzz/fuzz_execute.cpp \
    $LIB_FUZZING_ENGINE \
    build/libevmjit/libevmjit.a \
    -o $OUT/fuzz_execute

# Fuzz target 2: Compiler::compile()
$CXX $CXXFLAGS -std=c++11 \
    -I$SRC/evmjit/include \
    -I$SRC/evmjit/evmc/include \
    -I$SRC/evmjit/libevmjit \
    -I$SRC/evmjit/libevmjit/preprocessor \
    -I$SRC/evmjit/build/deps/llvm/include \
    -I$SRC/evmjit/build/deps/llvm/src/llvm/include \
    fuzz/fuzz_compiler.cpp \
    $LIB_FUZZING_ENGINE \
    build/libevmjit/libevmjit.a \
    -o $OUT/fuzz_compiler

# Fuzz target 3: bytecode parser
$CXX $CXXFLAGS -std=c++11 \
    -I$SRC/evmjit/include \
    -I$SRC/evmjit/evmc/include \
    -I$SRC/evmjit/libevmjit \
    -I$SRC/evmjit/libevmjit/preprocessor \
    -I$SRC/evmjit/build/deps/llvm/include \
    -I$SRC/evmjit/build/deps/llvm/src/llvm/include \
    fuzz/fuzz_parser.cpp \
    $LIB_FUZZING_ENGINE \
    build/libevmjit/libevmjit.a \
    -o $OUT/fuzz_parser

# Build custom mutator as a shared library
$CXX $CXXFLAGS -std=c++11 -fPIC -shared \
    fuzz/evm_mutator.cpp \
    -o $OUT/libevm_mutator.so

# Copy dictionary
cp fuzz/evm.dict $OUT/fuzz_execute.dict
cp fuzz/evm.dict $OUT/fuzz_compiler.dict
cp fuzz/evm.dict $OUT/fuzz_parser.dict

# Copy seed corpus
mkdir -p $OUT/fuzz_execute_seed_corpus
mkdir -p $OUT/fuzz_compiler_seed_corpus
mkdir -p $OUT/fuzz_parser_seed_corpus

cp fuzz/corpus/* $OUT/fuzz_execute_seed_corpus/
cp fuzz/corpus/* $OUT/fuzz_compiler_seed_corpus/
cp fuzz/corpus/* $OUT/fuzz_parser_seed_corpus/

# Create options file to use custom mutator
cat > $OUT/fuzz_execute.options <<EOF
[libfuzzer]
max_len = 24576
timeout = 30
rss_limit_mb = 4096
dict = evm.dict
custom_mutator_path = libevm_mutator.so
EOF

cat > $OUT/fuzz_compiler.options <<EOF
[libfuzzer]
max_len = 24576
timeout = 30
rss_limit_mb = 4096
dict = evm.dict
custom_mutator_path = libevm_mutator.so
EOF

cat > $OUT/fuzz_parser.options <<EOF
[libfuzzer]
max_len = 16384
timeout = 10
rss_limit_mb = 2048
dict = evm.dict
custom_mutator_path = libevm_mutator.so
EOF
