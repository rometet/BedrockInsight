"""Positive hurtAndBreak response + same main-inventory slot becoming empty.

This is not a completed/durable tool-break statistic and not all item locations.
"""
import json

HURT_SOURCE='ll26.60.hurt_slot_empty.v1'


def valid_hurt_details(raw):
    if not isinstance(raw,str) or len(raw)>512:
        return 0
    try:
        d=json.loads(raw)
    except (ValueError,RecursionError):
        return 0
    if not isinstance(d,dict) or not all(type(d.get(k)) is int for k in
        ('delta_damage','slots','slot','entry_units','after_units')):
        return 0
    return int(d.get('hook')=='ItemStackBase::hurtAndBreak' and
        d.get('phase')=='return_true_slot_empty' and d.get('scope')=='main_inventory_same_slot' and
        d.get('nested') is False and d.get('break_completed_verified') is False and
        1<=d['delta_damage']<=2147483647 and 1<=d['slots']<=128 and 0<=d['slot']<d['slots'] and
        d['entry_units']==1 and d['after_units']==0)
