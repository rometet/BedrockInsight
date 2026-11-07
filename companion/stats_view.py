"""Bounded, read-only nine-category/77-key projection. Not full gameplay acceptance."""
from dataclasses import dataclass
import json
import re
import sqlite3
import time

from .analytics import CTE, period_bounds, snapshot, valid_item_type
from .stats_catalog import CATEGORIES, CUSTOM, type_label

# Block-interaction CUSTOM keys, reproduced from LK-Stats v0.13.0
# src/mod/Stats/StatsCustom.h CustomInteractBlockMap. The map is the reader's
# independent check: a stored row only counts when the block type in
# secondary_type maps back to the key in primary_type.
BLOCK_USE_MAP = {
    'minecraft:anvil': 'interact_with_anvil',
    'minecraft:beacon': 'interact_with_beacon',
    'minecraft:blast_furnace': 'interact_with_blast_furnace',
    'minecraft:brewing_stand': 'interact_with_brewingstand',
    'minecraft:campfire': 'interact_with_campfire',
    'minecraft:cartography_table': 'interact_with_cartography_table',
    'minecraft:crafting_table': 'interact_with_crafting_table',
    'minecraft:furnace': 'interact_with_furnace',
    'minecraft:grindstone': 'interact_with_grindstone',
    'minecraft:lectern': 'interact_with_lectern',
    'minecraft:loom': 'interact_with_loom',
    'minecraft:smithing_table': 'interact_with_smithing_table',
    'minecraft:smoker': 'interact_with_smoker',
    'minecraft:stonecutter_block': 'interact_with_stonecutter',
    'minecraft:dispenser': 'inspect_dispenser',
    'minecraft:dropper': 'inspect_dropper',
    'minecraft:hopper': 'inspect_hopper',
    'minecraft:barrel': 'open_barrel',
    'minecraft:chest': 'open_chest',
    'minecraft:ender_chest': 'open_enderchest',
    'minecraft:shulker_box': 'open_shulker_box',
    'minecraft:undyed_shulker_box': 'open_shulker_box',
    'minecraft:white_shulker_box': 'open_shulker_box',
    'minecraft:orange_shulker_box': 'open_shulker_box',
    'minecraft:magenta_shulker_box': 'open_shulker_box',
    'minecraft:light_blue_shulker_box': 'open_shulker_box',
    'minecraft:yellow_shulker_box': 'open_shulker_box',
    'minecraft:lime_shulker_box': 'open_shulker_box',
    'minecraft:pink_shulker_box': 'open_shulker_box',
    'minecraft:gray_shulker_box': 'open_shulker_box',
    'minecraft:light_gray_shulker_box': 'open_shulker_box',
    'minecraft:cyan_shulker_box': 'open_shulker_box',
    'minecraft:purple_shulker_box': 'open_shulker_box',
    'minecraft:blue_shulker_box': 'open_shulker_box',
    'minecraft:brown_shulker_box': 'open_shulker_box',
    'minecraft:green_shulker_box': 'open_shulker_box',
    'minecraft:red_shulker_box': 'open_shulker_box',
    'minecraft:black_shulker_box': 'open_shulker_box',
    'minecraft:bell': 'bell_ring',
    'minecraft:trapped_chest': 'trigger_trapped_chest',
    'minecraft:noteblock': 'tune_noteblock',
}
BLOCK_USE_KEYS = frozenset(BLOCK_USE_MAP.values())

# Dedicated LK-Stats block hooks, one CUSTOM key each.
CAKE_CAUSE = 'll26.60.cake_remove_slice.v1'
CAULDRON_USE_CAUSE = 'll26.60.cauldron_use_inventory.v1'
CAULDRON_CLEAR_CAUSE = 'll26.60.cauldron_clear_item.v1'
FLOWER_POT_CAUSE = 'll26.60.flower_pot_place.v1'
NOTE_BLOCK_CAUSE = 'll26.60.noteblock_attack.v1'
BLOCK_USE_CAUSE = 'll26.60.block_use_success.v1'

# Item producers ported from LK-Stats v0.13.0 PlayerItemStatsHandler.
USE_CONSUMED_CAUSE = 'll26.60.player_use_item_consumed.v1'
HURT_USED_CAUSE = 'll26.60.item_hurt_used.v1'
CREATED_OUTPUT_CAUSE = 'll26.60.created_output_transfer.v1'
FURNACE_OUTPUT_CAUSE = 'll26.60.furnace_output_extract.v1'
TRADE_OUTPUT_CAUSE = 'll26.60.trade_created_output.v1'
ENCHANT_OUTPUT_CAUSE = 'll26.60.enchant_output_transfer.v1'
PLAY_RECORD_CAUSE = 'll26.60.play_record_place.v1'
# Consuming ItemUseMethod names and their pinned 26.60 ordinals.
USE_METHOD_NAMES = frozenset(('eat', 'consume', 'throw', 'place', 'fill_bottle', 'pour_bucket', 'interact'))
USE_METHOD_IDS = frozenset((1, 3, 4, 6, 7, 9, 11))

# key -> (cause, producer block type) for the dedicated hooks.
DEDICATED_INTERACTION_KEYS = {
    'eat_cake_slice': (CAKE_CAUSE, 'minecraft:cake'),
    'use_cauldron': (CAULDRON_USE_CAUSE, 'minecraft:cauldron'),
    'fill_cauldron': (CAULDRON_USE_CAUSE, 'minecraft:cauldron'),
    'clean_shulker_box': (CAULDRON_CLEAR_CAUSE, 'minecraft:cauldron'),
    'clean_armor': (CAULDRON_CLEAR_CAUSE, 'minecraft:cauldron'),
    'clean_banner': (CAULDRON_CLEAR_CAUSE, 'minecraft:cauldron'),
    'pot_flower': (FLOWER_POT_CAUSE, 'minecraft:flower_pot'),
    'play_noteblock': (NOTE_BLOCK_CAUSE, 'minecraft:noteblock'),
}
INTERACTION_KEYS = BLOCK_USE_KEYS | frozenset(DEDICATED_INTERACTION_KEYS)


def _object(pairs):
    result = {}
    for k, v in pairs:
        if k in result:
            raise ValueError('duplicate field')
        result[k] = v
    return result


def _invalid(_):
    raise ValueError('non-standard JSON number')


def extra_details(cause, primary, secondary, amount, raw):
    """Exact serialized contracts only; a native boolean is not a world-commit proof."""
    if not isinstance(raw, str) or len(raw) > 4096:
        return 0
    try:
        d = json.loads(raw, object_pairs_hook=_object, parse_constant=_invalid)
    except (ValueError, RecursionError):
        return 0
    if not isinstance(d, dict) or type(amount) is not int or not 0 < amount < 2**63:
        return 0
    integer = lambda k: type(d.get(k)) is int and -(2**63) <= d[k] < 2**63
    if cause == 'll26.60.craft_request_success.v1':
        return int(d.get('hook') == 'CraftHandlerBase::postRequest' and
                   d.get('phase') == 'return_success_results' and d.get('result_type') == primary and
                   integer('quantity') and d['quantity'] == amount and amount <= 4096 and
                   integer('action_type') and 0 <= d['action_type'] < 2**31 and d.get('anvil_like') is False and
                   d.get('per_recipe_nbt_measured') is False and d.get('inventory_commit_verified') is False)
    if cause == 'll26.60.durability_item_destroyed.v1':
        return int(amount == 1 and d.get('hook') == 'ItemStackBase::hurtAndBreak' and
                   d.get('phase') == 'return_true_item_destroyed' and
                   d.get('scope') == 'main_inventory_same_slot' and
                   all(integer(k) for k in ('delta_damage', 'max_damage', 'damage_before', 'slots', 'slot',
                                            'entry_units', 'after_units')) and
                   0 < d['delta_damage'] < 2**31 and 1 < d['max_damage'] <= 32767 and
                   0 <= d['damage_before'] <= d['max_damage'] and
                   d['damage_before']+d['delta_damage'] >= d['max_damage'] and
                   1 <= d['slots'] <= 128 and 0 <= d['slot'] < d['slots'] and
                   d['entry_units'] == 1 and d['after_units'] == 0 and d.get('nested') is False and
                   all(d.get(k) is True for k in ('is_damageable', 'durability_path',
                                                 'durability_exhausted_verified', 'max_damage_value_exceeded_read')))
    if cause == 'll26.60.poll_position_delta.v1':
        return int(d.get('adapter') == 'ServerLevelTickEvent' and d.get('phase') == 'polled_position_delta' and
                   d.get('mode') == primary and integer('distance_cm') and d['distance_cm'] == amount and
                   integer('flush_interval_ms') and d['flush_interval_ms'] == 5000)
    if cause == 'll26.60.sleep_flag_transition.v1':
        return int(amount == 1 and d.get('adapter') == 'ServerLevelTickEvent' and
                   d.get('phase') == 'sleep_flag_false_to_true' and d.get('success_verified') is True and
                   d.get('wake_time_measured') is False)
    if cause == 'll26.60.sneak_duration.v1':
        phase = d.get('phase')
        return int(phase in ('sneak_stop_to_start', 'disconnect_finalize', 'shutdown_finalize') and
                   amount <= 72000 and d.get('units') == 'ticks' and d.get('wall_clock') is True and
                   (phase != 'sneak_stop_to_start' or
                    (integer('elapsed_us') and 0 < d['elapsed_us'] <= 3600000000 and
                     amount == max(1, d['elapsed_us']//50000))))
    if cause == 'll26.60.open_trading_villager.v1':
        return int(amount == 1 and primary == 'minecraft:talked_to_villager' and
                   secondary in ('minecraft:villager', 'minecraft:villager_v2', 'minecraft:wandering_trader') and
                   d.get('hook') == 'ServerPlayer::$openTrading' and d.get('phase') == 'post_origin_open' and
                   d.get('target_type') == secondary and d.get('trade_screen_opened') is True and
                   d.get('trade_completed_measured') is False)
    if cause == 'll26.60.breed_offspring.v1':
        return int(amount == 1 and primary == 'minecraft:animals_bred' and valid_item_type(secondary) and
                   d.get('hook') == 'BreedableComponent::mate' and d.get('phase') == 'post_origin_offspring' and
                   integer('offspring') and d['offspring'] > 0 and d.get('love_cause_resolved') is True)
    if cause == 'll26.60.target_block_projectile_hit.v1':
        return int(amount == 1 and primary == 'minecraft:target_hit' and valid_item_type(secondary) and
                   d.get('hook') == 'TargetBlock::$onProjectileHit' and d.get('phase') == 'post_origin_hit' and
                   d.get('owner_resolved_player') is True and d.get('projectile_type') == secondary)
    if cause == BLOCK_USE_CAUSE:
        return int(amount == 1 and primary in BLOCK_USE_KEYS and
                   BLOCK_USE_MAP.get(secondary) == primary and
                   d.get('hook') == 'Block::use' and d.get('phase') == 'post_origin_true' and
                   d.get('block_type') == secondary and d.get('custom_key') == 'minecraft:' + primary and
                   d.get('success') is True and d.get('nested') is False and
                   d.get('simulated_player') is False and d.get('interaction_success_verified') is True)
    if cause == CAKE_CAUSE:
        return int(amount == 1 and primary == 'eat_cake_slice' and secondary == 'minecraft:cake' and
                   d.get('hook') == 'CakeBlock::removeCakeSlice' and d.get('phase') == 'post_origin_return' and
                   d.get('block_type') == secondary and d.get('custom_key') == 'minecraft:' + primary and
                   d.get('nested') is False and d.get('simulated_player') is False and
                   d.get('interaction_success_verified') is True)
    if cause == CAULDRON_USE_CAUSE:
        item = d.get('item_type')
        return int(amount == 1 and primary in ('use_cauldron', 'fill_cauldron') and
                   secondary == 'minecraft:cauldron' and
                   (primary == 'use_cauldron') == (item == 'minecraft:bucket') and
                   item in ('minecraft:bucket', 'minecraft:water_bucket') and
                   integer('use_count') and 0 <= d['use_count'] < 2**31 and
                   d.get('hook') == 'CauldronBlock::_useInventory' and
                   d.get('phase') == 'post_origin_return' and
                   d.get('custom_key') == 'minecraft:' + primary and
                   d.get('nested') is False and d.get('simulated_player') is False)
    if cause == CAULDRON_CLEAR_CAUSE:
        return int(amount == 1 and
                   primary in ('clean_shulker_box', 'clean_armor', 'clean_banner') and
                   secondary == 'minecraft:cauldron' and
                   d.get('hook') == 'CauldronBlock::_sendCauldronUsedEventToClient' and
                   d.get('phase') == 'post_origin_clear_item' and
                   d.get('custom_key') == 'minecraft:' + primary and
                   d.get('nested') is False and d.get('simulated_player') is False)
    if cause == FLOWER_POT_CAUSE:
        return int(amount == 1 and primary == 'pot_flower' and secondary == 'minecraft:flower_pot' and
                   d.get('hook') == 'FlowerPotBlock::use' and d.get('phase') == 'post_origin_success' and
                   d.get('block_type') == secondary and d.get('custom_key') == 'minecraft:' + primary and
                   d.get('success') is True and d.get('nested') is False and
                   d.get('simulated_player') is False and d.get('interaction_success_verified') is True)
    if cause == NOTE_BLOCK_CAUSE:
        return int(amount == 1 and primary == 'play_noteblock' and secondary == 'minecraft:noteblock' and
                   d.get('hook') == 'NoteBlock::$attack' and d.get('phase') == 'post_origin_true' and
                   d.get('block_type') == secondary and d.get('custom_key') == 'minecraft:' + primary and
                   d.get('success') is True and d.get('nested') is False and
                   d.get('simulated_player') is False and d.get('interaction_success_verified') is True)
    if cause == USE_CONSUMED_CAUSE:
        method = d.get('use_method_name')
        return int(amount == 1 and valid_item_type(primary) and
                   d.get('hook') == 'Player::$useItem' and d.get('phase') == 'post_origin_consumed' and
                   d.get('item_type') == primary and secondary in USE_METHOD_NAMES and method == secondary and
                   integer('use_method') and method in USE_METHOD_NAMES and
                   d['use_method'] in USE_METHOD_IDS and d.get('consume_item') is True and
                   d.get('nested') is False and
                   (secondary != 'interact' or primary == 'minecraft:bone_meal'))
    if cause == HURT_USED_CAUSE:
        return int(amount == 1 and valid_item_type(primary) and secondary == 'durability' and
                   d.get('hook') == 'ItemStackBase::hurtAndBreak' and d.get('phase') == 'post_origin_true' and
                   d.get('item_type') == primary and integer('delta_damage') and d['delta_damage'] > 0 and
                   integer('max_damage') and 1 < d['max_damage'] <= 32767 and
                   integer('damage_before') and 0 <= d['damage_before'] <= d['max_damage'] and
                   d.get('armor') is False and d.get('nested') is False)
    if cause == CREATED_OUTPUT_CAUSE:
        return int(1 <= amount <= 255 and valid_item_type(primary) and
                   secondary in ('workbench', 'inventory', 'trade') and
                   d.get('hook') == 'ItemStackRequestActionHandler::_handleTransfer' and
                   d.get('phase') == 'post_origin_transfer_success' and d.get('item_type') == primary and
                   integer('transfer_amount') and d['transfer_amount'] == amount and
                   d.get('branch') == secondary and integer('screen_container_type') and
                   d.get('net_result') == 0 and d.get('nested') is False and
                   d.get('inventory_commit_verified') is True)
    if cause == FURNACE_OUTPUT_CAUSE:
        block = d.get('block_type')
        return int(1 <= amount <= 255 and valid_item_type(primary) and
                   block in ('minecraft:furnace', 'minecraft:lit_furnace', 'minecraft:smoker') and
                   secondary == block and
                   d.get('hook') == 'LevelContainerModel::$_onItemChanged' and
                   d.get('phase') == 'pre_origin_model_delta' and
                   integer('slot') and d['slot'] == 2 and integer('old_count') and integer('new_count') and
                   integer('extracted') and 0 <= d['new_count'] < d['old_count'] <= 255 and
                   d['extracted'] == d['old_count'] - d['new_count'] == amount and
                   d.get('nested') is False)
    if cause == TRADE_OUTPUT_CAUSE:
        return int(amount == 1 and primary == 'minecraft:traded_with_villager' and secondary == 'trade_output' and
                   d.get('hook') == 'ItemStackRequestActionHandler::_handleTransfer' and
                   d.get('phase') == 'post_origin_transfer_success' and valid_item_type(d.get('item_type')) and
                   integer('transfer_amount') and 1 <= d['transfer_amount'] <= 255 and
                   d.get('screen_container_type') == 15 and
                   d.get('villager_entity_identified') is False and d.get('nested') is False)
    if cause == ENCHANT_OUTPUT_CAUSE:
        return int(amount == 1 and primary == 'minecraft:enchant_item' and secondary == 'enchantment' and
                   d.get('hook') == 'ItemStackRequestActionHandler::_handleTransfer' and
                   d.get('phase') == 'post_origin_transfer_success' and valid_item_type(d.get('item_type')) and
                   integer('transfer_amount') and 1 <= d['transfer_amount'] <= 255 and
                   d.get('screen_container_type') == 3 and d.get('nested') is False)
    if cause == PLAY_RECORD_CAUSE:
        return int(amount == 1 and primary == 'minecraft:play_record' and valid_item_type(secondary) and
                   d.get('hook') == 'Player::$useItem' and d.get('phase') == 'post_origin_place' and
                   d.get('custom_key') == 'minecraft:play_record' and d.get('item_type') == secondary and
                   d.get('use_method') == 6 and d.get('music_disc') is True and d.get('nested') is False)
    if cause == 'll26.60.actor_hurt_event.v1':
        return int(0 < amount < 2**31 and primary.startswith('cause:') and
                   secondary in ('entity', 'environment') and
                   d.get('adapter') == 'ActorHurtEvent' and d.get('phase') == 'pre_origin_amount' and
                   integer('cause_code') and -1 <= d['cause_code'] <= 34 and
                   integer('damaging_entity_type') and d.get('units') == 'tenths' and
                   isinstance(d.get('entity_source'), bool) and
                   d.get('absorbed_or_resisted_measured') is False)
    if cause == 'll26.60.actor_hurt_real_damage.v1':
        return int(0 < amount < 2**31 and valid_item_type(primary) and primary != 'minecraft:player' and
                   isinstance(secondary, str) and secondary.startswith('cause:') and
                   d.get('adapter') == 'ActorHurtEvent' and d.get('phase') == 'pre_origin_amount' and
                   integer('cause_code') and -1 <= d['cause_code'] <= 34 and
                   d.get('units') == 'tenths' and
                   d.get('attribution') == 'damaging_entity_unique_id' and
                   d.get('attack_action_substituted') is False and
                   d.get('absorbed_or_resisted_measured') is False)
    if cause == 'll26.60.trade_result_transfer_success.v1':
        return int(amount == 1 and primary == 'minecraft:traded_with_villager' and secondary == 'trade_result' and
                   d.get('hook') == 'ItemStackRequestActionHandler::_handleTransfer' and
                   d.get('phase') == 'post_origin_trade_result_push' and
                   integer('src_container') and integer('dst_container') and integer('transfer_amount') and
                   d['src_container'] in (33, 49) and d['dst_container'] in (12, 28, 29, 59) and
                   0 <= d['transfer_amount'] <= 255 and d.get('net_result') == 0 and
                   d.get('trade_commit_verified') is True and
                   d.get('villager_entity_identified') is False and
                   d.get('ingredient_transfer_excluded') is True)
    return 0


@dataclass(frozen=True)
class Projection:
    sql: str
    definition: str
    sources: tuple


def _events(table, value='amount', *, extra=False, where='1'):
    # table/value/where are internal literals, NEVER caller-selected SQL text.
    return (f'SELECT actor_id,primary_type AS type,{value} AS value,occurred_at_ms AS first_ms,'
            f'occurred_at_ms AS last_ms FROM {table} WHERE {where}' +
            (' AND bi_stats_details(cause,primary_type,secondary_type,amount,details_json)' if extra else ''))


CATEGORY_SOURCES = {
    'mined': Projection(_events('mined'), 'GameModeのtrue応答と同座標のブロック種類変化のみ。全経路・保存完了ではありません。',
                        ('ll26.60.gamemode_destroy_block.v1',)),
    'broken': Projection(_events('broken', extra=True), '主所持品の同一スロット、耐久閾値・1→0消失の報告。防具・副手・全破損の網羅性は未検証。',
                         ('ll26.60.durability_item_destroyed.v1',)),
    'crafted': Projection(_events('crafted', extra=True),
                          'CreatedOutputContainerからの成功転送量と、かまど/燻製器の完成品取り出し差分。全クラフト経路・レシピNBTは未検証。',
                          (CREATED_OUTPUT_CAUSE, FURNACE_OUTPUT_CAUSE, 'll26.60.craft_request_success.v1')),
    'used': Projection(_events('use_results', '1'),
                       'Player::$useItemの消費系方法、または非防具の耐久消費が成功した後の観測。効果完了・全用途の成功ではありません。',
                       (USE_CONSUMED_CAUSE, HURT_USED_CAUSE, 'll26.60.gamemode_use_return.v1')),
    'picked_up': Projection(_events('pickups'), 'take true応答中の同一主所持品・同種類の増加差分。全拾得・永続転送ではありません。',
                            ('ll26.60.take_inventory_delta.v1',)),
    'dropped': Projection(_events('drops'), '非randomly投棄true応答の入口個数。所持品差分・生成物の保存は未検証。',
                          ('ll26.60.player_drop_returned_true.v1',)),
    'killed': Projection(_events('observed_kills', '1')+' UNION ALL '+_events('projectile_kills', '1'),
                         '直接攻撃または子飛び道具のPlayer所有者と同一被害者の死亡フラグ成立。全キル数は未検証。',
                         ('ll26.60.direct_attack_dead_transition.v1', 'll26.60.projectile_owner_dead_transition.v1')),
    'killed_by': Projection(_events('killed_by', '1'), '死亡入口で同じLevelへ解決できたsource actor種類。環境・自己・不明は除外し、原因コードを種類にしません。',
                            ('ll26.60.resolved_killer_dead_transition.v1',)),
}

CUSTOM_SOURCES = {}
for _key, _table, _value, _where, _extra, _source, _definition in (
    ('drop', 'drops', 'amount', '1', False, 'll26.60.player_drop_returned_true.v1', CATEGORY_SOURCES['dropped'].definition),
    ('jump', 'jumps', '1', '1', False, 'll26.60.jump_pos_delta.v1', '上向き加速の処理後観測。空中移動完了数ではありません。'),
    ('deaths', 'deaths', '1', '1', False, 'll26.60.player_dead_transition.v1', '同じ未削除Playerの死亡フラグ成立。処理前通知は含みません。'),
    ('sleep_in_bed', 'sleeps', '1', "primary_type='minecraft:sleep_in_bed'", True, 'll26.60.sleep_flag_transition.v1', 'サーバーtickで睡眠フラグfalse→true。睡眠時間は未計測。'),
    ('sneak_time', 'sneaks', 'amount', "primary_type='sneak'", True, 'll26.60.sneak_duration.v1', 'スニーク通知間の実時間を50msへ換算（各正区間の最小値1）。保存時刻に全量を配賦し期間境界は分割しません。真の継続サーバーtickではありません。'),
):
    CUSTOM_SOURCES[_key] = Projection('SELECT actor_id,:key AS type,value,first_ms,last_ms FROM ('+
                                     _events(_table, _value, extra=_extra, where=_where)+')', _definition, (_source,))
for _key, _kind in (('mob_kills', 'mob'), ('player_kills', 'player')):
    CUSTOM_SOURCES[_key] = Projection('SELECT actor_id,:key AS type,value,first_ms,last_ms FROM ('+
                                     CATEGORY_SOURCES['killed'].sql+") WHERE actor_id!='' AND type"+
                                     ("='minecraft:player'" if _kind == 'player' else "!='minecraft:player'"),
                                     CATEGORY_SOURCES['killed'].definition, CATEGORY_SOURCES['killed'].sources)
for _mode in ('walk', 'sprint', 'crouch', 'swim', 'fly', 'aviate', 'fall', 'climb', 'boat', 'minecart',
              'pig', 'strider', 'horse', 'nautilus', 'happy_ghast',
              'walk_on_water', 'walk_under_water'):
    CUSTOM_SOURCES[_mode+'_one_cm'] = Projection('SELECT actor_id,:key AS type,value,first_ms,last_ms FROM ('+
        _events('travels', extra=True, where='primary_type=:mode')+')',
        '5秒単位のサーバー位置差分観測。保存時刻で期間分けし、境界を跨ぐバッチの距離は復元しません。全モード・全テレポート除外は未検証。',
        ('ll26.60.poll_position_delta.v1',))
for _key, _cause, _definition in (
    ('animals_bred', 'll26.60.breed_offspring.v1',
     'mateの返却offspring非空とlove causeのPlayer解決を報告。子の生存・永続保存・実プレイ帰属は未検証。'),
    ('talked_to_villager', 'll26.60.open_trading_villager.v1',
     'openTrading正常終了後の村人target解決。画面の実表示や取引commitを確認した値ではありません。'),
    ('target_hit', 'll26.60.target_block_projectile_hit.v1',
     'ターゲットブロックへの飛び道具命中を処理後に観測し、飛び道具の所有者をPlayerへ解決した値。未所有・環境発射は数えません。'),
    ('enchant_item', ENCHANT_OUTPUT_CAUSE,
     'エンチャント画面のCreatedOutputContainerから成功転送された回数。材料転送や失敗は除外。'),
    ('play_record', PLAY_RECORD_CAUSE,
     'レコードを設置して再生した回数。設置以外の使用方法は数えません。'),
):
    CUSTOM_SOURCES[_key] = Projection('SELECT actor_id,:key AS type,value,first_ms,last_ms FROM ('+
        _events('view_events', extra=True, where="event_kind='player_interaction_result' AND cause=:cause AND primary_type=:key")+')',
        _definition, (_cause,))

# The trade counter accepts both the LK-Stats v0.13.0 created-output producer and
# the older trade-result-preview producer, so previously stored rows stay visible.
CUSTOM_SOURCES['traded_with_villager'] = Projection(
    'SELECT actor_id,:key AS type,value,first_ms,last_ms FROM ('+
    _events('view_events', extra=True,
            where="event_kind='player_interaction_result' AND cause IN "
                  "('ll26.60.trade_result_transfer_success.v1','ll26.60.trade_created_output.v1') "
                  "AND primary_type=:key")+')',
    '取引結果の成功転送で成立した取引回数。材料転送や失敗は除外し、村人実体は未特定です。',
    (TRADE_OUTPUT_CAUSE, 'll26.60.trade_result_transfer_success.v1'))

for _key in sorted(INTERACTION_KEYS):
    _block_use = _key in BLOCK_USE_KEYS
    CUSTOM_SOURCES[_key] = Projection(
        'SELECT actor_id,:key AS type,value,first_ms,last_ms FROM ('+
        _events('view_events', extra=True,
                where="event_kind='player_interaction_result' AND cause=:cause AND primary_type=:bare")+')',
        ('エンジン自身のBlock::useがtrueを返した後、同座標のブロック種別が上流の対応表と一致した場合のみ計数。'
         '画面へ実際に開いたかは未検証。' if _block_use else
         'LK-Stats v0.13.0の専用フック（ケーキ/大釜/植木鉢/音符ブロック）が成功条件を満たした後のみ計数。'),
        ((BLOCK_USE_CAUSE,) if _block_use else (DEDICATED_INTERACTION_KEYS[_key][0],)))
for _key, _kind, _cause, _definition in (
    ('damage_taken', 'player_damage_taken', 'll26.60.actor_hurt_event.v1',
     'ActorHurtEventの実際のダメージ値（1/10単位）。吸収・耐性後の内訳と最終適用HPは未計測。'),
    ('damage_dealt', 'player_damage_dealt', 'll26.60.actor_hurt_real_damage.v1',
     'ActorHurtEventの実ダメージ値を加害ActorUniqueID→解決済みPlayerへ帰属。攻撃回数で代用しません。'),
):
    CUSTOM_SOURCES[_key] = Projection(
        'SELECT actor_id,:key AS type,value,first_ms,last_ms FROM ('+
        _events('view_events', extra=True, where="event_kind='"+_kind+"' AND cause=:cause")+')',
        _definition, (_cause,))

_TIME_CTE = """
, view_events AS (SELECT * FROM events WHERE actor_kind='player' AND actor_id!=''
 AND typeof(amount)='integer' AND amount>0 AND typeof(occurred_at_ms)='integer'
 AND occurred_at_ms>=:start AND occurred_at_ms<:end AND occurred_at_ms<=:now
 AND EXISTS(SELECT 1 FROM players WHERE player_id=events.actor_id)),
 view_closed AS (SELECT player_id,session_id,MAX(started_at_ms,:start) AS b,MIN(ended_at_ms,:end) AS e
 FROM sessions WHERE typeof(started_at_ms)='integer' AND typeof(ended_at_ms)='integer'
 AND typeof(duration_ms)='integer' AND started_at_ms>=0 AND ended_at_ms>started_at_ms
 AND ended_at_ms<=:now AND duration_ms=ended_at_ms-started_at_ms
 AND ended_at_ms>:start AND started_at_ms<:end),
 view_ordered AS (SELECT *,MAX(e) OVER(PARTITION BY player_id ORDER BY b,e,session_id
 ROWS BETWEEN UNBOUNDED PRECEDING AND 1 PRECEDING) AS prior_e FROM view_closed),
 view_grouped AS (SELECT *,SUM(CASE WHEN prior_e IS NULL OR b>prior_e THEN 1 ELSE 0 END)
 OVER(PARTITION BY player_id ORDER BY b,e,session_id ROWS UNBOUNDED PRECEDING) AS g FROM view_ordered),
 view_merged AS (SELECT player_id,MIN(b) AS b,MAX(e) AS e FROM view_grouped GROUP BY player_id,g),
 view_lastdeath AS (SELECT actor_id,MAX(occurred_at_ms) AS t FROM deaths
 WHERE typeof(occurred_at_ms)='integer' AND occurred_at_ms>=0 GROUP BY actor_id)
"""
for _key in ('play_time', 'time_since_death'):
    _begin = 'b' if _key == 'play_time' else 'MAX(b,COALESCE(t,0))'
    CUSTOM_SOURCES[_key] = Projection(f'SELECT player_id AS actor_id,:key AS type,SUM(e-{_begin})/50 AS value,'
        f'MIN({_begin}) AS first_ms,MAX(e-1) AS last_ms FROM view_merged LEFT JOIN view_lastdeath ON actor_id=player_id '
        f'WHERE e>{_begin} GROUP BY player_id',
        '期間末までの完結セッションを重複統合し、当期間に重なる時間だけを50ms換算。未終了・未来・不正・オフライン時間を加算しない下限。'+
        ('当期間末の最後の死亡以後だけを算入。' if _key == 'time_since_death' else ''), ('closed_sessions',))
CUSTOM_SOURCES['leave_game'] = Projection("SELECT actor_id,:key AS type,1 AS value,occurred_at_ms AS first_ms,occurred_at_ms AS last_ms FROM view_events WHERE event_kind='session_end' AND cause='levilamina_player_event' AND amount=1",
    'ネイティブ切断通知から保存された退出イベント数。クラッシュや通信途絶による未記録退出は補いません。', ('session_end/levilamina_player_event',))


def catalog():
    return {'categories': [{'id': 'minecraft:'+k, 'label': label, 'unit': unit} for k, label, unit in CATEGORIES],
            'custom': [{'id': 'minecraft:'+k, 'label': label, 'unit': unit,
                        'projection_supported': k in CUSTOM_SOURCES, 'collection_complete': False}
                       for k, label, unit in CUSTOM]}


def report(analytics, *, category='custom', key=None, period='all', date=None, player=None,
           search='', type=None, page=0, rank_page=0, limit=20, players_page=0, now_ms=None):
    if not isinstance(category, str) or (key is not None and not isinstance(key, str)):
        raise ValueError('分類またはキーが不正です')
    category = category.removeprefix('minecraft:')
    cats = {k: (label, unit) for k, label, unit in CATEGORIES}
    if category not in cats or any(not isinstance(x, int) or isinstance(x, bool) or not 0 <= x <= 1000
                                   for x in (page, rank_page, players_page)) or not isinstance(limit, int) or isinstance(limit, bool) or not 1 <= limit <= 100:
        raise ValueError('分類またはページが範囲外です')
    if not isinstance(search, str) or len(search) > 64 or (type is not None and not valid_item_type(type)):
        raise ValueError('種類の検索・指定が不正です')
    if player is not None and not re.fullmatch(r'[0-9a-f]{32}', player):
        raise ValueError('プレイヤー指定が不正です')
    if category != 'custom' and key is not None:
        raise ValueError('CUSTOM以外ではキーを指定しません')
    key = key or ('minecraft:play_time' if category == 'custom' else None)
    short = key.removeprefix('minecraft:') if key else ''
    keys = {k: (label, unit) for k, label, unit in CUSTOM}
    if category == 'custom' and (not key.startswith('minecraft:') or short not in keys):
        raise ValueError('CUSTOMキーが不正です')
    selected_label, unit = keys[short] if category == 'custom' else cats[category]
    source = CUSTOM_SOURCES.get(short) if category == 'custom' else CATEGORY_SOURCES[category]
    now_ms = time.time_ns()//1_000_000 if now_ms is None else now_ms
    start, end, label = period_bounds(period, date, now_ms)
    args = {'start': start, 'end': min(end, now_ms+1), 'now': now_ms, 'key': key,
            'mode': short.removesuffix('_one_cm'), 'bare': short,
            'cause': source.sources[0] if source else '',
            'search': search, 'type': type, 'selected': None, 'limit': limit, 'offset': page*limit,
            'rank_offset': rank_page*limit, 'players_offset': players_page*100}
    sql = source.sql if source else "SELECT '' AS actor_id,:key AS type,0 AS value,0 AS first_ms,0 AS last_ms WHERE 0"
    prefix = CTE + _TIME_CTE + ', view_source AS ('+sql+"""), view_filtered AS (
 SELECT * FROM view_source WHERE typeof(value)='integer' AND value>0
 AND typeof(first_ms)='integer' AND typeof(last_ms)='integer'
 AND first_ms>=:start AND last_ms>=first_ms AND last_ms<:end AND last_ms<=:now
 AND EXISTS(SELECT 1 FROM players WHERE player_id=view_source.actor_id)
 AND (instr(type,:search)>0 OR instr(bi_stats_label(type),:search)>0)
 AND (:type IS NULL OR type=:type)), view_values AS (
 SELECT actor_id,SUM(value) AS value FROM view_filtered GROUP BY actor_id)
"""
    with snapshot(analytics.database) as db:
        db.create_function('bi_stats_details', 5, extra_details, deterministic=True)
        db.create_function('bi_stats_label', 1, type_label, deterministic=True)
        selected = None
        if player:
            for i, row in enumerate(db.execute('SELECT player_id,last_name FROM players ORDER BY player_id LIMIT 100001')):
                if i == 100000:
                    raise sqlite3.OperationalError('player lookup exceeds budget')
                if analytics.player_key(row['player_id']) == player:
                    args['selected'] = row['player_id']; selected = {'key': player, 'name': row['last_name']}; break
            if selected is None:
                raise LookupError('player not found')
        player_rows = db.execute(prefix+'''SELECT p.player_id,p.last_name,v.value FROM players p
            LEFT JOIN view_values v ON v.actor_id=p.player_id ORDER BY p.last_name COLLATE BINARY,p.player_id
            LIMIT 101 OFFSET :players_offset''', args).fetchall()
        players = [{'key': analytics.player_key(r['player_id']), 'name': r['last_name'], 'value': r['value']}
                   for r in player_rows[:100]]
        totals = dict(db.execute(prefix+'''SELECT SUM(value) AS value,COUNT(*) AS source_rows,
            MIN(first_ms) AS first_ms,MAX(last_ms) AS last_ms FROM view_filtered
            WHERE (:selected IS NULL OR actor_id=:selected)''', args).fetchone())
        type_count = db.execute(prefix+'''SELECT COUNT(DISTINCT type) FROM view_filtered
            WHERE (:selected IS NULL OR actor_id=:selected)''', args).fetchone()[0]
        types = [dict(r) for r in db.execute(prefix+'''SELECT type,SUM(value) AS value FROM view_filtered
            WHERE (:selected IS NULL OR actor_id=:selected) GROUP BY type
            ORDER BY value DESC,type COLLATE BINARY LIMIT :limit OFFSET :offset''', args)]
        ranking = [dict(r) for r in db.execute(prefix+'''SELECT p.player_id,p.last_name,v.value FROM view_values v
            JOIN players p ON p.player_id=v.actor_id WHERE v.value>0
            ORDER BY v.value DESC,p.last_name COLLATE BINARY,p.player_id
            LIMIT :limit OFFSET :rank_offset''', args)]
        rank_count = db.execute(prefix+'SELECT COUNT(*) FROM view_values WHERE value>0', args).fetchone()[0]
        for r in types:
            r['label'] = type_label(r['type'])
        for i, r in enumerate(ranking):
            r['key'] = analytics.player_key(r.pop('player_id')); r['name'] = r.pop('last_name')
            r['rank'] = rank_page*limit+i+1
    return {'catalog': catalog(), 'category': 'minecraft:'+category, 'key': key, 'label': selected_label, 'unit': unit,
            'period': {'label': label, 'timezone': 'JST (UTC+09:00)', 'start_ms': start, 'end_ms_exclusive': args['end']},
            'selected_player': selected, 'players': players, 'players_page': players_page, 'players_has_more': len(player_rows)>100,
            'totals': totals, 'types': types, 'type_count': type_count, 'page': page, 'limit': limit,
            'has_more': (page+1)*limit < type_count, 'ranking': ranking, 'rank_page': rank_page,
            'ranking_has_more': (rank_page+1)*limit < rank_count, 'ranking_scope': '全プレイヤー（同じ期間・種類検索）',
            'projection_supported': source is not None, 'status': 'unsupported' if source is None else
            'recorded_observation' if totals['source_rows'] else 'no_record', 'collection_complete': False,
            'native_runtime_acceptance': 'not_attested', 'sources': list(source.sources) if source else [],
            'definition': source.definition if source else '確定結果の読取経路は未対応。既存の試行・処理前通知・enum登録を数値へ昇格しません。',
            'limitations': '全9分類／77キーの収集・実プレイ受入は未完了。未記録は不明、異種単位のCUSTOM総合計は作りません。種類名は独立日本語ラベル＋未知IDの安全な表示。',
            'generated_at_ms': now_ms}
