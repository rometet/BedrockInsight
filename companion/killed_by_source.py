"""Resolved source-actor type observation, separate from death-cause history."""
import json

KILLED_BY_SOURCE = 'll26.60.resolved_killer_dead_transition.v1'


def _invalid_constant(_value):
    raise ValueError('non-standard JSON numeric literal')


def _unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError('duplicate attribution field')
        result[key] = value
    return result


def valid_killed_by_details(raw, path, *, hook='Player::$die'):
    if not isinstance(raw, str) or len(raw) > 1024:
        return 0
    try:
        d = json.loads(raw, object_pairs_hook=_unique_object, parse_constant=_invalid_constant)
    except (ValueError, RecursionError):
        return 0
    if not isinstance(d, dict):
        return 0
    flags = {'before_dead': False, 'after_dead': True, 'nested': False,
             'source_entity': True, 'source_block': False, 'resolved_actor': True,
             'same_level': True, 'self': False, 'world_death_verified': False}
    if d.get('hook') != hook or d.get('phase') != 'post_dead_transition' or not all(
            d.get(key) is value for key, value in flags.items()):
        return 0
    for key in ('source_id', 'damaging_id', 'damage_cause'):
        if type(d.get(key)) is not int or not -(2**63) <= d[key] < 2**63:
            return 0
    if not d['source_id'] or not d['damaging_id']:
        return 0
    direct = (path == 'direct' and d['damage_cause'] == 2 and d.get('source_child') is False
              and d['source_id'] == d['damaging_id'])
    child = (path == 'child_projectile' and d['damage_cause'] == 3 and d.get('source_child') is True
             and d['source_id'] != d['damaging_id'])
    return int(direct or child)
