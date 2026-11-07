"""Observed ABSORPTION attribute decrease in a processed hurt call, not HP loss."""
from .analytics import valid_item_type
SOURCE='ll26.60.hurt_absorption_delta.v1'
KEYS=('minecraft:damage_absorbed','minecraft:damage_dealt_absorbed')


def valid(primary,secondary,amount,d):
    ints=('result_type','before_milli','after_milli','input_milli','returned_milli','parent_calls','damage_cause','source_id','damaging_id')
    if (primary not in KEYS or type(amount) is not int or not 1<=amount<=1000000 or
        not all(type(d.get(k)) is int and -(2**63)<=d[k]<2**63 for k in ints)):return 0
    before,after,inp=d['before_milli'],d['after_milli'],d['input_milli']
    if (d.get('hook') not in ('Player::$_hurt','Mob::$_hurt') or d.get('phase')!='post_origin_attribute_decrease' or
        d['result_type']!=2 or d.get('attribute')!='ABSORPTION' or d.get('scope')!='same_actor_same_attribute_instance' or
        d.get('rounding')!='nearest_milli_then_tenth' or not 0<=after<before<=100000000 or
        not 1<=inp<=100000000 or before-after>inp+1 or not 0<=d['returned_milli']<=100000000 or
        amount!=(before-after+50)//100 or not 0<=d['parent_calls']<=1 or
        (d['hook']=='Mob::$_hurt' and d['parent_calls']!=0) or
        not all(d.get(k) is True for k in ('same_actor','same_attribute','same_level')) or d.get('ambiguous') is not False or
        d.get('victim_type')!=secondary or not valid_item_type(secondary) or not -1<=d['damage_cause']<=34 or
        not all(d.get(k) is False for k in ('final_health_loss_measured','exclusive_causation_verified','durable_world_commit_verified'))):return 0
    if primary==KEYS[0]:
        return int(secondary=='minecraft:player' and d.get('role')=='victim' and d.get('attribution')=='victim'
                   and d['source_id']==d['damaging_id']==0)
    return int(d.get('role')=='attacker' and d['source_id']!=0 and d['damaging_id']!=0 and
        ((d.get('attribution')=='direct' and d['damage_cause']==2 and d['source_id']==d['damaging_id']) or
         (d.get('attribution')=='child_projectile' and d['damage_cause']==3 and d['source_id']!=d['damaging_id'])))
