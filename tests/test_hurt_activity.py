"""Deterministic SQLite/API/actual-assets DOM fixtures, NOT real item-break proof."""
import hashlib
import json
from datetime import datetime,timezone

from companion.hurt_source import HURT_SOURCE,valid_hurt_details
from companion.action_acceptance import METRICS,action_totals,verify_http_dom
from test_action_acceptance import ACTOR,markers,row
from test_analytics_companion import Fixture,fixture_writer,BASE

DETAILS={'hook':'ItemStackBase::hurtAndBreak','phase':'return_true_slot_empty',
    'scope':'main_inventory_same_slot','delta_damage':1,'slots':36,'slot':0,
    'entry_units':1,'after_units':0,'nested':False,'break_completed_verified':False}


class HurtActivityTests(Fixture):
    def hurt(self,pid='private-uuid-a',item='minecraft:wooden_pickaxe',at=BASE+1000,
             cause=HURT_SOURCE,event='item_hurt_slot_loss',amount=1,secondary='',patch=None):
        d=DETAILS|(patch or {})
        with fixture_writer(self.path) as db:
            db.execute('''INSERT INTO events(occurred_at_ms,event_kind,actor_kind,actor_id,cause,amount,
                primary_type,secondary_type,details_json) VALUES(?,?,'player',?,?,?,?,?,?)''',
                (at,event,pid,cause,amount,item,secondary,json.dumps(d)))

    def test_sql_selected_types_rank_ties_and_no_false_legacy_or_zero(self):
        self.player();self.player('z','Alice');self.player('unmeasured','New')
        self.hurt();self.hurt(item='minecraft:wooden_axe');self.hurt(pid='z');self.hurt(pid='z')
        r=self.report(metric='hurt_slot_losses',limit=1)
        self.assertEqual(r['totals']['hurt_slot_losses'],4)
        self.assertEqual(r['ranking'][0]['key'],self.analytics.player_key('private-uuid-a'))
        self.assertEqual(self.report(metric='hurt_slot_losses',limit=1,page=1)['ranking'][0]['key'],self.analytics.player_key('z'))
        r=self.report(player=self.analytics.player_key('private-uuid-a'))
        self.assertEqual(r['selected_player']['hurt_slot_losses'],2)
        self.assertEqual(r['item_hurt_slots']['types'],[
            {'type':'minecraft:wooden_axe','count':1},{'type':'minecraft:wooden_pickaxe','count':1}])
        self.assertIsNone(r['selected_player']['use_return_actions'])
        self.assertIsNone(self.report(player=self.analytics.player_key('unmeasured'))['selected_player']['hurt_slot_losses'])
        self.assertNotIn('private-uuid',json.dumps(r))
        self.assertIn('破損成立・因果関係・永続保存の証明ではありません',r['item_hurt_slots']['definition'])

    def test_fail_closed_source_and_detail_fields(self):
        self.player()
        for kw in ({'cause':'legacy'},{'cause':HURT_SOURCE+'.future'},{'event':'item_use_result'},
                   {'amount':0},{'amount':2},{'amount':1.5},{'secondary':'off_hand'},
                   {'pid':'unmapped'},{'item':'minecraft:<svg>'}):self.hurt(**kw)
        patches=[{'hook':'ItemStack::hurtAndBreak'},{'phase':'pre'},{'scope':'equipment'},
            {'delta_damage':0},{'delta_damage':-1},{'delta_damage':2147483648},
            {'slots':0},{'slots':129},{'slot':-1},{'slot':36},{'entry_units':0},{'entry_units':2},
            {'after_units':1},{'after_units':-1},{'nested':True},{'nested':0},{'break_completed_verified':True}]
        for key in ('delta_damage','slots','slot','entry_units','after_units'):
            patches.extend({key:x} for x in (True,'1',1.0,None))
        for patch in patches:
            self.assertEqual(valid_hurt_details(json.dumps(DETAILS|patch)),0)
            self.hurt(patch=patch)
        for raw in ('','null','[]','{bad','['*600):self.assertEqual(valid_hurt_details(raw),0)
        r=self.report(metric='hurt_slot_losses');self.assertIsNone(r['totals']['hurt_slot_losses'])
        self.assertEqual(r['ranking'],[]);self.assertEqual(r['item_hurt_slots']['types'],[])

    def test_jst_day_month_boundaries_future_and_schema_readonly(self):
        self.player();boundary=int(datetime(2026,10,31,15,tzinfo=timezone.utc).timestamp()*1000)
        self.hurt(at=boundary-1);self.hurt(at=boundary);self.hurt(at=BASE+999999999)
        for period,date in (('day','2026-10-31'),('day','2026-11-01'),('month','2026-10'),('month','2026-11')):
            self.assertEqual(self.report(period=period,date=date)['totals']['hurt_slot_losses'],1)
        for version in (1,2,3,4):
            with fixture_writer(self.path) as db:db.execute(f'PRAGMA user_version={version}')
            before=hashlib.sha256(self.path.read_bytes()).digest()
            self.assertEqual(self.report(period='all')['totals']['hurt_slot_losses'],2)
            self.assertEqual(hashlib.sha256(self.path.read_bytes()).digest(),before)

    def test_types_limit_and_old_database_empty(self):
        self.assertIsNone(self.report()['totals']['hurt_slot_losses']);self.player()
        for i in range(105):self.hurt(item=f'addon:tool_{i:03d}')
        r=self.report();self.assertEqual(len(r['item_hurt_slots']['types']),100)
        self.assertEqual(r['item_hurt_slots']['types'][0]['type'],'addon:tool_000')
        self.assertEqual(r['item_hurt_slots']['observed'],{'first_ms':BASE+1000,'last_ms':BASE+1000})

    def test_acceptance_arithmetic_and_true_response_not_full_break(self):
        valid=row(2,HURT_SOURCE,amount=1,details_json=json.dumps(DETAILS))
        totals,sources=action_totals(markers([valid]),ACTOR,BASE,BASE+10000)
        self.assertEqual(totals['hurt_slot_losses'],1);self.assertEqual(sources,{HURT_SOURCE})
        for patch in ({'nested':True},{'after_units':1},{'break_completed_verified':True}):
            bad=valid|{'details_json':json.dumps(DETAILS|patch)}
            with self.assertRaises(ValueError):action_totals(markers([bad]),ACTOR,BASE,BASE+10000)

    def test_authenticated_http_and_actual_japanese_actions_assets(self):
        self.player(name='<svg onload=alert(1)>');self.hurt()
        totals={key:0 for key in METRICS};totals['hurt_slot_losses']=1
        # Own ephemeral listener/token; actual assets render quantities/types/nulls.
        result=verify_http_dom(self.path,'private-uuid-a',{'hurt_slot_losses':1},totals)
        self.assertIsInstance(result,dict)
