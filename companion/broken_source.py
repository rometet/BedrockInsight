"""Versioned, bounded in-call durability/slot observations; not full break proof."""
import json
import re

V1='ll26.60.durability_item_destroyed.v1'
V2='ll26.60.durability_item_destroyed.v2'
BASE_FIELDS=frozenset(('hook','phase','scope','delta_damage','slots','slot','max_damage',
    'damage_before','is_damageable','entry_units','after_units','nested','durability_path'))
OLD_FIELDS=BASE_FIELDS|{'durability_exhausted_verified','max_damage_value_exceeded_read'}
NEW_FIELDS=BASE_FIELDS|{'item_type','same_player','same_level','same_slot','dimension',
    'threshold_input_reached','final_break_verified','world_save_verified'}


def _object(pairs):
    d={}
    for key,value in pairs:
        if key in d:raise ValueError('duplicate field')
        d[key]=value
    return d


def valid_broken_details(raw,primary,cause,secondary):
    if not isinstance(raw,str) or len(raw)>1024 or not isinstance(primary,str) or len(primary)>128 or not re.fullmatch(r'[a-z0-9_.-]+:[a-z0-9_./-]+',primary):return 0
    try:
        if len(raw.encode('utf-8'))>1024:return 0
        d=json.loads(raw,object_pairs_hook=_object)
    except (ValueError,RecursionError,UnicodeError):return 0
    return valid(primary,cause,secondary,d)


def valid(primary,cause,secondary,d):
    v2=cause==V2
    if cause not in (V1,V2) or not isinstance(d,dict) or set(d)!=(NEW_FIELDS if v2 else OLD_FIELDS):return 0
    if d.get('hook')!='ItemStackBase::hurtAndBreak' or d.get('phase')!='return_true_item_destroyed' or d.get('is_damageable') is not True or d.get('nested') is not False or d.get('durability_path') is not True:return 0
    if not all(type(d.get(k)) is int for k in ('delta_damage','max_damage','damage_before','slots','slot','entry_units','after_units')):return 0
    if not (0<d['delta_damage']<2**31 and 1<d['max_damage']<=32767 and 0<=d['damage_before']<=d['max_damage'] and d['damage_before']+d['delta_damage']>=d['max_damage'] and d['entry_units']==1 and d['after_units']==0 and 0<=d['slot']<d['slots']<=128):return 0
    if not v2:
        return int(secondary=='' and d['scope']=='main_inventory_same_slot' and d['durability_exhausted_verified'] is True and d['max_damage_value_exceeded_read'] is True)
    scopes={'main_inventory':'main_inventory_same_slot','offhand':'offhand_same_slot','armor':'armor_same_slot'}
    if not isinstance(secondary,str) or secondary not in scopes or scopes[secondary]!=d['scope'] or d['item_type']!=primary:return 0
    if secondary=='offhand' and d['slots']!=1:return 0
    if secondary=='armor' and d['slots']!=4:return 0
    return int(all(d[k] is True for k in ('same_player','same_level','same_slot','threshold_input_reached')) and
        d['final_break_verified'] is False and d['world_save_verified'] is False and type(d['dimension']) is int and -1<=d['dimension']<=127)
