#include "bedrock_insight/core/block_observation.hpp"
#include <cassert>
#include <iostream>
#include <optional>
#include <stdexcept>
using namespace bedrock_insight;
int main() {
    // Break: the pre-call type must be a real block and the post-call type must differ.
    assert(block_break_confirmed("minecraft:stone", "minecraft:air"));
    assert(block_break_confirmed("minecraft:oak_log", "minecraft:dirt"));
    assert(!block_break_confirmed("minecraft:air", "minecraft:air"));
    assert(!block_break_confirmed("minecraft:stone", "minecraft:stone"));
    assert(!block_break_confirmed("minecraft:<svg>", "minecraft:air"));
    assert(!block_break_confirmed("", "minecraft:air"));
    assert(!block_break_confirmed("minecraft:stone", ""));

    // Place: post-call must be non-air and differ from the pre-call state.
    assert(placed_block_type("minecraft:air", "minecraft:stone") == std::optional<std::string>("minecraft:stone"));
    assert(placed_block_type("minecraft:short_grass", "minecraft:oak_planks") == std::optional<std::string>("minecraft:oak_planks"));
    assert(!placed_block_type("minecraft:air", "minecraft:air"));
    assert(!placed_block_type("minecraft:stone", "minecraft:stone"));
    assert(!placed_block_type("minecraft:stone", "minecraft:air"));
    assert(!placed_block_type("bad", "minecraft:stone"));
    assert(!placed_block_type("minecraft:air", "minecraft/<x>"));

    // Observer: origin runs exactly once; emit only for accepted + snapshot + verified after.
    for (bool accepted : {false, true}) for (bool has_entry : {false, true}) for (bool after_ok : {false, true}) {
        int calls = 0, afters = 0, emitted = 0;
        auto got = observe_block_outcome(
            [&]() -> std::optional<int> { if (has_entry) return 1; return {}; },
            [&]() { ++calls; return accepted; },
            [&](int&) { ++afters; return after_ok; },
            [&](int) { ++emitted; });
        assert(got == accepted && calls == 1);
        assert(afters == int(accepted && has_entry));
        assert(emitted == int(accepted && has_entry && after_ok));
    }

    // A throwing snapshot must not skip origin and must not emit.
    {
        int calls = 0, emitted = 0;
        assert(observe_block_outcome(
            []() -> std::optional<int> { throw 1; },
            [&]() { ++calls; return true; },
            [](int&) { assert(false); return true; },
            [&](int) { ++emitted; }));
        assert(calls == 1 && emitted == 0);
    }

    // Instrumentation exceptions are swallowed; engine result/exception is preserved.
    {
        assert(observe_block_outcome(
            [] { return std::optional<int>(1); }, [] { return true; },
            [](int&) -> bool { throw 1; }, [](int) { assert(false); }));
    }
    {
        try {
            observe_block_outcome(
                [] { return std::optional<int>(1); },
                []() -> bool { throw std::runtime_error("engine"); },
                [](int&) { assert(false); return true; },
                [](int) { assert(false); });
            assert(false);
        } catch (std::runtime_error const&) {}
    }
    std::cout << "Block observation PASS: state-transition gates, air/replace rules, origin-once; no engine proof\n";
}
