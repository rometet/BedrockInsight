"""Proven post-outcome block transitions. Attempt events stay separate."""
import json
import re

MINED_SOURCE = 'll26.60.gamemode_destroy_block.v1'
PLACED_SOURCE = 'll26.60.gamemode_build_block.v1'
MINED_V2_SOURCE = 'll26.60.gamemode_destroy_block.v2'
PLACED_V2_SOURCE = 'll26.60.gamemode_build_block.v2'
_TYPE = re.compile(r"[a-z0-9_.-]+:[a-z0-9_./-]+")


def _type(value):
    return isinstance(value, str) and len(value) <= 128 and _TYPE.fullmatch(value) is not None


def _unique(pairs):
    result={}
    for key,value in pairs:
        if key in result:raise ValueError('duplicate block evidence field')
        result[key]=value
    return result


def valid_block_details(raw, kind, primary, cause=None, secondary=None):
    """True only for a stored same-position state transition of the right kind.

    Rejects tampered/attempt rows, cross-kind details, air results and any
    primary_type that does not equal the proven before/after block type.
    """
    if kind not in ('mined', 'placed') or not _type(primary):
        return False
    if not isinstance(raw, str) or len(raw) > 768:
        return False
    try:
        if len(raw.encode('utf-8'))>768:return False
        d = json.loads(raw,object_pairs_hook=_unique)
    except (ValueError, RecursionError, UnicodeError):
        return False
    if not isinstance(d, dict):
        return False
    v1,v2=(MINED_SOURCE,MINED_V2_SOURCE) if kind=='mined' else (PLACED_SOURCE,PLACED_V2_SOURCE)
    if cause is None:cause=v1 # Historical local helper interface only; SQL always passes actual cause.
    if cause not in (v1,v2):return False
    fields={'hook','phase','scope','before_type','after_type','dimension','nested','state_transition_verified','world_save_verified'}
    if kind=='placed':fields|={'hand','sim_tick'}
    if cause==v2:fields|={'same_player','same_level'}
    if set(d)!=fields or (cause==v2 and (d['same_player'] is not True or d['same_level'] is not True)):return False
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
    if type(dimension) is not int or not -1 <= dimension <= (127 if cause==v2 else 255):
        return False
    if kind == 'mined':
        if before == 'minecraft:air' or primary != before or secondary not in (None,''):
            return False
    else:
        if after == 'minecraft:air' or primary != after:
            return False
        if type(d.get('hand')) is not int or not 0 <= d['hand'] <= 1:
            return False
        if type(d.get('sim_tick')) is not bool:
            return False
        if secondary is not None and secondary!=('main_hand' if d['hand']==0 else 'off_hand'):
            return False
    return True
