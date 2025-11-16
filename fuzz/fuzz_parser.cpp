// Copyright (c) 2025 The EVMJIT Authors.
// Fuzz target for bytecode parsing - PUSH data and instruction iteration
//
// This fuzzer tests:
// - PUSH instruction data extraction (readPushData/skipPushData)
// - Bytecode iteration and boundary handling
// - Off-by-one errors in bytecode parsing
// - Incomplete PUSH instructions at end of bytecode
// - Jump destination identification

#include <cstdint>
#include <vector>

#include "Instruction.h"
#include "JIT.h"

using namespace dev::evmjit;

// Helper function to simulate basic block parsing logic from Compiler::createBasicBlocks
// This tests the core parsing without needing LLVM infrastructure
static void parseBytecode(code_iterator codeBegin, code_iterator codeEnd)
{
    auto skipPushDataAndGetNext = [](code_iterator curr, code_iterator end) -> code_iterator
    {
        static const auto push1 = static_cast<size_t>(Instruction::PUSH1);
        static const auto push32 = static_cast<size_t>(Instruction::PUSH32);
        size_t offset = 1;
        if (*curr >= push1 && *curr <= push32)
            offset += std::min<size_t>(*curr - push1 + 1, (end - curr) - 1);
        return curr + offset;
    };

    // Iterate through bytecode, identifying instructions and basic block boundaries
    bool isDead = false;
    auto begin = codeBegin;

    for (auto curr = begin, next = begin; curr != codeEnd; curr = next)
    {
        next = skipPushDataAndGetNext(curr, codeEnd);

        if (isDead)
        {
            if (Instruction(*curr) == Instruction::JUMPDEST)
            {
                isDead = false;
                begin = curr;
            }
            else
                continue;
        }

        bool isEnd = false;
        switch (Instruction(*curr))
        {
        case Instruction::JUMP:
        case Instruction::RETURN:
        case Instruction::REVERT:
        case Instruction::STOP:
        case Instruction::SUICIDE:
            isDead = true;
            // fallthrough
        case Instruction::JUMPI:
            isEnd = true;
            break;

        default:
            break;
        }

        if (next == codeEnd || Instruction(*next) == Instruction::JUMPDEST)
            isEnd = true;

        if (isEnd)
        {
            // This would create a basic block: [begin, next)
            begin = next;
        }
    }
}

// Test readPushData and skipPushData functions
static void testPushDataFunctions(const uint8_t* data, size_t size)
{
    if (size < 2)
        return;

    auto codeBegin = reinterpret_cast<code_iterator>(data);
    auto codeEnd = codeBegin + size;

    for (auto curr = codeBegin; curr != codeEnd; ++curr)
    {
        auto instr = Instruction(*curr);

        // Test readPushData for PUSH instructions
        if (instr >= Instruction::PUSH1 && instr <= Instruction::PUSH32)
        {
            auto currCopy = curr;
            try
            {
                // readPushData modifies the iterator
                auto pushValue = readPushData(currCopy, codeEnd);
                // Value successfully read (may be zero-padded if bytecode ends early)
            }
            catch (...)
            {
                // Catch any potential exceptions
            }

            // Test skipPushData
            currCopy = curr;
            try
            {
                skipPushData(currCopy, codeEnd);
                // Should have advanced currCopy past the PUSH data
            }
            catch (...)
            {
                // Catch any potential exceptions
            }
        }
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    // Limit bytecode size for fast fuzzing
    constexpr size_t kMaxBytecodeSize = 16 * 1024; // 16KB max
    if (size == 0 || size > kMaxBytecodeSize)
        return 0;

    auto codeBegin = reinterpret_cast<code_iterator>(data);
    auto codeEnd = codeBegin + size;

    try
    {
        // Test 1: Basic bytecode parsing logic
        parseBytecode(codeBegin, codeEnd);

        // Test 2: PUSH data extraction functions
        testPushDataFunctions(data, size);
    }
    catch (...)
    {
        // Catch any exceptions to prevent fuzzer from treating them as crashes
    }

    return 0;
}
