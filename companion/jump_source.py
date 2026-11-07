"""Validate upstream-compatible Bedrock jump events and legacy impulse fixtures.

LeviLamina 26.60 publishes PlayerJumpEvent after handling authenticated
input with grounded/ascending checks. A separate listener may cancel it;
this is an event count, not independent proof of a completed world jump.
"""
import json

JUMP_SOURCE = 'll26.60.jump_pos_delta.v1'  # historical producer/fixture cause
JUMP_EVENT_SOURCE = 'll26.60.player_jump_event.v1'


def valid_jump_details(raw):
    if not isinstance(raw, str) or len(raw) > 512:
        return 0
    try:
        d = json.loads(raw)
    except (ValueError, RecursionError):
        return 0
    if not isinstance(d, dict):
        return 0
    if d.get('hook') == 'LeviLamina::PlayerJumpEvent':
        return int(
            d.get('phase') == 'post_auth_input'
            and d.get('grounded_and_ascending_checked_by_emitter') is True
            and d.get('final_world_jump_verified') is False
        )
    if not all(type(d.get(k)) is int for k in ('before_y_micro', 'after_y_micro', 'dimension')):
        return 0
    before, after = d['before_y_micro'], d['after_y_micro']
    return int(
        d.get('hook') == 'Mob::jumpFromGround'
        and d.get('phase') == 'post_upward_impulse'
        and d.get('measurement') == 'pos_delta_y_x1e6'
        and d.get('grounded_before') is True
        and d.get('vehicle_before') is False
        and d.get('vehicle_after') is False
        and d.get('world_jump_verified') is False
        and -16000000 <= before <= 16000000
        and 0 < after <= 16000000
        and after > before
        and -(2**31) <= d['dimension'] < 2**31
    )
