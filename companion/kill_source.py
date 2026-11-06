"""Direct matching-source player attribution plus observed Mob dead transition."""
import json
from .killed_by_source import valid_killed_by_details

KILL_SOURCE='ll26.60.direct_attack_dead_transition.v1'
PROJECTILE_KILL_SOURCE='ll26.60.projectile_owner_dead_transition.v1'


def valid_projectile_kill_details(raw):
    if not valid_killed_by_details(raw, 'child_projectile', hook='Mob::$die'):
        return 0
    d=json.loads(raw)
    return int(d.get('resolved_player') is True and d.get('source_ids_equal') is False)


def valid_kill_details(raw):
    if not isinstance(raw,str) or len(raw)>512:
        return 0
    try:
        d=json.loads(raw)
    except (ValueError,RecursionError):
        return 0
    if not isinstance(d,dict) or type(d.get('damage_cause')) is not int or d['damage_cause']!=2:
        return 0
    flags={'source_entity':True,'source_child':False,'source_block':False,'source_ids_equal':True,
           'resolved_player':True,'self':False,'before_dead':False,'after_dead':True,'nested':False,
           'world_death_verified':False}
    return int(d.get('hook')=='Mob::$die' and d.get('phase')=='post_dead_transition' and
               all(d.get(k) is v for k,v in flags.items()))
