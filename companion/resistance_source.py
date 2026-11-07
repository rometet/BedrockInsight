"""Versioned resistance formula on a correlated post-enchantment stage basis."""
from .analytics import valid_item_type
SOURCE='ll26.60.processed_hurt_resistance_formula.v1'
KEYS=('minecraft:damage_resisted','minecraft:damage_dealt_resisted')
DEFINITION=('同一のPlayer/Mob被ダメージ呼出し内で、同じ対象・sourceのエンチャント軽減後の値と耐性効果を観測。'
    '外側がDamageProcessedを返し効果・Level/次元が維持された場合のみ、耐性I〜Vの20〜100%式で算出します。'
    '最終HP差や実際の耐性適用量を実測した値ではありません。milliに丸めてから1/10HPへ丸めます。'
    '複数/再入/別対象stage、効果変化、永久/無期限など期間<=0、NotHurt/EffectsOnly、全経路は除外。'
    '加害側は同一Levelへ解決した直接/子飛び道具source Playerのみ。完全収集・実機受入は未証明。')

def valid(primary,secondary,amount,d):
    ints=('effect_id','amplifier','duration_ticks','hurt_input_milli','stage_input_milli','basis_milli',
          'returned_milli','result_type','stage_calls','parent_calls','damage_cause','source_id','damaging_id')
    if (primary not in KEYS or type(amount) is not int or not 1<=amount<=1000000 or
        not all(type(d.get(k)) is int and -(2**63)<=d[k]<2**63 for k in ints)):return 0
    if (d.get('hook') not in ('Player::$_hurt','Mob::$_hurt') or d.get('stage_hook')!='Mob::$getDamageAfterEnchantReduction' or
        d.get('phase')!='processed_hurt_resistance_formula' or d.get('calculation')!='post_enchant_milli_times_capped_level_over_5' or
        d.get('rounding')!='nearest_milli_then_tenth' or d['effect_id']!=11 or not 0<=d['amplifier']<=255 or
        not 1<=d['duration_ticks']<2**31 or not 1<=d['hurt_input_milli']<=100000000 or
        not 1<=d['stage_input_milli']<=min(100000000,d['hurt_input_milli']+1) or
        not 1<=d['basis_milli']<=min(100000000,d['stage_input_milli']+1) or not 0<=d['returned_milli']<=100000000 or
        amount!=(d['basis_milli']*min(d['amplifier']+1,5)+250)//500 or d['result_type']!=2 or d['stage_calls']!=1 or
        not 0<=d['parent_calls']<=1 or (d['hook']=='Mob::$_hurt' and d['parent_calls']!=0) or
        not all(d.get(k) is True for k in ('same_actor','same_level','same_source','same_effect')) or d.get('ambiguous') is not False or
        d.get('victim_type')!=secondary or not valid_item_type(secondary) or not -1<=d['damage_cause']<=34 or
        not all(d.get(k) is False for k in ('resistance_applied_measured','final_health_loss_measured','durable_world_commit_verified'))):return 0
    if primary==KEYS[0]:return int(secondary=='minecraft:player' and d.get('role')=='victim' and d.get('attribution')=='victim'
                                  and d['source_id']==d['damaging_id']==0)
    return int(d.get('role')=='attacker' and d['source_id']!=0 and d['damaging_id']!=0 and
        ((d.get('attribution')=='direct' and d['damage_cause']==2 and d['source_id']==d['damaging_id']) or
         (d.get('attribution')=='child_projectile' and d['damage_cause']==3 and d['source_id']!=d['damaging_id'])))
