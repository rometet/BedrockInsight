#pragma once
#include "bedrock_insight/core/absorption_sql.hpp"
#include <string>
namespace bedrock_insight {
// The same fixed-point/lifetime evidence contract, with a distinct attribute,
// provenance and keys. All replacements are compile-time-owned literals, never
// request input. Legacy notifications cannot enter this query.
inline std::string const& health_loss_predicate() {
    static std::string const sql=[] {
        std::string value=kAbsorptionPredicate;
        auto replace=[&](std::string const& from,std::string const& to) {
            for(std::size_t pos=0;(pos=value.find(from,pos))!=std::string::npos;pos+=to.size())value.replace(pos,from.size(),to);
        };
        replace("hurt_absorption_delta.v1","hurt_health_delta.v1");
        replace("'ABSORPTION'","'HEALTH'");
        replace("minecraft:damage_absorbed","minecraft:damage_taken");
        replace("minecraft:damage_dealt_absorbed","minecraft:damage_dealt");
        // Keep malformed JSON inside the CASE guard, not an optimizer-dependent
        // extra AND which could attempt JSON extraction before json_valid.
        replace("json_type(details_json)='object'",
            "json_type(details_json)='object' AND json_type(details_json,'$.health_attribute_decrease_observed')='true'");
        return value;
    }();
    return sql;
}
}
