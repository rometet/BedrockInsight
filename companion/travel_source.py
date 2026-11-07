"""Versioned server-position contracts, not a proof of complete travel capture."""
import json

TRAVEL_V1 = 'll26.60.poll_position_delta.v1'
TRAVEL_V2 = 'll26.60.poll_position_delta.v2'
MODES = frozenset(('walk','sprint','crouch','swim','fly','aviate','fall','climb','boat','minecart',
                   'pig','strider','horse','nautilus','happy_ghast','walk_on_water','walk_under_water'))


def _object(pairs):
    result={}
    for key,value in pairs:
        if key in result:raise ValueError('duplicate key')
        result[key]=value
    return result


def _invalid(_):raise ValueError('non-standard number')


def valid_travel_v2(primary, amount, at, raw):
    if (primary not in MODES or type(amount) is not int or not 0 < amount <= 100000000 or
            type(at) is not int or not 0 <= at < 2**63 or not isinstance(raw,str) or len(raw)>2048):return 0
    try:d=json.loads(raw,object_pairs_hook=_object,parse_constant=_invalid)
    except (ValueError,RecursionError):return 0
    if not isinstance(d,dict):return 0
    integer=lambda key:type(d.get(key)) is int and -(2**63)<=d[key]<2**63
    if not all(integer(key) for key in ('distance_cm','first_ms','last_ms','dimension','sample_count',
                                       'flush_interval_ms','max_gap_us','max_step_cm')):return 0
    day=lambda ms:ms//86400000+(ms%86400000+32400000)//86400000
    return int(d.get('adapter')=='ServerLevelTickEvent' and d.get('phase')=='coherent_position_window' and
               d.get('clock')=='steady' and d.get('mode')==primary and d['distance_cm']==amount and
               0<=d['first_ms']<=d['last_ms']==at and day(d['first_ms'])==day(at) and
               -(2**31)<=d['dimension']<2**31 and 1<=d['sample_count']<=1000000 and
               d['flush_interval_ms']==5000 and d['max_gap_us']==1000000 and d['max_step_cm']==1000 and
               d.get('precision')=='floor_window_cm' and d.get('period_assignment')=='last_observation' and
               d.get('world_movement_verified') is False)
