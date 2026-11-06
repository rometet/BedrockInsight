#pragma once
#include <utility>

namespace bedrock_insight {
// Snapshot returns an optional owned entry, or empty if the precondition fails.
// Only the engine call's successful completion permits a postcondition check.
// Observer errors cannot skip origin or replace an engine exception.
template<class Snapshot, class Origin, class After, class Emit>
void observe_state_transition(Snapshot&& snapshot, Origin&& origin, After&& after, Emit&& emit) {
    decltype(snapshot()) entry;
    try { entry=snapshot(); } catch (...) {}
    origin();
    if (entry) {
        try { if (after(*entry)) emit(std::move(*entry)); } catch (...) {}
    }
}
} // namespace bedrock_insight
