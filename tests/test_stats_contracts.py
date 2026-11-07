"""Read-path contracts only. Never imply player/runtime outcome acceptance."""
import json

from companion.stats_view import (extra_details, report, catalog, USE_METHODS,
    USE_CONSUMED_CAUSE, CREATED_OUTPUT_CAUSE, HURT_USED_CAUSE)
from test_analytics_companion import Fixture, fixture_writer, BASE, NOW


class StatsContractTests(Fixture):
    def put(self, kind, cause, primary, secondary, amount, details):
        with fixture_writer(self.path) as db:
            db.execute('''INSERT INTO events(occurred_at_ms,event_kind,actor_kind,actor_id,
                cause,primary_type,secondary_type,amount,details_json) VALUES(?,?,'player',?,?,?,?,?,?)''',
                (BASE+1000,kind,'private-uuid-a',cause,primary,secondary,amount,json.dumps(details)))

    def test_used_id_name_pair_and_new_sources_fail_closed_without_details(self):
        self.player()
        for method,number in USE_METHODS.items():
            primary='minecraft:bone_meal' if method=='interact' else 'minecraft:apple'
            details={'hook':'Player::$useItem','phase':'post_origin_consumed','item_type':primary,
                     'use_method':number,'use_method_name':method,'consume_item':True,'nested':False}
            self.assertEqual(extra_details(USE_CONSUMED_CAUSE,primary,method,1,json.dumps(details)),1)
            for bad in (details | {'use_method':11 if number!=11 else 1},
                        details | {'use_method':True},details | {'consume_item':1}):
                self.assertEqual(extra_details(USE_CONSUMED_CAUSE,primary,method,1,json.dumps(bad)),0)
            self.put('item_use_result',USE_CONSUMED_CAUSE,primary,method,1,details)
        self.put('item_use_result',USE_CONSUMED_CAUSE,'minecraft:apple','eat',1,{})
        self.put('item_use_result',HURT_USED_CAUSE,'minecraft:iron_pickaxe','durability',1,{})
        result=report(self.analytics,category='used',now_ms=NOW)
        self.assertEqual(result['totals']['value'],7)
        self.assertIn('実消費',result['definition'])

    def test_created_branch_screen_and_zero_result_need_exact_typed_match(self):
        self.player()
        for branch,screen in (('inventory',-1),('workbench',1),('trade',15)):
            details={'hook':'ItemStackRequestActionHandler::_handleTransfer',
                     'phase':'post_origin_transfer_success','item_type':'minecraft:stick',
                     'transfer_amount':4,'branch':branch,'screen_container_type':screen,
                     'net_result':0,'nested':False,'inventory_commit_verified':True}
            self.put('item_craft_result',CREATED_OUTPUT_CAUSE,'minecraft:stick',branch,4,details)
            for bad in (details | {'screen_container_type':3},details | {'screen_container_type':True},
                        details | {'branch':'enchantment'},details | {'net_result':False}):
                self.assertEqual(extra_details(CREATED_OUTPUT_CAUSE,'minecraft:stick',branch,4,json.dumps(bad)),0)
                self.put('item_craft_result',CREATED_OUTPUT_CAUSE,'minecraft:stick',branch,4,bad)
        result=report(self.analytics,category='crafted',now_ms=NOW)
        self.assertEqual(result['totals']['value'],12)
        self.assertIn('処理前モデル差分',result['definition'])

    def test_legacy_damage_notifications_preserved_but_not_health_custom(self):
        self.player()
        taken={'adapter':'ActorHurtEvent','phase':'pre_origin_amount','cause_code':2,
               'damaging_entity_type':3,'units':'tenths','entity_source':True,
               'absorbed_or_resisted_measured':False}
        dealt={'adapter':'ActorHurtEvent','phase':'pre_origin_amount','cause_code':2,'units':'tenths',
               'attribution':'damaging_entity_unique_id','attack_action_substituted':False,
               'absorbed_or_resisted_measured':False}
        self.put('player_damage_taken','ll26.60.actor_hurt_event.v1','cause:2','entity',30,taken)
        self.put('player_damage_dealt','ll26.60.actor_hurt_real_damage.v1','minecraft:player','cause:2',40,dealt)
        self.put('player_damage_taken','ll26.60.actor_hurt_event.v1','cause:4','entity',90,taken)
        self.put('player_damage_taken','ll26.60.actor_hurt_event.v1','cause:2','environment',90,taken)
        self.put('player_damage_dealt','ll26.60.actor_hurt_real_damage.v1','minecraft:player','cause:4',90,dealt)
        for key in ('damage_taken','damage_dealt'):
            result=report(self.analytics,key='minecraft:'+key,now_ms=NOW)
            self.assertIsNone(result['totals']['value'])
            self.assertIn('HP属性減少観測',result['label']);self.assertIn('HEALTH',result['definition'])
            self.assertEqual(result['observation_stage'],'health_attribute_delta')
            self.assertFalse(result['final_world_outcome_verified'])
            self.assertFalse(result['collection_complete'])
        entries={x['id']:x for x in catalog()['custom']}
        self.assertEqual(entries['minecraft:damage_taken']['observation_stage'],'health_attribute_delta')
        old_actions=self.analytics.report(now_ms=NOW)['damage']
        self.assertEqual(old_actions['observation_stage'],'pre_origin_notification')
        self.assertFalse(old_actions['final_world_outcome_verified'])
        self.assertIn('処理前申告量',old_actions['definition'])
