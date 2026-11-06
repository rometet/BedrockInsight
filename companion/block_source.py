"""Proven post-outcome block transitions. Attempt events stay separate."""
import json
import re

MINED_SOURCE = 'll26.60.gamemode_destroy_block.v1'
PLACED_SOURCE = 'll26.60.gamemode_build_block.v1'
_TYPE = re.compile(r"[a-z0-9_.-]+:[a-z0-9_./-]+")


def _type(value):
    return isinstance(value, str) and len(value) <= 128 and _TYPE.fullmatch(value) is not None


def valid_block_details(raw, kind, primary):
    """True only for a stored same-position state transition of the right kind.

    Rejects tampered/attempt rows, cross-kind details, air results and any
    primary_type that does not equal the proven before/after block type.
    """
    if kind not in ('mined', 'placed') or not _type(primary):
        return False
    if not isinstance(raw, str) or len(raw) > 768:
        return False
    try:
        d = json.loads(raw)
    except (ValueError, RecursionError):
        return False
    if not isinstance(d, dict):
        return False
    hook = 'GameMode::$destroyBlock' if kind == 'mined' else 'GameMode::$buildBlock'
    if (d.get('hook') != hook or d.get('phase') != 'return_true_post_state' or
            d.get('scope') != 'same_position_block_source' or
            d.get('nested') is not False or
            d.get('state_transition_verified') is not True or
            d.get('world_save_verified') is not False):
        return False
    before, after = d.get('before_type'), d.get('after_type')
    if not _type(before) or not _type(after) or before == after:
        return False
    dimension = d.get('dimension')
    if type(dimension) is not int or not -1 <= dimension <= 255:
        return False
    if kind == 'mined':
        if before == 'minecraft:air' or primary != before:
            return False
    else:
        if after == 'minecraft:air' or primary != after:
            return False
        if type(d.get('hand')) is not int or not 0 <= d['hand'] <= 1:
            return False
        if type(d.get('sim_tick')) is not bool:
            return False
    return True
