// Copyright (c) 2025 The EVMJIT Authors.
// Fuzz target for Compiler::compile() - bytecode to LLVM IR compilation
//
// This fuzzer tests:
// - Bytecode parsing and instruction decoding
// - Basic block construction and CFG analysis
// - PUSH instruction data extraction
// - Jump destination validation
// - LLVM IR generation for all EVM opcodes

#include <cstdint>
#include <memory>
#include <string>

// LLVM includes must be wrapped with preprocessor guards
#include "preprocessor/llvm_includes_start.h"
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include "preprocessor/llvm_includes_end.h"

#include "Compiler.h"
#include "JIT.h"

using namespace dev::eth::jit;
using namespace dev::evmjit;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    // Limit bytecode size to avoid excessive compilation times
    constexpr size_t kMaxBytecodeSize = 24 * 1024; // 24KB max
    if (size == 0 || size > kMaxBytecodeSize)
        return 0;

    // Create LLVM context (one per fuzzing iteration to avoid state pollution)
    llvm::LLVMContext context;

    // Test different EVM revisions
    const evmc_revision revisions[] = {
        EVMC_FRONTIER,
        EVMC_HOMESTEAD,
        EVMC_TANGERINE_WHISTLE,
        EVMC_SPURIOUS_DRAGON,
        EVMC_BYZANTIUM,
        EVMC_CONSTANTINOPLE,
    };

    // Use first byte to select revision and static call flag
    evmc_revision rev = revisions[data[0] % 6];
    bool staticCall = (data[0] & 0x40) != 0;

    // Compiler options
    Compiler::Options options;
    options.rewriteSwitchToBranches = true;
    options.dumpCFG = false;

    try
    {
        // Create compiler instance
        Compiler compiler(options, rev, staticCall, context);

        // Cast to code_iterator (byte const*)
        auto codeBegin = reinterpret_cast<byte const*>(data);
        auto codeEnd = codeBegin + size;

        // Compile bytecode to LLVM IR
        auto module = compiler.compile(codeBegin, codeEnd, "fuzz_target");

        // Module automatically destroyed when going out of scope
    }
    catch (...)
    {
        // Catch any exceptions to prevent fuzzer from treating them as crashes
        // In production code, exceptions from compilation should be handled gracefully
    }

    return 0;
}
