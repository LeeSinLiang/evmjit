// Copyright (c) 2025 The EVMJIT Authors.
// Custom mutator for EVM bytecode fuzzing
//
// This mutator understands EVM bytecode structure:
// - Valid opcode ranges (0x00-0xff, but not all values are valid opcodes)
// - PUSH instructions (0x60-0x7f) with variable-length immediate data
// - JUMPDEST alignment and positioning
// - Basic block boundaries
//
// The mutator produces more valid bytecode sequences than random mutations,
// increasing fuzzing effectiveness and code coverage.

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <random>
#include <vector>

namespace
{

// EVM instruction categories
constexpr uint8_t STOP = 0x00;
constexpr uint8_t PUSH1 = 0x60;
constexpr uint8_t PUSH32 = 0x7f;
constexpr uint8_t DUP1 = 0x80;
constexpr uint8_t DUP16 = 0x8f;
constexpr uint8_t SWAP1 = 0x90;
constexpr uint8_t SWAP16 = 0x9f;
constexpr uint8_t JUMPDEST = 0x5b;
constexpr uint8_t JUMP = 0x56;
constexpr uint8_t JUMPI = 0x57;

// Valid EVM opcodes (not all bytes 0x00-0xff are valid)
const std::vector<uint8_t> validOpcodes = {
    // Arithmetic
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
    // Comparison & bitwise
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b,
    // SHA3
    0x20,
    // Environmental
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b,
    0x3c, 0x3d, 0x3e,
    // Block info
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45,
    // Stack, memory, storage, flow
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b,
    // PUSH1-PUSH32
    0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b,
    0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
    0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
    // DUP1-DUP16
    0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x8b,
    0x8c, 0x8d, 0x8e, 0x8f,
    // SWAP1-SWAP16
    0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0x9b,
    0x9c, 0x9d, 0x9e, 0x9f,
    // LOG0-LOG4
    0xa0, 0xa1, 0xa2, 0xa3, 0xa4,
    // System operations
    0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xfa, 0xfd, 0xff
};

std::mt19937& getRng()
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    return gen;
}

// Get random valid opcode
uint8_t randomOpcode()
{
    std::uniform_int_distribution<size_t> dist(0, validOpcodes.size() - 1);
    return validOpcodes[dist(getRng())];
}

// Calculate PUSH data size from opcode
size_t pushDataSize(uint8_t opcode)
{
    if (opcode >= PUSH1 && opcode <= PUSH32)
        return opcode - PUSH1 + 1;
    return 0;
}

// Skip over PUSH data in bytecode
size_t skipPush(const uint8_t* data, size_t size, size_t pos)
{
    if (pos >= size)
        return pos;

    uint8_t opcode = data[pos];
    if (opcode >= PUSH1 && opcode <= PUSH32)
    {
        size_t dataSize = opcode - PUSH1 + 1;
        return std::min(pos + 1 + dataSize, size);
    }
    return pos + 1;
}

} // anonymous namespace

extern "C" size_t LLVMFuzzerCustomMutator(
    uint8_t* data, size_t size, size_t max_size, unsigned int seed)
{
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> mutation_type(0, 9);

    if (size == 0)
    {
        // Generate initial bytecode
        size_t initial_size = std::min<size_t>(64, max_size);
        for (size_t i = 0; i < initial_size; ++i)
            data[i] = randomOpcode();
        return initial_size;
    }

    switch (mutation_type(rng))
    {
    case 0: // Insert random valid opcode
        if (size < max_size)
        {
            std::uniform_int_distribution<size_t> pos_dist(0, size);
            size_t pos = pos_dist(rng);
            std::memmove(data + pos + 1, data + pos, size - pos);
            data[pos] = randomOpcode();
            return size + 1;
        }
        break;

    case 1: // Replace opcode with another valid opcode
        if (size > 0)
        {
            std::uniform_int_distribution<size_t> pos_dist(0, size - 1);
            size_t pos = pos_dist(rng);
            // Only replace if not in the middle of PUSH data
            if (pos == 0 || pushDataSize(data[pos - 1]) == 0)
                data[pos] = randomOpcode();
        }
        break;

    case 2: // Insert JUMPDEST at random position
        if (size < max_size)
        {
            std::uniform_int_distribution<size_t> pos_dist(0, size);
            size_t pos = pos_dist(rng);
            std::memmove(data + pos + 1, data + pos, size - pos);
            data[pos] = JUMPDEST;
            return size + 1;
        }
        break;

    case 3: // Insert PUSH with random data
        {
            std::uniform_int_distribution<int> push_dist(0, 31);
            int push_size = push_dist(rng);
            size_t total_size = 1 + push_size;

            if (size + total_size <= max_size)
            {
                std::uniform_int_distribution<size_t> pos_dist(0, size);
                size_t pos = pos_dist(rng);
                std::memmove(data + pos + total_size, data + pos, size - pos);

                data[pos] = PUSH1 + push_size;
                for (int i = 0; i < push_size; ++i)
                    data[pos + 1 + i] = std::uniform_int_distribution<uint8_t>()(rng);

                return size + total_size;
            }
        }
        break;

    case 4: // Mutate PUSH data (keep opcode, change data)
        if (size > 0)
        {
            std::uniform_int_distribution<size_t> pos_dist(0, size - 1);
            size_t pos = pos_dist(rng);

            if (data[pos] >= PUSH1 && data[pos] <= PUSH32)
            {
                size_t dataSize = pushDataSize(data[pos]);
                for (size_t i = 1; i <= dataSize && pos + i < size; ++i)
                    data[pos + i] ^= std::uniform_int_distribution<uint8_t>()(rng);
            }
        }
        break;

    case 5: // Insert control flow: JUMP or JUMPI
        if (size < max_size)
        {
            std::uniform_int_distribution<size_t> pos_dist(0, size);
            size_t pos = pos_dist(rng);
            std::memmove(data + pos + 1, data + pos, size - pos);
            data[pos] = std::bernoulli_distribution()(rng) ? JUMP : JUMPI;
            return size + 1;
        }
        break;

    case 6: // Delete random instruction
        if (size > 1)
        {
            std::uniform_int_distribution<size_t> pos_dist(0, size - 1);
            size_t pos = pos_dist(rng);
            size_t next_pos = skipPush(data, size, pos);
            size_t delete_size = next_pos - pos;
            std::memmove(data + pos, data + next_pos, size - next_pos);
            return size - delete_size;
        }
        break;

    case 7: // Duplicate a sequence
        if (size > 0 && size < max_size / 2)
        {
            std::uniform_int_distribution<size_t> start_dist(0, size - 1);
            size_t start = start_dist(rng);
            std::uniform_int_distribution<size_t> len_dist(1, std::min<size_t>(32, size - start));
            size_t len = len_dist(rng);

            if (size + len <= max_size)
            {
                std::memmove(data + size, data + start, len);
                return size + len;
            }
        }
        break;

    case 8: // Splice two parts of bytecode
        if (size >= 4)
        {
            std::uniform_int_distribution<size_t> pos_dist(1, size - 2);
            size_t pos1 = pos_dist(rng);
            size_t pos2 = pos_dist(rng);
            if (pos1 != pos2)
            {
                std::swap(data[pos1], data[pos2]);
            }
        }
        break;

    case 9: // Truncate or extend
        {
            std::uniform_int_distribution<size_t> new_size_dist(1, max_size);
            size_t new_size = new_size_dist(rng);

            if (new_size > size && new_size <= max_size)
            {
                // Extend with random valid opcodes
                for (size_t i = size; i < new_size; ++i)
                    data[i] = randomOpcode();
                return new_size;
            }
            else if (new_size < size)
            {
                return new_size;
            }
        }
        break;
    }

    return size;
}
