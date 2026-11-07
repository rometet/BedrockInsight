"""Same-call water-animal bucket gain; NOT fishing-rod loot or all catches."""
SOURCE = 'll26.60.fish_bucket_delta.v1'
BUCKETS = {'minecraft:cod':'minecraft:cod_bucket','minecraft:fish':'minecraft:cod_bucket',
           'minecraft:salmon':'minecraft:salmon_bucket','minecraft:pufferfish':'minecraft:pufferfish_bucket',
           'minecraft:tropicalfish':'minecraft:tropical_fish_bucket','minecraft:axolotl':'minecraft:axolotl_bucket',
           'minecraft:tadpole':'minecraft:tadpole_bucket'}


def valid(primary, secondary, amount, d):
    integer=lambda k:type(d.get(k)) is int
    if (primary!='minecraft:fish_caught' or secondary not in BUCKETS or type(amount) is not int or amount!=1 or
        not all(integer(k) for k in ('entry_bucket_units','bucket_calls','slots','before_units','after_units'))):return 0
    return int(d.get('hook')=='Player::interact' and d.get('child_hook')=='BucketItem::$_useOn' and
        d.get('phase')=='return_success_bucket_gain_target_retired' and d.get('scope')=='main_inventory_type_total' and
        d.get('target_type')==secondary and d.get('bucket_type')==BUCKETS[secondary] and
        d.get('hand')=='main_hand' and d.get('entry_bucket_type')=='minecraft:water_bucket' and
        d['entry_bucket_units']==1 and d['bucket_calls']==1 and
        all(d.get(k) is True for k in ('interaction_success','bucket_swing','target_retired','same_call_target')) and
        d.get('target_after') in ('removed_flag','weak_unavailable') and
        d.get('nested') is False and 1<=d['slots']<=128 and 0<=d['before_units']<d['slots']*255 and
        d['after_units']==d['before_units']+1 and d['after_units']<=d['slots']*255 and
        d.get('fishing_rod') is False and d.get('durable_world_commit_verified') is False)
