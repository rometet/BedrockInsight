#pragma once

namespace bedrock_insight {
// A narrowly scoped observation, NOT proof that a tool broke or was saved.
// Called only after hurtAndBreak returned true, on a reacquired inventory slot.
inline bool hurt_slot_became_empty(int delta_damage,int entry_units,int after_units,
    int slots_before,int slots_after,int slot,bool same_inventory,bool same_slot) noexcept {
    return delta_damage>0 && entry_units==1 && after_units==0 &&
        slots_before>=1 && slots_before<=128 && slots_after==slots_before &&
        slot>=0 && slot<slots_before && same_inventory && same_slot;
}
}
