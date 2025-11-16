# CLAUDE.md - AI Assistant Guide for EVMJIT

## Repository Overview

**Project Name:** EVM JIT (Ethereum Virtual Machine Just-In-Time Compiler)
**Version:** 0.9.0.2
**License:** MIT
**Primary Language:** C++11
**Build System:** CMake 3.4.0+
**Dependencies:** LLVM 5.0.0 (automatically downloaded and built)
**Architecture:** x86_64 only

### Purpose

EVMJIT is a library for just-in-time compilation of Ethereum EVM bytecode to native machine code using LLVM. It provides a performance-optimized alternative to traditional interpreter-based EVM execution for Ethereum clients.

### Project Status

**IMPORTANT:** This project is not actively maintained. The maintainers are seeking new contributors. See [issue #184](https://github.com/ethereum/evmjit/issues/184).

## Codebase Structure

```
/home/user/evmjit/
├── libevmjit/              # Core JIT compilation library (~6,137 lines)
│   ├── JIT.cpp/.h          # Main JIT instance and EVMC interface
│   ├── Compiler.cpp/.h     # EVM bytecode to LLVM IR compiler
│   ├── Ext.cpp/.h          # External environment (blockchain state)
│   ├── Memory.cpp/.h       # EVM memory management
│   ├── GasMeter.cpp/.h     # Gas metering and cost calculation
│   ├── RuntimeManager.cpp/.h # Runtime context management
│   ├── Arith256.cpp/.h     # 256-bit arithmetic operations
│   ├── BasicBlock.cpp/.h   # Control flow graph construction
│   ├── Array.cpp/.h        # Stack data structure
│   ├── Cache.cpp/.h        # Compiled code caching
│   ├── Instruction.cpp/.h  # EVM opcode definitions
│   ├── Optimizer.cpp/.h    # LLVM IR optimization passes
│   └── ...                 # Supporting files
├── include/                # Public API headers
│   └── evmjit.h           # Main public interface (evmjit_create())
├── tests/                  # Test suite
│   └── test-evmjit-standalone.c
├── cmake/                  # CMake modules
│   └── ProjectLLVM.cmake   # LLVM dependency management
├── scripts/                # Build helper scripts
├── docker/                 # Docker container for testing
├── evmc/                   # EVMC interface (git submodule)
├── CMakeLists.txt         # Root build configuration
├── .clang-format          # Code formatting rules
└── CI configs             # .travis.yml, circle.yml, appveyor.yml, wercker.yml
```

### Key Components

| Component | Purpose | Files |
|-----------|---------|-------|
| **JIT Engine** | Main entry point and EVMC interface | JIT.cpp/h |
| **Compiler** | Converts EVM bytecode to LLVM IR | Compiler.cpp/h |
| **Code Generator** | Implements EVM instructions in LLVM IR | Ext.cpp, Memory.cpp, GasMeter.cpp |
| **Optimizer** | LLVM IR optimization pipeline | Optimizer.cpp/h |
| **Runtime** | Execution context and state management | RuntimeManager.cpp/h |
| **Cache** | Compiled code storage and retrieval | Cache.cpp/h |

### Compilation Pipeline

```
EVM Bytecode
    ↓
Instruction Parser
    ↓
Basic Block Analysis (CFG Construction)
    ↓
Compiler (Bytecode → LLVM IR)
    ↓
Optimizer (LLVM IR Optimization)
    ↓
MCJIT (Native Code Generation)
    ↓
Executable Function
    ↓
Cache Storage
```

## Development Workflows

### Initial Setup

```bash
# Clone with submodules
git clone <repository-url>
cd evmjit
git submodule update --init --recursive

# Create build directory
mkdir build
cd build

# Configure with CMake
cmake ..

# Build (RelWithDebInfo recommended)
cmake --build . --config RelWithDebInfo
```

### Build Options

```bash
# Enable tests
cmake .. -DEVMJIT_TESTS=ON

# Enable examples
cmake .. -DEVMJIT_EXAMPLES=ON

# Specify build type
cmake .. -DCMAKE_BUILD_TYPE=Release     # or Debug, RelWithDebInfo
```

### Build Targets

- `evmjit` - Main shared/static library
- `evmjit-standalone` - Static library with all LLVM dependencies bundled
- `evmjit-standalone-thin` - Thin static library (Linux/macOS only)

### Testing

```bash
# Enable tests during configuration
cmake .. -DEVMJIT_TESTS=ON

# Build tests
cmake --build .

# Run tests
ctest
```

### CI/CD Platforms

The project uses multiple CI platforms:
- **Travis CI**: Linux (GCC/Clang) and macOS builds
- **CircleCI**: Linux and macOS with parallel job support
- **AppVeyor**: Windows (Visual Studio 2015, Win64)
- **Wercker**: Additional validation
- **Docker**: Ubuntu 16.04 with LLVM 3.9 testing

## Code Conventions and Standards

### Code Style

This project uses **Chromium-based** code style with the following key rules:

#### Formatting (enforced by .clang-format)

- **Indentation:** 4 spaces (no tabs)
- **Column Limit:** 100 characters
- **Brace Style:** Allman style (braces on new lines)
  ```cpp
  void function()
  {
      if (condition)
      {
          // code
      }
  }
  ```
- **Pointer Alignment:** Left (`Type* var`, not `Type *var`)
- **Access Modifiers:** Offset by -4 (at class level)
- **No Short Functions on Single Line:** Except inline functions
- **Constructor Initializers:** Before colon, one per line

#### Running Code Formatter

```bash
# Format a single file
clang-format -i libevmjit/Compiler.cpp

# Format all C++ files
find libevmjit -name "*.cpp" -o -name "*.h" | xargs clang-format -i
```

### C++ Standards

- **C++ Version:** C++11 (strictly enforced)
- **RTTI:** Disabled (`-fno-rtti`)
- **Visibility:** Hidden by default (`-fvisibility=hidden`)
- **Compiler Warnings:** Strict (`-Wall -Wextra -Wconversion`)
- **Sign Conversion:** Disabled (`-Wno-sign-conversion`)

### Namespaces

- `dev::evmjit` - Main JIT namespace
- `dev::eth::jit` - Ethereum JIT compiler namespace
- `llvm` - LLVM library bindings

### Naming Conventions

Based on code analysis:
- **Classes:** PascalCase (e.g., `Compiler`, `BasicBlock`, `GasMeter`)
- **Functions:** camelCase (e.g., `evmjit_create()`, `compile()`)
- **Constants/Enums:** PascalCase or UPPER_CASE
- **Private Members:** Standard naming (no prefix)

### File Organization

- **Header Files:** `.h` extension
- **Implementation Files:** `.cpp` extension
- **Header/Implementation Pairs:** Co-located in same directory
- **One Class Per File:** Generally followed

### Include Order (from .clang-format)

1. Project headers (`"..."`)
2. C standard library headers (`<*.h>`)
3. Boost headers (`<boost...>`)
4. Other library headers (`<...>`)

## Architecture and Key Concepts

### EVM Revisions Supported

- FRONTIER
- HOMESTEAD
- TANGERINE_WHISTLE
- SPURIOUS_DRAGON
- BYZANTIUM
- CONSTANTINOPLE

### Data Types

- **i256**: 256-bit integer (4× uint64_t) for EVM stack values
- **RuntimeData**: Execution context (gas, code, call data, addresses)
- **ExecutionContext**: Runtime environment with memory and EVMC context
- **ReturnCode**: Execution result codes

### Critical Implementation Details

#### Gas Metering
- All instructions have associated gas costs
- Memory expansion costs are tracked
- Out-of-gas conditions halt execution
- Pre-paid gas model used

#### Memory Management
- EVM memory is byte-addressable
- Grows in 256-bit (32-byte) words
- Expansion cost increases quadratically
- Implemented using LLVM memory operations

#### Stack Operations
- 256-bit wide stack
- Maximum depth: 1024
- Underflow/overflow checking
- Implemented as LLVM array

#### Control Flow
- Jump destinations validated via JUMPDEST
- Basic block construction for CFG
- Invalid jumps cause execution failure

### LLVM Integration

#### LLVM Version
Fixed at LLVM 5.0.0 (downloaded and built by CMake)

#### LLVM Components Used
- **MCJIT**: JIT execution engine
- **x86 Target**: Only x86_64 code generation
- **IP Optimization**: Interprocedural optimizations
- **IR Builder**: LLVM IR construction
- **Module/Function**: Code organization

#### Optimization Passes
See `Optimizer.cpp` for the complete pipeline.

## Guidelines for AI Assistants

### When Making Code Changes

1. **Always run clang-format** before committing:
   ```bash
   clang-format -i <modified-files>
   ```

2. **Respect the architecture:** x86_64 only - do not add ARM/other architectures without extensive refactoring

3. **Maintain EVM compatibility:** Any changes to instruction implementations must match EVM specification exactly

4. **Gas costs must be accurate:** Incorrect gas metering is a critical bug

5. **Test EVMC compatibility:** The public API (`evmjit_create()`) must maintain EVMC interface

6. **LLVM version is fixed:** Do not upgrade LLVM without thorough testing (ABI/API changes likely)

### Common Tasks

#### Adding a New EVM Instruction

1. Add opcode definition to `Instruction.h`
2. Implement handler in `Compiler.cpp`
3. Add gas costs to `GasMeter.cpp`
4. Update tests

#### Modifying Gas Costs

1. Update `GasMeter.cpp` with new costs
2. Verify against Ethereum Yellow Paper
3. Ensure all EVM revision checks are correct

#### Debugging Compilation Issues

1. Check LLVM IR output (enable debug flags)
2. Verify basic block construction in `BasicBlock.cpp`
3. Check jump destination validation
4. Review gas meter state

#### Performance Optimization

1. Review LLVM optimization passes in `Optimizer.cpp`
2. Check cache hit rates in `Cache.cpp`
3. Profile native code execution
4. Consider IR-level optimizations in `Compiler.cpp`

### Building and Testing Workflow

```bash
# Standard development cycle
mkdir build && cd build
cmake .. -DEVMJIT_TESTS=ON
cmake --build . --config RelWithDebInfo

# Run tests
ctest --output-on-failure

# Format code
clang-format -i ../libevmjit/*.cpp ../libevmjit/*.h

# Clean rebuild
rm -rf *
cmake .. -DEVMJIT_TESTS=ON
cmake --build . --config RelWithDebInfo
```

### Git Workflow

#### Branch Naming
- Feature branches: `feature/<description>`
- Bug fixes: `fix/<description>`
- AI assistant branches: `claude/<session-id>`

#### Commit Messages
Follow conventional format:
```
<type>: <short description>

<detailed explanation if needed>

<reference to issues if applicable>
```

Types: `feat`, `fix`, `refactor`, `test`, `docs`, `chore`, `perf`

#### Before Committing

1. Format code with clang-format
2. Build successfully
3. Run tests (if EVMJIT_TESTS=ON)
4. Check for compiler warnings
5. Verify no unresolved symbols (Linux)

### Understanding the Code

#### Entry Points
- `evmjit_create()` in `JIT.cpp` - Creates JIT instance
- `JIT::exec()` - Main execution entry

#### Tracing Execution
1. Start at `JIT::exec()` in `JIT.cpp`
2. Follow to `Compiler::compile()` in `Compiler.cpp`
3. See instruction handling in `Compiler::compileBasicBlock()`
4. Check runtime in `RuntimeManager.cpp`

#### Key Files to Understand First
1. `Common.h` - Type definitions
2. `JIT.h` - Main interface
3. `Instruction.h` - EVM opcodes
4. `Compiler.h` - Compilation interface
5. `evmjit.h` - Public API

### Common Pitfalls

1. **Platform assumptions:** Code assumes x86_64 - don't add portable changes without careful consideration
2. **LLVM API changes:** LLVM 5.0.0 API is different from newer versions
3. **EVM semantics:** Integer overflow, division by zero, etc. have specific behaviors
4. **Gas accounting:** Must be precise - off-by-one errors are critical
5. **Memory model:** EVM memory is byte-addressable but grows in words
6. **Stack depth:** Maximum 1024 - must check limits
7. **RTTI disabled:** Cannot use dynamic_cast or typeid

### Documentation

When adding features or making significant changes:

1. Update this CLAUDE.md if architecture/workflow changes
2. Add doxygen-style comments to headers
3. Document non-obvious implementation choices in code comments
4. Update README.md for user-facing changes

### Dependencies and External Interfaces

#### EVMC Interface
- Submodule at `evmc/`
- Defines VM interface standard
- Must maintain compatibility

#### LLVM
- Version 5.0.0 (fixed)
- Downloaded automatically by CMake
- Located in `build/deps/llvm/`

#### No Other Runtime Dependencies
- Self-contained library
- Static linking preferred for distribution

## Useful Commands Reference

```bash
# Build commands
cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo -DEVMJIT_TESTS=ON
cmake --build . -j$(nproc)

# Testing
ctest --verbose
ctest --output-on-failure

# Code formatting
find libevmjit include -name "*.cpp" -o -name "*.h" | xargs clang-format -i

# Check formatting
clang-format --dry-run --Werror libevmjit/*.cpp

# Clean build
rm -rf build && mkdir build && cd build

# Install
sudo cmake --install .

# Generate compile_commands.json for IDE integration
cmake .. -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

## Environment Variables

Runtime options can be passed via `EVMJIT` environment variable:

```bash
# Get help on available options
EVMJIT="-help" <ethereum-client>

# Example usage
EVMJIT="-cache=0" testeth --jit
```

## Resources

- **Repository:** https://github.com/ethereum/evmjit
- **Gitter Chat:** https://gitter.im/ethereum/evmjit
- **Maintainers Issue:** https://github.com/ethereum/evmjit/issues/184
- **EVMC Spec:** https://github.com/ethereum/evmc
- **Ethereum Yellow Paper:** https://ethereum.github.io/yellowpaper/paper.pdf
- **LLVM 5.0 Docs:** https://releases.llvm.org/5.0.0/docs/

## Additional Notes

### Memory and Performance

- Compiled code is cached for reuse
- First execution compiles (slower), subsequent executions use cache (faster)
- Memory usage scales with number of unique contract codes
- LLVM optimizations significantly improve execution speed

### Thread Safety

Review `JIT.cpp` and `Cache.cpp` for thread safety details. LLVM MCJIT has specific threading requirements.

### Platform-Specific Notes

- **Linux:** Default development platform, best tested
- **macOS:** Supported, uses libtool for standalone library
- **Windows:** Requires Visual Studio 2015+, uses different C++ runtime linking
- **Other platforms:** Not supported (x86_64 requirement)

---

**Last Updated:** 2025-11-16
**For Version:** 0.9.0.2
**Maintained By:** AI Assistant (Claude)
