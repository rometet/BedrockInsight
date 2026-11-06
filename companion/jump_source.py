"""Ground-jump method's direct upward state change, not packet or flight count."""
import json

JUMP_SOURCE='ll26.60.jump_pos_delta.v1'


def valid_jump_details(raw):
    if not isinstance(raw,str) or len(raw)>512:
        return 0
    try:
        d=json.loads(raw)
    except (ValueError,RecursionError):
        return 0
    if not isinstance(d,dict):
        return 0
    if not all(type(d.get(k)) is int for k in ('before_y_micro','after_y_micro','dimension')):
        return 0
    before,after=d['before_y_micro'],d['after_y_micro']
    return int(d.get('hook')=='Mob::jumpFromGround' and d.get('phase')=='post_upward_impulse' and
               d.get('measurement')=='pos_delta_y_x1e6' and d.get('grounded_before') is True and
               d.get('vehicle_before') is False and d.get('vehicle_after') is False and
               d.get('world_jump_verified') is False and -16000000<=before<=16000000 and
               0<after<=16000000 and after>before and -(2**31)<=d['dimension']<2**31)
