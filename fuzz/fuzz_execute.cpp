// Copyright (c) 2025 The EVMJIT Authors.
// Fuzz target for EVMC execute() function - end-to-end bytecode execution
//
// This fuzzer tests the complete pipeline:
// - Bytecode parsing and validation
// - Basic block construction and CFG analysis
// - LLVM IR compilation and optimization
// - Native code generation and execution
// - Gas metering and memory management

#include <cstdint>
#include <cstring>
#include <vector>

#include <evmjit.h>
#include <evmc/evmc.h>

// Minimal EVMC host implementation for fuzzing
namespace
{

// Mock host context callbacks
static bool account_exists(evmc_context*, const evmc_address*) noexcept
{
    return false;
}

static evmc_bytes32 get_storage(evmc_context*, const evmc_address*, const evmc_bytes32*) noexcept
{
    return {};
}

static evmc_storage_status set_storage(
    evmc_context*, const evmc_address*, const evmc_bytes32*, const evmc_bytes32*) noexcept
{
    return EVMC_STORAGE_UNCHANGED;
}

static evmc_uint256be get_balance(evmc_context*, const evmc_address*) noexcept
{
    return {};
}

static size_t get_code_size(evmc_context*, const evmc_address*) noexcept
{
    return 0;
}

static evmc_bytes32 get_code_hash(evmc_context*, const evmc_address*) noexcept
{
    return {};
}

static size_t copy_code(evmc_context*, const evmc_address*, size_t, uint8_t*, size_t) noexcept
{
    return 0;
}

static void selfdestruct(evmc_context*, const evmc_address*, const evmc_address*) noexcept {}

static void call(evmc_result* result, evmc_context*, const evmc_message*) noexcept
{
    result->status_code = EVMC_REVERT;
    result->gas_left = 0;
}

static evmc_tx_context get_tx_context(evmc_context*) noexcept
{
    evmc_tx_context ctx{};
    ctx.tx_gas_price = {};
    ctx.tx_origin = {};
    ctx.block_coinbase = {};
    ctx.block_number = 1;
    ctx.block_timestamp = 1;
    ctx.block_gas_limit = 10000000;
    ctx.block_difficulty = {};
    ctx.chain_id = {};
    return ctx;
}

static evmc_bytes32 get_block_hash(evmc_context*, int64_t) noexcept
{
    return {};
}

static void emit_log(
    evmc_context*, const evmc_address*, const uint8_t*, size_t, const evmc_bytes32[], size_t) noexcept
{
}

// Global host interface table
const evmc_context_fn_table host_fn_table = {
    account_exists,
    get_storage,
    set_storage,
    get_balance,
    get_code_size,
    get_code_hash,
    copy_code,
    selfdestruct,
    call,
    get_tx_context,
    get_block_hash,
    emit_log,
};

// Mock host context
struct FuzzHostContext
{
    const evmc_context_fn_table* fn_table = &host_fn_table;
};

} // anonymous namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    // Limit bytecode size to avoid excessive compilation times
    constexpr size_t kMaxBytecodeSize = 24 * 1024; // 24KB max
    if (size == 0 || size > kMaxBytecodeSize)
        return 0;

    // Create EVMJIT instance (singleton, created once)
    static evmc_instance* jit = evmjit_create();
    if (!jit)
        return 0;

    // Set up host context
    FuzzHostContext host_ctx;
    evmc_context context;
    context.fn_table = &host_fn_table;

    // Create execution message
    evmc_message msg{};
    msg.kind = EVMC_CALL;
    msg.flags = 0;
    msg.depth = 0;
    msg.gas = 1000000; // Provide sufficient gas
    msg.recipient = {};
    msg.sender = {};
    msg.input_data = nullptr;
    msg.input_size = 0;
    msg.value = {};
    msg.create2_salt = {};

    // Compute code hash for caching
    // For fuzzing, we use first 32 bytes or zero-padded data as hash
    evmc_bytes32 code_hash{};
    size_t hash_len = size < 32 ? size : 32;
    std::memcpy(code_hash.bytes, data, hash_len);
    msg.code_hash = code_hash;

    // Test all EVM revisions
    const evmc_revision revisions[] = {
        EVMC_FRONTIER,
        EVMC_HOMESTEAD,
        EVMC_TANGERINE_WHISTLE,
        EVMC_SPURIOUS_DRAGON,
        EVMC_BYZANTIUM,
        EVMC_CONSTANTINOPLE,
    };

    // Cycle through revisions (use first byte to select revision if available)
    evmc_revision rev = revisions[data[0] % 6];

    // Execute the bytecode
    evmc_result result = jit->execute(jit, &context, rev, &msg, data, size);

    // Clean up result
    if (result.release)
        result.release(&result);

    return 0;
}
