import json

from test_analytics_companion import BASE, Fixture, fixture_writer

CRAFT = 'll26.60.craft_request_success.v1'
TRAVEL = 'll26.60.poll_position_delta.v1'
INTERACT = 'll26.60.block_use_success.v1'
CAKE = 'll26.60.cake_remove_slice.v1'
HURT = 'll26.60.actor_hurt_event.v1'
DEALT = 'll26.60.actor_hurt_real_damage.v1'
BROKEN = 'll26.60.durability_item_destroyed.v1'
SLEEP = 'll26.60.sleep_flag_transition.v1'
SNEAK = 'll26.60.sneak_duration.v1'


class ParityActivityTests(Fixture):
    def insert(self, kind, cause, primary, secondary='', amount=1,
               pid='private-uuid-a', at=BASE + 1000, details='{}'):
        with fixture_writer(self.path) as db:
            db.execute('''INSERT INTO events(occurred_at_ms,event_kind,actor_kind,actor_id,cause,amount,
                primary_type,secondary_type,details_json) VALUES(?,?,'player',?,?,?,?,?,?)''',
                       (at, kind, pid, cause, amount, primary, secondary, details))

    def test_crafted_success_by_type_and_actions(self):
        self.player()
        self.player('bob', 'Bob')
        self.insert('item_craft_result', CRAFT, 'minecraft:stick', amount=12)
        self.insert('item_craft_result', CRAFT, 'minecraft:stick', amount=4)
        self.insert('item_craft_result', CRAFT, 'minecraft:crafting_table', amount=1)
        self.insert('item_craft_result', CRAFT, 'minecraft:stick', amount=8, pid='bob')
        # Rejected: failed/attempt cause, zero/oversized amount, bad type, hand secondary.
        self.insert('item_craft', 'levilamina_player_event', 'minecraft:stick', amount=1)
        self.insert('item_craft_result', CRAFT + '.attempt', 'minecraft:stick', amount=1)
        # Anvil/enchant results share the handler but use a distinct cause.
        self.insert('item_craft_result', 'll26.60.anvil_result.v1', 'minecraft:stick', amount=1)
        self.insert('item_craft_result', CRAFT, 'minecraft:stick', amount=0)
        self.insert('item_craft_result', CRAFT, 'minecraft:stick', amount=5000)
        self.insert('item_craft_result', CRAFT, 'minecraft:<svg>', amount=1)
        self.insert('item_craft_result', CRAFT, 'minecraft:stick', secondary='main_hand', amount=1)
        r = self.report(metric='crafted_items')
        self.assertEqual(r['totals']['crafted_items'], 25)
        self.assertEqual(r['totals']['craft_actions'], 4)
        self.assertEqual([x['value'] for x in r['ranking']], [17, 8])
        self.assertEqual(r['items_crafted']['types'], [
            {'type': 'minecraft:stick', 'items': 24, 'actions': 3},
            {'type': 'minecraft:crafting_table', 'items': 1, 'actions': 1}])
        self.assertIn('全クラフト経路', r['items_crafted']['definition'])

    def test_travel_modes_distance_and_ranking(self):
        self.player()
        self.player('bob', 'Bob')
        self.insert('player_travel_distance', TRAVEL, 'walk', amount=300)
        self.insert('player_travel_distance', TRAVEL, 'walk', amount=200)
        self.insert('player_travel_distance', TRAVEL, 'sprint', amount=1000)
        self.insert('player_travel_distance', TRAVEL, 'boat', amount=500, pid='bob')
        # Rejected: wrong cause, zero/negative, non-integer, overlong mode key.
        self.insert('player_travel_distance', TRAVEL + '.x', 'walk', amount=10)
        self.insert('player_travel_distance', TRAVEL, 'walk', amount=0)
        self.insert('player_travel_distance', TRAVEL, 'walk', amount=-5)
        self.insert('player_travel_distance', TRAVEL, 'walk', amount=1.5)
        self.insert('player_travel_distance', TRAVEL, 'x' * 40, amount=5)
        r = self.report(metric='travel_cm')
        self.assertEqual(r['totals']['travel_cm'], 2000)
        self.assertEqual([x['value'] for x in r['ranking']], [1500, 500])
        self.assertEqual(r['travel_distance']['modes'], [
            {'mode': 'sprint', 'cm': 1000}, {'mode': 'boat', 'cm': 500}, {'mode': 'walk', 'cm': 500}])
        self.assertIn('全移動距離の証明ではありません', r['travel_distance']['definition'])

    def test_interactions_require_mapped_block_type(self):
        self.player()
        self.insert('player_interaction_result', INTERACT, 'open_chest', secondary='minecraft:chest')
        self.insert('player_interaction_result', INTERACT, 'open_chest', secondary='minecraft:chest')
        self.insert('player_interaction_result', INTERACT, 'open_barrel', secondary='minecraft:barrel')
        self.insert('player_interaction_result', CAKE, 'eat_cake_slice', secondary='minecraft:cake')
        # Rejected: invalid block secondary type, non-1 amount, wrong cause.
        self.insert('player_interaction_result', INTERACT, 'open_chest', secondary='<bad>')
        self.insert('player_interaction_result', INTERACT, 'open_chest', secondary='minecraft:chest', amount=2)
        self.insert('player_interaction_result', INTERACT + '.x', 'open_chest', secondary='minecraft:chest')
        # Rejected: the removed pre-outcome notification is never a success.
        self.insert('player_interaction_result', 'll26.60.player_interact_block_event.v1',
                    'open_chest', secondary='minecraft:chest')
        r = self.report(metric='interactions')
        self.assertEqual(r['totals']['interactions'], 4)
        self.assertEqual(r['block_interactions']['types'], [
            {'key': 'open_chest', 'count': 2}, {'key': 'eat_cake_slice', 'count': 1},
            {'key': 'open_barrel', 'count': 1}])
        self.assertIn('画面の実描画や取引成立は保証しません', r['block_interactions']['definition'])

    def test_damage_taken_amount_and_dealt_real_damage(self):
        self.player()
        self.insert('player_damage_taken', HURT, 'cause:4', secondary='entity', amount=32)
        self.insert('player_damage_taken', HURT, 'cause:4', secondary='entity', amount=18)
        # Real pre-origin damage, attributed by damaging-entity unique id.
        self.insert('player_damage_dealt', DEALT, 'minecraft:zombie', secondary='cause:2', amount=40)
        self.insert('player_damage_dealt', DEALT, 'minecraft:zombie', secondary='cause:2', amount=25)
        # Rejected: old attack-action cause is NOT damage; zero amount; bad type.
        self.insert('player_damage_dealt', 'll26.60.player_attack_event.v1',
                    'minecraft:zombie', amount=1)
        self.insert('player_damage_taken', HURT, 'cause:4', amount=0)
        self.insert('player_damage_taken', HURT + '.x', 'cause:4', amount=5)
        self.insert('player_damage_dealt', DEALT, '<bad>', amount=1)
        r = self.report(metric='damage_taken_tenths')
        self.assertEqual(r['totals']['damage_taken_tenths'], 50)
        self.assertEqual(r['totals']['damage_dealt_tenths'], 65)
        self.assertEqual(r['damage']['kinds'], [{'kind': 'entity', 'tenths': 50, 'count': 2}])
        self.assertIn('攻撃行動の回数をダメージとして代用しません', r['damage']['definition'])

    def test_broken_sleep_and_sneak_counters(self):
        self.player()
        self.insert('item_broken_result', BROKEN, 'minecraft:iron_pickaxe', amount=1)
        self.insert('item_broken_result', BROKEN, 'minecraft:iron_pickaxe', amount=1)
        # Slot loss alone must not count as broken.
        self.insert('item_hurt_slot_loss', 'll26.60.hurt_slot_empty.v1',
                    'minecraft:iron_pickaxe', amount=1)
        self.insert('player_sleep_result', SLEEP, 'minecraft:sleep_in_bed', amount=1)
        self.insert('player_sleep_result', SLEEP, 'minecraft:sleep_in_bed', amount=2)
        self.insert('player_sneak_time', SNEAK, 'sneak', amount=120)
        self.insert('player_sneak_time', SNEAK, 'sneak', amount=80)
        r = self.report(metric='broken_items')
        self.assertEqual(r['totals']['broken_items'], 2)
        self.assertEqual(r['totals']['sleep_successes'], 1)
        self.assertEqual(r['totals']['sneak_time_ticks'], 200)
        self.assertEqual(r['broken_items']['types'], [
            {'type': 'minecraft:iron_pickaxe', 'count': 2}])
        self.assertEqual(r['sleep']['count'], 1)
        self.assertEqual(r['sneak']['ticks'], 200)

    def test_absent_new_records_are_unknown_not_zero(self):
        self.player()
        self.event('session_start', cause='levilamina_player_event')
        r = self.report(period='all')
        for metric in ('crafted_items', 'travel_cm', 'interactions',
                       'damage_taken_tenths', 'damage_dealt_tenths', 'broken_items',
                       'sleep_successes', 'sneak_time_ticks'):
            self.assertIsNone(r['totals'][metric])
        self.assertNotIn('private-uuid', json.dumps(r))
