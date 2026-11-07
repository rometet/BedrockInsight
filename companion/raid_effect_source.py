"""Changed raid-related effects, not real raid causation/outcomes."""
SOURCE='ll26.60.raid_effect_changed.v1'
EFFECTS={'minecraft:raid_trigger':(36,600),'minecraft:raid_win':(29,48000)}


def valid(primary,secondary,amount,d):
    if primary not in EFFECTS or type(amount) is not int or amount!=1:return 0
    effect,duration=EFFECTS[primary]
    if not all(type(d.get(k)) is int for k in ('effect_id','duration_ticks','amplifier',
        'before_duration_ticks','before_amplifier','after_duration_ticks','after_amplifier')):return 0
    if type(d.get('before_present')) is not bool:return 0
    before=(d['before_duration_ticks'],d['before_amplifier'])
    after=(d['after_duration_ticks'],d['after_amplifier'])
    return int(secondary=='effect_id:'+str(effect) and d.get('hook')=='Actor::addEffect' and
        d.get('phase')=='post_origin_effect_change' and d['effect_id']==effect and
        d['duration_ticks']==duration and 0<=d['amplifier']<=255 and after==(duration,d['amplifier']) and
        ((not d['before_present'] and before==(0,0)) or (d['before_present'] and
            -1<=before[0]<=2**31-1 and 0<=before[1]<=255 and before!=after)) and
        d.get('same_player') is True and d.get('same_level') is True and d.get('nested') is False and
        d.get('raid_outcome_verified') is False and d.get('effect_source_verified') is False and
        d.get('durable_world_commit_verified') is False)
