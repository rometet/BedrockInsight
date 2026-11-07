import hashlib
import http.client
import json
from pathlib import Path
import re
import subprocess
import threading

from companion.stats_catalog import CATEGORIES, CUSTOM
from companion.stats_view import (BLOCK_USE_KEYS, BLOCK_USE_MAP, INTERACTION_KEYS, catalog, extra_details,
                                  report)
from companion.server import CompanionServer
from test_analytics_companion import Fixture, fixture_writer, BASE, NOW
from test_block_activity import mined_details
from test_killed_by_activity import DETAILS as KILLER, CHILD

CRAFT = {'hook':'CraftHandlerBase::postRequest','phase':'return_success_results',
         'result_type':'minecraft:stick','quantity':4,'action_type':12,'anvil_like':False,
         'per_recipe_nbt_measured':False,'inventory_commit_verified':False}
BROKEN = {'hook':'ItemStackBase::hurtAndBreak','phase':'return_true_item_destroyed',
          'scope':'main_inventory_same_slot','delta_damage':2,'max_damage':100,'damage_before':99,
          'slots':36,'slot':1,'is_damageable':True,'entry_units':1,'after_units':0,'nested':False,
          'durability_path':True,'durability_exhausted_verified':True,'max_damage_value_exceeded_read':True}
PICKUP = {'hook':'Player::take','phase':'return_true_inventory_delta','scope':'main_inventory_type_total',
          'entry_units':64,'slots':36,'before_units':63,'after_units':66,'nested':False,'durable_transfer_verified':False}
USE = {'hook':'GameMode::$useItem','phase':'return_true_inventory_snapshot','scope':'main_inventory_type_total',
       'entry_units':16,'slots':36,'before_units':16,'after_units':15,'loss_units':1,'nested':False,
       'effect_completed_verified':False,'consumption_verified':False}
DEATH = {'hook':'Player::$die','phase':'post_dead_transition','before_dead':False,
         'after_dead':True,'killer_attribution':'unmeasured'}


class StatsViewTests(Fixture):
    def row(self, kind, cause, item, amount=1, secondary='', details=None,
            pid='private-uuid-a', at=BASE+1000, actor='player'):
        with fixture_writer(self.path) as db:
            db.execute('''INSERT INTO events(occurred_at_ms,event_kind,actor_kind,actor_id,cause,amount,
                primary_type,secondary_type,details_json) VALUES(?,?,?,?,?,?,?,?,?)''',
                (at,kind,actor,pid,cause,amount,item,secondary,
                 details if isinstance(details,str) else json.dumps(details or {})))

    def view(self, **args):
        return report(self.analytics, now_ms=NOW, **args)

    def seed(self):
        self.player();self.player('z','Alice')
        self.session('closed', BASE, BASE+10000)
        self.row('block_mined_result','ll26.60.gamemode_destroy_block.v1','minecraft:stone',details=mined_details())
        self.row('item_broken_result','ll26.60.durability_item_destroyed.v1','minecraft:iron_pickaxe',details=BROKEN)
        self.row('item_craft_result','ll26.60.craft_request_success.v1','minecraft:stick',4,details=CRAFT)
        self.row('item_use_result','ll26.60.gamemode_use_return.v1','minecraft:snowball',secondary='main_hand',details=USE)
        self.row('item_pickup_delta','ll26.60.take_inventory_delta.v1','minecraft:stone',3,details=PICKUP)
        self.row('item_drop_result','ll26.60.player_drop_returned_true.v1','minecraft:stone',3)
        direct=KILLER | {'hook':'Mob::$die','resolved_player':True,'source_ids_equal':True}
        self.row('direct_kill_observation','ll26.60.direct_attack_dead_transition.v1','minecraft:zombie',secondary='mob',details=direct)
        self.row('player_killed_by_entity','ll26.60.resolved_killer_dead_transition.v1','minecraft:zombie',secondary='direct',details=KILLER)

    def test_exact_nine_and_ordered77_independent_labels_not_capture_proof(self):
        native=(Path(__file__).resolve().parents[1]/'src/bedrock_insight/core/stats_catalog.hpp').read_text(encoding='utf-8-sig')
        section=native.split('kCustomKeys{{')[1].split('}};')[0]
        self.assertEqual(['minecraft:'+k for k,_,_ in CUSTOM],re.findall(r'"(minecraft:[^"]+)"',section))
        self.assertEqual(len(CATEGORIES),9);self.assertEqual(len(CUSTOM),77)
        c=catalog();self.assertTrue(all(x['label']!=x['id'] for x in c['custom']))
        self.assertTrue(all(x['collection_complete'] is False for x in c['custom']))
        c['custom'][0]['id']='bad';self.assertEqual(catalog()['custom'][0]['id'],'minecraft:animals_bred')

    def test_every_key_selectable_with_honest_empty_or_unsupported_state(self):
        self.player()
        for key,_,_ in CUSTOM:
            with self.subTest(key=key):
                r=self.view(key='minecraft:'+key)
                self.assertIn(r['status'],('unsupported','no_record'))
                self.assertIsNone(r['totals']['value']);self.assertEqual(r['types'],[]);self.assertEqual(r['ranking'],[])
                self.assertFalse(r['collection_complete'])
        for category,_,_ in CATEGORIES:
            r=self.view(category='minecraft:'+category)
            self.assertIsNone(r['totals']['value'])

    def test_eight_categories_and_custom_actual_sql_fixture_with_units(self):
        self.seed()
        expected={'mined':1,'broken':1,'crafted':4,'used':1,'picked_up':3,'dropped':3,'killed':1,'killed_by':1,'custom':200}
        for category,value in expected.items():
            r=self.view(category=category)
            self.assertEqual(r['totals']['value'],value,category)
            self.assertEqual(r['ranking'][0]['value'],value,category)
            self.assertEqual(r['status'],'recorded_observation')
            self.assertNotIn('private-uuid',json.dumps(r));self.assertNotIn('source_id',json.dumps(r))
        self.assertEqual(self.view(category='picked_up')['unit'],'個')
        self.assertIn('全クラフト経路',self.view(category='crafted')['definition'])

    def test_pre_notifications_not_canonical_custom_and_invalid_details(self):
        self.player()
        self.row('player_interaction_result','ll26.60.player_interact_block_event.v1','open_chest',secondary='minecraft:chest')
        self.row('player_damage_taken','ll26.60.actor_hurt_event.v1','cause:2',200)
        # A pre-outcome notification and a detail-less hurt row have projections
        # now, but neither satisfies the confirmed contract: no_record, never a
        # fabricated success value.
        for key in ('open_chest','damage_taken','damage_dealt'):
            r=self.view(key='minecraft:'+key)
            self.assertEqual(r['status'],'no_record',key);self.assertIsNone(r['totals']['value'])
        for key in ('time_since_rest',):
            r=self.view(key='minecraft:'+key)
            self.assertEqual(r['status'],'no_record');self.assertIsNone(r['totals']['value'])
        fish=self.view(key='minecraft:fish_caught')
        self.assertEqual(fish['status'],'no_record');self.assertIsNone(fish['totals']['value'])
        # target_hit now has a real post-origin projection, so an attempt-only DB
        # is "no_record", never a fake unsupported→counted success.
        target=self.view(key='minecraft:target_hit')
        self.assertEqual(target['status'],'no_record');self.assertIsNone(target['totals']['value'])
        for d in (None,{'phase':'before_origin'},CRAFT | {'inventory_commit_verified':True},
                  CRAFT | {'quantity':True},CRAFT | {'quantity':5},CRAFT | {'anvil_like':True},
                  '{bad',json.dumps(CRAFT)[:-1]+',"quantity":4}',json.dumps(CRAFT)[:-1]+',"extra":NaN}'):
            self.row('item_craft_result','ll26.60.craft_request_success.v1','minecraft:stick',4,details=d)
        self.row('item_broken_result','ll26.60.durability_item_destroyed.v1','minecraft:iron_pickaxe',details=BROKEN | {'damage_before':2})
        self.row('item_hurt_slot_loss','ll26.60.hurt_slot_empty.v1','minecraft:iron_pickaxe')
        self.assertIsNone(self.view(category='crafted')['totals']['value'])
        self.assertIsNone(self.view(category='broken')['totals']['value'])

    def test_interaction_keys_match_native_catalog(self):
        native=(Path(__file__).resolve().parents[1]/
                'src/bedrock_insight/core/interaction_observation.hpp').read_text(encoding='utf-8-sig')
        block_use=native.split('kBlockUseKeys{{')[1].split('}};')[0]
        self.assertEqual(set(re.findall(r'"([a-z_]+)"',block_use)),set(BLOCK_USE_KEYS))
        section=native.split('kInteractionKeys{{')[1].split('}};')[0]
        self.assertEqual(set(re.findall(r'"([a-z_]+)"',section)),set(INTERACTION_KEYS))
        self.assertEqual(len(BLOCK_USE_KEYS),24)
        self.assertEqual(len(INTERACTION_KEYS),32)
        # The reader's own block map must reproduce the upstream values.
        self.assertEqual(BLOCK_USE_KEYS,frozenset(BLOCK_USE_MAP.values()))

    def test_confirmed_interaction_and_real_damage_are_counted(self):
        self.player()
        ok={'hook':'Block::use','phase':'post_origin_true','block_type':'minecraft:chest',
            'custom_key':'minecraft:open_chest','success':True,'nested':False,
            'simulated_player':False,'interaction_success_verified':True}
        args=('player_interaction_result','ll26.60.block_use_success.v1','open_chest')
        self.row(*args,secondary='minecraft:chest',details=ok)
        # A failed result, a nested call, a mistyped block and the pre-outcome
        # notification cause must not count.
        self.row(*args,secondary='minecraft:chest',details=ok|{'success':False})
        self.row(*args,secondary='minecraft:chest',details=ok|{'nested':True})
        self.row(*args,secondary='minecraft:chest',details=ok|{'custom_key':'minecraft:open_barrel'})
        self.row(*args,secondary='minecraft:barrel',details=ok)
        self.row('player_interaction_result','ll26.60.player_interact_block_event.v1','open_chest',
                 secondary='minecraft:chest')
        r=self.view(key='minecraft:open_chest')
        self.assertEqual(r['status'],'recorded_observation');self.assertEqual(r['totals']['value'],1)
        # The dedicated cauldron hook: bucket is a use, water bucket is a fill.
        cauldron={'hook':'CauldronBlock::_useInventory','phase':'post_origin_return',
                  'custom_key':'minecraft:fill_cauldron','item_type':'minecraft:water_bucket',
                  'use_count':1,'nested':False,'simulated_player':False}
        crows=('player_interaction_result','ll26.60.cauldron_use_inventory.v1','fill_cauldron')
        self.row(*crows,secondary='minecraft:cauldron',details=cauldron)
        # A bucket must never be counted as a fill, and vice versa.
        self.row(*crows,secondary='minecraft:cauldron',
                 details=cauldron|{'custom_key':'minecraft:use_cauldron','item_type':'minecraft:bucket'})
        self.assertEqual(self.view(key='minecraft:fill_cauldron')['totals']['value'],1)
        self.assertIsNone(self.view(key='minecraft:use_cauldron')['totals']['value'])
        # Cake and noteblock have their own producers only.
        self.row('player_interaction_result','ll26.60.cake_remove_slice.v1','eat_cake_slice',
                 secondary='minecraft:cake',
                 details={'hook':'CakeBlock::removeCakeSlice','phase':'post_origin_return',
                          'block_type':'minecraft:cake','custom_key':'minecraft:eat_cake_slice',
                          'nested':False,'simulated_player':False,'interaction_success_verified':True})
        self.assertEqual(self.view(key='minecraft:eat_cake_slice')['totals']['value'],1)
        hurt={'adapter':'ActorHurtEvent','phase':'pre_origin_amount','cause_code':2,
              'damaging_entity_type':3,'units':'tenths','entity_source':True,
              'absorbed_or_resisted_measured':False}
        self.row('player_damage_taken','ll26.60.actor_hurt_event.v1','cause:2',30,
                 secondary='entity',details=hurt)
        dealt={'adapter':'ActorHurtEvent','phase':'pre_origin_amount','cause_code':2,'units':'tenths',
               'attribution':'damaging_entity_unique_id','attack_action_substituted':False,
               'absorbed_or_resisted_measured':False}
        self.row('player_damage_dealt','ll26.60.actor_hurt_real_damage.v1','minecraft:zombie',40,
                 secondary='cause:2',details=dealt)
        # Legacy notification amounts remain in the DB, but cannot substitute
        # for the distinct HEALTH attribute-delta source.
        self.assertIsNone(self.view(key='minecraft:damage_taken')['totals']['value'])
        self.assertIsNone(self.view(key='minecraft:damage_dealt')['totals']['value'])
        trade={'hook':'ItemStackRequestActionHandler::_handleTransfer',
               'phase':'post_origin_trade_result_push','src_container':33,'dst_container':12,
               'transfer_amount':3,'net_result':0,'trade_commit_verified':True,
               'villager_entity_identified':False,'ingredient_transfer_excluded':True}
        rows=('player_interaction_result','ll26.60.trade_result_transfer_success.v1',
              'minecraft:traded_with_villager')
        self.row(*rows,secondary='trade_result',details=trade)
        self.row(*rows,secondary='trade_result',details=trade|{'dst_container':13})
        self.row(*rows,secondary='trade_result',details=trade|{'net_result':1})
        self.row(*rows,secondary='trade_result',details=trade|{'src_container':31})
        r=self.view(key='minecraft:traded_with_villager')
        self.assertEqual(r['status'],'recorded_observation');self.assertEqual(r['totals']['value'],1)

    def test_selected_player_global_type_ranking_ties_and_literal_search(self):
        self.player();self.player('z','Alice')
        for pid in ('private-uuid-a','z'):
            self.row('item_drop_result','ll26.60.player_drop_returned_true.v1','minecraft:stone',3,pid=pid)
        self.row('item_drop_result','ll26.60.player_drop_returned_true.v1','addon:fruit/red',2)
        r=self.view(category='dropped',search='石',limit=1)
        self.assertEqual(r['totals']['value'],6);self.assertEqual(r['types'][0]['label'],'石')
        self.assertEqual(r['ranking'][0]['key'],self.analytics.player_key('private-uuid-a'))
        r=self.view(category='dropped',type='minecraft:stone',limit=1,rank_page=1,
                    player=self.analytics.player_key('private-uuid-a'))
        self.assertEqual(r['totals']['value'],3)
        self.assertEqual(r['ranking'][0]['key'],self.analytics.player_key('z'))
        for text in ("' OR 1=1 --",'%', '_', '<svg>'):
            self.assertIsNone(self.view(category='dropped',search=text)['totals']['value'])
        r=self.view(category='dropped',type='addon:fruit/red')
        self.assertEqual(r['types'][0]['label'],'addon:fruit/red')

    def test_more_than200_types_and_independent_players_pages(self):
        self.player()
        with fixture_writer(self.path) as db:
            db.executemany('''INSERT INTO events(occurred_at_ms,event_kind,actor_kind,actor_id,cause,amount,primary_type)
                VALUES(?,'item_drop_result','player','private-uuid-a','ll26.60.player_drop_returned_true.v1',1,?)''',
                [(BASE+1000,f'addon:type_{i:03d}') for i in range(253)])
            db.executemany('INSERT INTO players VALUES(?,?,?,?)',[(f'p{i}',f'Z{i:03d}',BASE,BASE) for i in range(101)])
        r=self.view(category='dropped',page=12,limit=20,players_page=1)
        self.assertEqual(r['type_count'],253);self.assertEqual(len(r['types']),13)
        self.assertEqual(r['types'][0]['type'],'addon:type_240');self.assertFalse(r['has_more'])
        self.assertEqual(r['totals']['value'],253);self.assertEqual(len(r['players']),2)
        self.assertFalse(r['players_has_more'])

    def test_custom_mob_player_direct_projectile_and_killed_by_are_separate(self):
        self.seed()
        direct=KILLER | {'hook':'Mob::$die','resolved_player':True,'source_ids_equal':True}
        projectile=CHILD | {'hook':'Mob::$die','resolved_player':True,'source_ids_equal':False}
        self.row('direct_kill_observation','ll26.60.direct_attack_dead_transition.v1','minecraft:player',secondary='player',details=direct)
        self.row('direct_kill_observation','ll26.60.projectile_owner_dead_transition.v1','minecraft:skeleton',secondary='mob',details=projectile)
        self.row('direct_kill_observation','ll26.60.projectile_owner_dead_transition.v1','minecraft:player',secondary='player',details=projectile)
        self.row('player_killed_by_entity','ll26.60.resolved_killer_dead_transition.v1','minecraft:skeleton',secondary='child_projectile',details=CHILD)
        self.assertEqual(self.view(key='minecraft:mob_kills')['totals']['value'],2)
        self.assertEqual(self.view(key='minecraft:player_kills')['totals']['value'],2)
        self.assertEqual(self.view(category='killed')['totals']['value'],4)
        self.assertEqual(self.view(category='killed_by')['totals']['value'],2)
        self.assertIsNone(self.view(key='minecraft:deaths')['totals']['value'])

    def test_all_mapped_movement_modes_and_typed_period_event_guards(self):
        self.player()
        for mode in ('walk','sprint','crouch','swim','fly','aviate','fall','boat','minecart',
                     'pig','strider','horse','nautilus','happy_ghast'):
            d={'adapter':'ServerLevelTickEvent','phase':'polled_position_delta','mode':mode,'distance_cm':500,'flush_interval_ms':5000}
            self.row('player_travel_distance','ll26.60.poll_position_delta.v1',mode,500,details=d)
            with self.subTest(mode=mode):self.assertEqual(self.view(key='minecraft:'+mode+'_one_cm')['totals']['value'],500)
        for item,at,pid,actor,cause in (
            ('addon:before',BASE-1,'private-uuid-a','player','ll26.60.player_drop_returned_true.v1'),
            ('addon:at_end',BASE+86400000,'private-uuid-a','player','ll26.60.player_drop_returned_true.v1'),
            ('addon:future',NOW+1,'private-uuid-a','player','ll26.60.player_drop_returned_true.v1'),
            ('addon:fractional',BASE+1000.5,'private-uuid-a','player','ll26.60.player_drop_returned_true.v1'),
            ('addon:missing',BASE+1000,'missing','player','ll26.60.player_drop_returned_true.v1'),
            ('addon:mob',BASE+1000,'private-uuid-a','mob','ll26.60.player_drop_returned_true.v1'),
            ('addon:legacy',BASE+1000,'private-uuid-a','player','legacy'),
        ):self.row('item_drop_result',cause,item,pid=pid,at=at,actor=actor)
        self.row('item_drop_result','ll26.60.player_drop_returned_true.v1','minecraft:stone',at=BASE)
        self.assertEqual(self.view(category='dropped',period='day',date='2026-11-06')['totals']['value'],1)
        self.assertEqual(self.view(category='dropped',period='month',date='2026-11')['totals']['value'],2)

    def test_target_hit_projection_requires_resolved_owner_and_matching_projectile(self):
        self.player()
        good={'hook':'TargetBlock::$onProjectileHit','phase':'post_origin_hit',
              'owner_resolved_player':True,'projectile_type':'minecraft:arrow'}
        self.row('player_interaction_result','ll26.60.target_block_projectile_hit.v1',
                 'minecraft:target_hit',secondary='minecraft:arrow',details=good)
        for bad in (good | {'phase':'before_origin_hit'}, good | {'owner_resolved_player':False},
                    good | {'owner_resolved_player':1}, good | {'projectile_type':'minecraft:snowball'},
                    good | {'hook':'TargetBlock::other'}):
            self.row('player_interaction_result','ll26.60.target_block_projectile_hit.v1',
                     'minecraft:target_hit',secondary='minecraft:arrow',details=bad)
        self.row('player_interaction_result','ll26.60.target_block_projectile_hit.v1',
                 'minecraft:target_hit',2,secondary='minecraft:arrow',details=good)
        result=self.view(key='minecraft:target_hit')
        self.assertEqual(result['totals']['value'],1)
        self.assertEqual(result['status'],'recorded_observation')
        self.assertIn('飛び道具',result['definition'])
        self.assertTrue(result['projection_supported'])

    def test_sleep_sneak_jump_disconnect_limited_projections(self):
        self.player()
        sleep={'adapter':'ServerLevelTickEvent','phase':'sleep_flag_false_to_true','success_verified':True,'wake_time_measured':False}
        self.row('player_sleep_result','ll26.60.sleep_flag_transition.v1','minecraft:sleep_in_bed',details=sleep)
        self.row('player_sleep_result','ll26.60.sleep_flag_transition.v1','minecraft:sleep_in_bed',details=sleep | {'wake_time_measured':True})
        for phase in ('sneak_stop_to_start','disconnect_finalize','shutdown_finalize'):
            self.row('player_sneak_time','ll26.60.sneak_duration.v1','sneak',5,details={'phase':phase,'units':'ticks','wall_clock':True,'elapsed_us':250000})
        self.row('player_sneak_time','ll26.60.sneak_duration.v1','sneak',100,details={'phase':'before','units':'ticks','wall_clock':True})
        jump={'hook':'Mob::jumpFromGround','phase':'post_upward_impulse','measurement':'pos_delta_y_x1e6',
              'grounded_before':True,'vehicle_before':False,'vehicle_after':False,'before_y_micro':-80000,
              'after_y_micro':420000,'dimension':0,'world_jump_verified':False}
        self.row('player_jump_impulse','ll26.60.jump_pos_delta.v1','minecraft:player',details=jump)
        self.row('session_end','levilamina_player_event','')
        self.row('session_end','synthetic_shutdown','')
        self.row('session_end','levilamina_player_event','',2)
        for key,value in (('sleep_in_bed',1),('sneak_time',15),('jump',1),('leave_game',1)):
            self.assertEqual(self.view(key='minecraft:'+key)['totals']['value'],value,key)

    def test_new_detail_contract_bounds_do_not_accept_coerced_flags_or_elapsed(self):
        for cause,primary,amount,d in (
            ('ll26.60.craft_request_success.v1','minecraft:stick',4,CRAFT | {'action_type':2**31}),
            ('ll26.60.durability_item_destroyed.v1','minecraft:iron_pickaxe',1,BROKEN | {'delta_damage':2**31}),
            ('ll26.60.durability_item_destroyed.v1','minecraft:iron_pickaxe',1,BROKEN | {'is_damageable':1}),
            ('ll26.60.sneak_duration.v1','sneak',2,{'phase':'sneak_stop_to_start','units':'ticks','wall_clock':True,'elapsed_us':50000}),
            ('ll26.60.sneak_duration.v1','sneak',72001,{'phase':'shutdown_finalize','units':'ticks','wall_clock':True}),
            ('ll26.60.sleep_flag_transition.v1','minecraft:sleep_in_bed',1,{'adapter':'ServerLevelTickEvent','phase':'sleep_flag_false_to_true','success_verified':1,'wake_time_measured':False}),
        ):
            with self.subTest(cause=cause):self.assertEqual(extra_details(cause,primary,'',amount,json.dumps(d)),0)

    def test_time_union_crash_gaps_jst_and_death_asof_period_end(self):
        self.player()
        for sid,a,b in [('a',-1000,1000),('nested',0,500),('chain',500,2000),('gap',3000,5000)]:
            self.session(sid,BASE+a,BASE+b)
        self.session('open',BASE,None);self.session('invalid',BASE,BASE+10000,duration=2)
        self.session('future',BASE,9000000000000000)
        self.row('player_death','ll26.60.player_dead_transition.v1','minecraft:player',secondary='cause:5',details=DEATH,at=BASE+3500)
        self.row('player_death','ll26.60.player_dead_transition.v1','minecraft:player',secondary='cause:5',details=DEATH,at=BASE+4500.5)
        self.assertEqual(self.view()['totals']['value'],100)
        self.assertEqual(self.view(period='day',date='2026-11-06')['totals']['value'],80)
        self.assertEqual(self.view(period='day',date='2026-11-05')['totals']['value'],20)
        self.assertEqual(self.view(key='minecraft:time_since_death')['totals']['value'],30)
        self.assertEqual(self.view(key='minecraft:deaths')['totals']['value'],1)
        self.assertEqual(self.view(key='minecraft:time_since_death',period='day',date='2026-11-05')['totals']['value'],20)

    def test_movement_mode_and_special_sources_not_interaction_attempts(self):
        self.player()
        d={'adapter':'ServerLevelTickEvent','phase':'polled_position_delta','mode':'walk','distance_cm':500,'flush_interval_ms':5000}
        self.row('player_travel_distance','ll26.60.poll_position_delta.v1','walk',500,details=d)
        self.row('player_travel_distance','ll26.60.poll_position_delta.v1','walk',900,details=d)
        self.assertEqual(self.view(key='minecraft:walk_one_cm')['totals']['value'],500)
        self.assertIn('期間',self.view(key='minecraft:walk_one_cm')['definition'])
        trade={'hook':'ServerPlayer::$openTrading','phase':'post_origin_open','target_type':'minecraft:villager',
               'trade_screen_opened':True,'trade_completed_measured':False}
        breed={'hook':'BreedableComponent::mate','phase':'post_origin_offspring','offspring':1,'love_cause_resolved':True}
        self.row('player_interaction_result','ll26.60.open_trading_villager.v1','minecraft:talked_to_villager',secondary='minecraft:villager',details=trade)
        self.row('player_interaction_result','ll26.60.breed_offspring.v1','minecraft:animals_bred',secondary='minecraft:cow',details=breed)
        self.row('player_interaction_result','ll26.60.breed_offspring.v1','minecraft:animals_bred',secondary='minecraft:cow',details=breed | {'offspring':0})
        self.assertEqual(self.view(key='minecraft:talked_to_villager')['totals']['value'],1)
        self.assertEqual(self.view(key='minecraft:animals_bred')['totals']['value'],1)
        self.assertIsNone(self.view(key='minecraft:traded_with_villager')['totals']['value'])

    def test_history_readonly_old_schemas_and_rejections(self):
        self.seed()
        for version in (1,2,3,4):
            with fixture_writer(self.path) as db:db.execute(f'PRAGMA user_version={version}')
            before=hashlib.sha256(self.path.read_bytes()).digest()
            self.assertEqual(self.view(category='mined')['totals']['value'],1)
            self.assertEqual(hashlib.sha256(self.path.read_bytes()).digest(),before)
        for args in ({'category':'x'},{'key':'x'},{'category':'mined','key':'minecraft:drop'},
                     {'page':-1},{'page':1001},{'rank_page':True},{'limit':0},{'limit':True},
                     {'search':'x'*65},{'type':"x' OR 1=1"},{'player':'private-uuid-a'},
                     {'period':'day','date':'2026-11-07'}):
            with self.subTest(args=args),self.assertRaises(ValueError):self.view(**args)
        with self.assertRaises(LookupError):self.view(player='0'*32)


class StatsHTTPTests(Fixture):
    row=StatsViewTests.row
    seed=StatsViewTests.seed

    def setUp(self):
        super().setUp();self.seed()
        self.server=CompanionServer(self.path,0)
        self.thread=threading.Thread(target=self.server.serve_forever,daemon=True);self.thread.start()

    def tearDown(self):
        self.server.shutdown();self.server.server_close();self.thread.join(3);super().tearDown()

    def get(self,path,auth=True,headers=None,method='GET'):
        client=http.client.HTTPConnection('127.0.0.1',self.server.server_port,timeout=5)
        try:
            h={'Authorization':'Bearer '+self.server.viewer_token} if auth else {}
            h.update(headers or {});client.request(method,path,headers=h)
            response=client.getresponse();return response.status,dict(response.getheaders()),response.read()
        finally:client.close()

    def test_auth_origin_loopback_and_readonly_routes(self):
        self.assertEqual(self.server.server_address[0],'127.0.0.1')
        self.assertEqual(self.get('/api/v1/stats',False)[0],401)
        self.assertEqual(self.get('/api/v1/stats?token=not-a-token',False)[0],401)
        self.assertEqual(self.get('/api/v1/stats',headers={'Host':'evil.invalid'})[0],403)
        self.assertEqual(self.get('/api/v1/stats',headers={'Origin':'http://evil.invalid'})[0],403)
        self.assertEqual(self.get('/api/v1/stats',headers={'Sec-Fetch-Site':'cross-site'})[0],403)
        self.assertEqual(self.get('/api/v1/stats',method='POST')[0],405)
        for path in ('/stats','/stats.js','/stats.css'):
            status,headers,_=self.get(path,False);self.assertEqual(status,200)
            self.assertEqual(headers['Cache-Control'],'no-store')
            self.assertIn("frame-ancestors 'none'",headers['Content-Security-Policy'])

    def test_bad_queries_expiry_and_actual_japanese_dom(self):
        for query in ('category=unknown','key=unknown','category=mined&category=used','page=-1','limit=101',
                      'rank_page=abc','secret=x','type=bad','search='+'x'*65):
            self.assertEqual(self.get('/api/v1/stats?'+query)[0],400,query)
        self.assertEqual(self.get('/api/v1/stats?player='+'0'*32)[0],404)
        with fixture_writer(self.path) as db:db.execute('UPDATE players SET last_name=? WHERE player_id=?',('<svg onload=alert(1)>','private-uuid-a'))
        status,_,raw=self.get('/api/v1/stats?category=dropped')
        self.assertEqual(status,200);self.assertNotIn(b'private-uuid',raw)
        result=subprocess.run(['node','tests/stats_library_ui_tests.cjs'],input=raw.decode(),text=True,
                              capture_output=True,timeout=15,cwd=Path(__file__).resolve().parents[1])
        self.assertEqual(result.returncode,0,result.stderr)
        self.server.token_deadline=0
        self.assertEqual(self.get('/api/v1/stats')[0],401)
