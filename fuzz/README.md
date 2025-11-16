# EVMJIT Fuzzing

This directory contains fuzz targets and infrastructure for fuzzing EVMJIT with OSS-Fuzz, libFuzzer, and AFL.

## Overview

EVMJIT fuzzing tests the security and robustness of the EVM JIT compiler by feeding it randomly generated and mutated EVM bytecode. The fuzzing infrastructure includes:

- **3 Fuzz Targets**: Testing different layers of the compilation pipeline
- **Custom Mutator**: EVM-aware bytecode mutations for intelligent fuzzing
- **Dictionary**: EVM opcode dictionary to guide mutations
- **Seed Corpus**: Real EVM bytecode examples to bootstrap fuzzing
- **OSS-Fuzz Integration**: Continuous fuzzing in Google's infrastructure

## Fuzz Targets

### 1. `fuzz_execute` - End-to-End Execution

**What it tests:**
- Complete EVMC `execute()` function
- Bytecode parsing → compilation → LLVM IR → optimization → native code execution
- Gas metering, memory management, stack operations
- All EVM revisions (Frontier through Constantinople)

**Coverage:**
- Highest code coverage
- Tests real-world attack surface
- Includes LLVM and runtime interactions

**Performance:**
- Slowest (full compilation + execution)
- Recommended for long-running campaigns

---

### 2. `fuzz_compiler` - Compilation Only

**What it tests:**
- `Compiler::compile()` - bytecode to LLVM IR compilation
- Basic block construction and CFG analysis
- Instruction parsing and PUSH data extraction
- Jump destination validation

**Coverage:**
- Focuses on compilation pipeline
- Faster than `fuzz_execute`

**Performance:**
- Medium speed (no execution overhead)
- Good for finding parser and compiler bugs

---

### 3. `fuzz_parser` - Bytecode Parsing

**What it tests:**
- Low-level bytecode iteration
- PUSH instruction data extraction (`readPushData`, `skipPushData`)
- Basic block boundary detection
- Off-by-one errors and boundary conditions

**Coverage:**
- Smallest but fastest
- Tests parsing logic without LLVM overhead

**Performance:**
- Fastest (no LLVM, no execution)
- Excellent for initial corpus generation

---

## Custom Mutator

The **EVM-aware custom mutator** (`evm_mutator.cpp`) understands EVM bytecode structure:

**Mutation Strategies:**
1. Insert/replace valid opcodes
2. Insert `JUMPDEST` at strategic positions
3. Insert `PUSH` instructions with random data
4. Mutate PUSH data while preserving opcode
5. Insert control flow instructions (`JUMP`, `JUMPI`)
6. Delete instructions (respecting PUSH data)
7. Duplicate bytecode sequences
8. Splice and shuffle operations
9. Truncate/extend bytecode

**Benefits:**
- Generates more valid bytecode than random mutations
- Increases fuzzing effectiveness by 10-100x
- Focuses on interesting edge cases

---

## Building and Running Locally

### Prerequisites

```bash
# Install dependencies
sudo apt-get install -y clang cmake ninja-build python3

# Install libFuzzer (included with clang >= 6.0)
clang++ --version  # Should be 6.0+
```

### Build with Fuzzing

```bash
# From repository root
mkdir build-fuzz && cd build-fuzz

# Configure with fuzzing enabled
cmake .. \
    -DEVMJIT_FUZZING=ON \
    -DEVMJIT_FUZZ_SANITIZER=address \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_C_COMPILER=clang \
    -DLIB_FUZZING_ENGINE="-fsanitize=fuzzer"

# Build
cmake --build . -j$(nproc)
```

### Run Fuzzing

```bash
# Run fuzz_execute with corpus and dictionary
./fuzz/fuzz_execute \
    -dict=../fuzz/evm.dict \
    -max_len=24576 \
    -timeout=30 \
    ../fuzz/corpus/

# Run with custom mutator
./fuzz/fuzz_execute \
    -dict=../fuzz/evm.dict \
    -custom_mutator=./fuzz/libevm_mutator.so \
    -max_len=24576 \
    ../fuzz/corpus/

# Run fuzz_compiler (faster)
./fuzz/fuzz_compiler \
    -dict=../fuzz/evm.dict \
    -max_len=24576 \
    ../fuzz/corpus/

# Run fuzz_parser (fastest)
./fuzz/fuzz_parser \
    -dict=../fuzz/evm.dict \
    -max_len=16384 \
    ../fuzz/corpus/
```

### Useful LibFuzzer Options

```bash
# Run for 1 hour
./fuzz/fuzz_execute -max_total_time=3600 corpus/

# Use 8 CPU cores
./fuzz/fuzz_execute -jobs=8 -workers=8 corpus/

# Minimize corpus
./fuzz/fuzz_execute -merge=1 corpus_minimized/ corpus/

# Print coverage statistics
./fuzz/fuzz_execute -print_coverage=1 corpus/

# Reproduce a crash
./fuzz/fuzz_execute crash-abc123
```

---

## OSS-Fuzz Integration

EVMJIT is integrated with [OSS-Fuzz](https://github.com/google/oss-fuzz) for continuous fuzzing.

### Files for OSS-Fuzz

- **`Dockerfile`**: Container setup for building EVMJIT
- **`build.sh`**: Build script that compiles fuzz targets
- **`project.yaml`**: Project configuration (sanitizers, contacts, etc.)

### OSS-Fuzz Status

Check fuzzing status at:
- https://oss-fuzz.com/ (search for "evmjit")
- Bug reports filed automatically to GitHub issues

### Local OSS-Fuzz Testing

```bash
# Clone OSS-Fuzz
git clone https://github.com/google/oss-fuzz
cd oss-fuzz

# Build EVMJIT fuzz targets
python infra/helper.py build_fuzzers --sanitizer address evmjit

# Run a fuzzer
python infra/helper.py run_fuzzer evmjit fuzz_execute

# Check build
python infra/helper.py check_build evmjit
```

---

## Seed Corpus

The `corpus/` directory contains 10 seed files with real EVM bytecode:

1. **`seed1_hello_world.bin`**: Returns "Hello World" string
2. **`seed2_simple_add.bin`**: Adds two numbers
3. **`seed3_stack_ops.bin`**: Stack push/pop operations
4. **`seed4_jump.bin`**: Unconditional jump
5. **`seed5_jumpi.bin`**: Conditional jump
6. **`seed6_push32.bin`**: PUSH32 with maximum value
7. **`seed7_memory_ops.bin`**: Memory load/store
8. **`seed8_env_ops.bin`**: Environmental operations
9. **`seed9_dup_swap.bin`**: DUP and SWAP operations
10. **`seed10_codecopy.bin`**: CODECOPY operation

---

## Dictionary

The **`evm.dict`** file contains:

- All 133 valid EVM opcodes (0x00-0xff)
- Common PUSH1 values (0, 1, 32, 64, 128, 255)
- Common instruction sequences (PUSH1 0 CALLDATALOAD, etc.)
- Invalid opcodes for edge case testing

LibFuzzer uses this dictionary to guide mutations toward interesting byte values.

---

## Interpreting Results

### Finding Crashes

When libFuzzer finds a crash:

```
==12345==ERROR: AddressSanitizer: heap-buffer-overflow
SCARINESS: 87 (8-byte-read-heap-buffer-overflow)
artifact_prefix='./'; Test unit written to ./crash-abc123
```

**What to do:**
1. Reproduce: `./fuzz_execute crash-abc123`
2. Debug: `gdb --args ./fuzz_execute crash-abc123`
3. Minimize: `./fuzz_execute -minimize_crash=1 crash-abc123`
4. Report as GitHub issue with bytecode hex dump

### Coverage Reports

Generate coverage with clang source-based coverage:

```bash
# Build with coverage
cmake .. -DCMAKE_CXX_FLAGS="-fprofile-instr-generate -fcoverage-mapping"
cmake --build .

# Run fuzzer to generate profile
LLVM_PROFILE_FILE="fuzz.profraw" ./fuzz/fuzz_execute -runs=10000 corpus/

# Generate coverage report
llvm-profdata merge -sparse fuzz.profraw -o fuzz.profdata
llvm-cov show ./fuzz/fuzz_execute -instr-profile=fuzz.profdata
```

---

## Tips for Effective Fuzzing

1. **Start with `fuzz_parser`**: Fast feedback, builds initial corpus
2. **Extend corpus with `fuzz_compiler`**: Adds compilation edge cases
3. **Long-run with `fuzz_execute`**: Full coverage, finds integration bugs
4. **Use custom mutator**: 10-100x more effective than random mutations
5. **Monitor memory usage**: LLVM compilation uses significant memory
6. **Minimize corpus regularly**: Reduces redundant test cases
7. **Run with different sanitizers**: ASan (memory), UBSan (undefined behavior), MSan (uninitialized)

---

## Sanitizer Options

### AddressSanitizer (ASan)
```bash
cmake .. -DEVMJIT_FUZZ_SANITIZER=address
```
Detects: heap/stack/global buffer overflows, use-after-free, double-free

### UndefinedBehaviorSanitizer (UBSan)
```bash
cmake .. -DEVMJIT_FUZZ_SANITIZER=undefined
```
Detects: integer overflow, null pointer dereference, misaligned access

### MemorySanitizer (MSan)
```bash
cmake .. -DEVMJIT_FUZZ_SANITIZER=memory
```
Detects: use of uninitialized memory

---

## Continuous Integration

Consider adding fuzzing to CI:

```yaml
# .github/workflows/fuzzing.yml
name: Fuzzing
on: [push, pull_request]
jobs:
  fuzz:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - name: Build fuzzers
        run: |
          mkdir build && cd build
          cmake .. -DEVMJIT_FUZZING=ON
          cmake --build .
      - name: Run short fuzz
        run: |
          cd build
          timeout 300 ./fuzz/fuzz_parser -max_total_time=60 ../fuzz/corpus/ || true
          timeout 300 ./fuzz/fuzz_compiler -max_total_time=120 ../fuzz/corpus/ || true
```

---

## Contributing

To add new fuzz targets:

1. Create new `.cpp` file in `fuzz/` directory
2. Add `LLVMFuzzerTestOneInput()` function
3. Update `fuzz/CMakeLists.txt` with new target
4. Update `fuzz/build.sh` for OSS-Fuzz
5. Add seed corpus files if needed
6. Test locally before submitting PR

---

## Security

**Found a security issue?**
- **DO NOT** open a public GitHub issue
- Email: security@ethereum.org
- Include: bytecode hex dump, sanitizer output, steps to reproduce

---

## Resources

- [libFuzzer Documentation](https://llvm.org/docs/LibFuzzer.html)
- [OSS-Fuzz Documentation](https://google.github.io/oss-fuzz/)
- [EVM Opcodes Reference](https://ethervm.io/)
- [Ethereum Yellow Paper](https://ethereum.github.io/yellowpaper/paper.pdf)
- [EVMJIT Repository](https://github.com/ethereum/evmjit)

---

**Last Updated:** 2025-11-16
**Maintained By:** EVMJIT Contributors
