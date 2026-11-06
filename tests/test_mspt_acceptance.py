"""Evidence-checker fixtures only. These are NOT native runtime acceptance."""
import copy
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import sqlite3
import subprocess
import sys
import unittest
import uuid

from companion.mspt_acceptance import check_evidence
from companion.telemetry import BUSY_START, BUSY_CAUSE, BUSY_END, BUSY_INVALID, parse_busy_observation

PROJECT=Path(__file__).resolve().parents[1]
START=1793919960000


class MsptAcceptanceTests(unittest.TestCase):
    def setUp(self):
        stamp=lambda ms: datetime.fromtimestamp(ms/1000,timezone.utc).isoformat()
        self.runtime={k:True for k in ('native_loaded','native_enabled','server_started','stop_command_issued',
            'bds_load_verified','loader_config_migrated_with_disabled_overrides')}
        self.runtime.update(exit_code=0,forced_termination=False,error=None,
            startup_outcome={'stop_reason':'ready_hold_completed'},capture_access={'mode':'empty-smoke'},
            preflight={'candidate_policy_passed':True},started_utc=stamp(START),finished_utc=stamp(START+20000))
        raw=json.dumps(dict(clock='steady',hook='Level::$tick/Highest+Lowest',wall_ns=5000000000,
            busy_ns=200000000,max_inner_ns=2000000,frames=100,sampled_peak_online=0))
        self.rows=[(1,START+1000,BUSY_START,'lltick-acceptance-fixture',0,'',-1),
            (3,START+6000,BUSY_CAUSE,'lltick-acceptance-fixture',100,raw,0),
            (5,START+11000,BUSY_CAUSE,'lltick-acceptance-fixture',100,raw,0),
            (7,START+15000,BUSY_END,'lltick-acceptance-fixture',0,'',-1)]
        self.source=PROJECT/'.build/isolated-bds'/('run-'+'e'*32)/'mods/BedrockInsight/data/bedrock_insight.sqlite3'
        self.live=dict(source=str(self.source),generated_at_ms=START+11050,anonymous=401,
            authenticated=200,listener_stopped_only_own=True,
            server=parse_busy_observation(self.rows[2][1:],START+11050))
        self.live['ui_dom']=dict(verification='actual Japanese assets DOM, not browser',
            mspt='2',tps='20',online='0',sampling=self.live['server']['reason'])

    def check(self):
        return check_evidence(self.runtime,self.live,self.rows,self.source)

    def test_two_windows_api_dom_normal_stop(self):
        report=self.check()
        self.assertEqual((report['busy_windows'],report['actual_ticks'],report['processing_mspt']),(2,200,2))
        self.assertFalse(report['browser_verified'])

    def test_crash_self_exit_forced_stop_false_flags_rejected(self):
        for key,value in [('native_enabled',False),('native_enabled','true'),('exit_code',-1066598274),
            ('exit_code',3),('exit_code',False),('forced_termination',True),('stop_command_issued',False),
            ('bds_load_verified',False),('error','failure')]:
            with self.subTest(key=key,value=value):
                bad=copy.deepcopy(self.runtime);bad[key]=value
                with self.assertRaises(ValueError): check_evidence(bad,self.live,self.rows,self.source)

    def test_incomplete_mixed_invalid_restarted_or_malformed_stream_rejected(self):
        variants=[self.rows[:-1],self.rows[1:],self.rows[:1]+self.rows[2:],list(reversed(self.rows))]
        for row_index,column,value in [(2,2,BUSY_INVALID),(2,2,BUSY_START),(2,3,'lltick-other'),
            (2,4,0),(2,5,'{}'),(2,1,START-1),(2,0,1),(2,2,'ll26.60.busytick.v2')]:
            rows=copy.deepcopy(self.rows);row=list(rows[row_index]);row[column]=value;rows[row_index]=tuple(row);variants.append(rows)
        for rows in variants:
            with self.subTest(rows=rows):
                with self.assertRaises(ValueError): check_evidence(self.runtime,self.live,rows,self.source)

    def test_mismatched_missing_stale_http_ui_or_source_rejected(self):
        variants=[]
        for key,value in [('anonymous',200),('authenticated',True),('listener_stopped_only_own',False),
            ('source',str(PROJECT/'different.sqlite3')),('generated_at_ms',START+30000),('ui_dom',None)]:
            live=copy.deepcopy(self.live);live[key]=value;variants.append(live)
        for container,key,value in [('server','processing_mspt',50),('server','last_sample_ms',START+1),
            ('server','sampling_status','stopped'),('server','current_online',False),
            ('ui_dom','mspt','未計測'),('ui_dom','tps','NaN'),('ui_dom','online','未計測')]:
            live=copy.deepcopy(self.live);live[container][key]=value;variants.append(live)
        for live in variants:
            with self.subTest(live=live):
                with self.assertRaises(ValueError): check_evidence(self.runtime,live,self.rows,self.source)

    def test_unknown_online_does_not_block_real_busy_mspt(self):
        for index in (1,2):
            row=list(self.rows[index]);raw=json.loads(row[5]);raw['sampled_peak_online']=-1
            row[5]=json.dumps(raw);row[6]=-1;self.rows[index]=tuple(row)
        self.live['server']=parse_busy_observation(self.rows[2][1:],START+11050)
        self.live['ui_dom']['online']='未計測'
        self.assertEqual(self.check()['processing_mspt'],2)

    def test_cli_closed_private_copy_is_read_only_and_optimization_keeps_gates(self):
        root=PROJECT/'.build/mspt-evidence/fixtures'/('run-'+uuid.uuid4().hex);root.mkdir(parents=True)
        (root/'FIXTURE_ONLY_NOT_NATIVE_EVIDENCE.txt').write_text('Synthetic checker fixture, NOT BDS/real MSPT acceptance.\n',encoding='utf-8')
        database=root/'fixture-closed.sqlite3'
        db=sqlite3.connect(database)
        try:
            db.executescript('PRAGMA user_version=4; CREATE TABLE events(event_id INTEGER PRIMARY KEY,occurred_at_ms INTEGER,cause TEXT,session_id TEXT,amount INTEGER,details_json TEXT,online_players INTEGER,event_kind TEXT,actor_kind TEXT);')
            db.executemany("INSERT INTO events VALUES(?,?,?,?,?,?,?,'server_tick','system')",self.rows);db.commit()
        finally: db.close()
        for name,data in [('runtime.json',self.runtime),('live.json',self.live)]:
            (root/name).write_text(json.dumps(data),encoding='utf-8')
        original=hashlib.sha256(database.read_bytes()).hexdigest()
        command=[sys.executable,'-O','-m','companion.mspt_acceptance','--snapshot',str(database),
            '--runtime',str(root/'runtime.json'),'--live-report',str(root/'live.json'),'--source',str(self.source)]
        completed=subprocess.run(command,cwd=PROJECT,capture_output=True,encoding='utf-8',timeout=5)
        self.assertEqual(completed.returncode,0,completed.stderr)
        self.assertEqual(json.loads(completed.stdout)['result']['busy_windows'],2)
        self.runtime['exit_code']=3;(root/'runtime.json').write_text(json.dumps(self.runtime),encoding='utf-8')
        rejected=subprocess.run(command,cwd=PROJECT,capture_output=True,encoding='utf-8',timeout=5)
        self.assertNotEqual(rejected.returncode,0)
        self.assertEqual(hashlib.sha256(database.read_bytes()).hexdigest(),original)
        self.assertFalse(Path(str(database)+'-wal').exists());self.assertFalse(Path(str(database)+'-shm').exists())
