#pragma once

#include "bedrock_insight/core/event.hpp"
#include <optional>
#include <string_view>
#include <utility>

namespace bedrock_insight {
inline bool valid_item_type(std::string_view name) noexcept {
    if (name.empty() || name.size()>128) return false;
    auto colon=name.find(':');
    if (colon==0 || colon==std::string_view::npos || colon+1==name.size()) return false;
    for (std::size_t i=0;i<name.size();++i) {
        char c=name[i];
        if (i==colon) continue;
        if (!((c>='a' && c<='z') || (c>='0' && c<='9') || c=='_' || c=='-' || c=='.'
            || (i>colon && c=='/'))) return false;
    }
    return true;
}

// Snapshot before origin: the item may be changed/invalid after the engine call.
// Only a true return is observed, NOT proof of an inventory delta/world entity.
// Instrumentation exceptions never skip origin or replace its result/exception.
template<class Snapshot, class Origin, class Emit>
bool observe_drop_result(bool randomly, Snapshot&& snapshot, Origin&& origin, Emit&& emit) {
    std::optional<AnalyticsEvent> value;
    if (!randomly) {
        try { value=snapshot(); } catch (...) {}
    }
    bool const accepted=origin();
    if (accepted && value) {
        try { emit(std::move(*value)); } catch (...) {}
    }
    return accepted;
}
} // namespace bedrock_insight
