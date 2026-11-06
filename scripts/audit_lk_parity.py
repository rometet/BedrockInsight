"""Fail-closed requirement inventory, not a native/runtime acceptance generator.

Reads repository documents/catalog only. Every row needs its OWN actual-play,
SQLite and GUI/API evidence; global compile/smoke cannot promote 77 keys.
"""
import argparse
from datetime import datetime,timezone
import hashlib
import json
from pathlib import Path
import re
import uuid

PROJECT=Path(__file__).resolve().parents[1]
CATEGORIES=('custom','mined','broken','crafted','used','picked_up','dropped','killed','killed_by')


def custom_keys(requirements,catalog):
    required=re.findall(r'^\d+\. `([^`]+)`',requirements,re.M)
    block=catalog.split('kCustomKeys{{',1)[1].split('}};',1)[0]
    actual=re.findall(r'"(minecraft:[a-z_]+)"',block)
    if len(required)!=77 or len(set(required))!=77 or actual!=required:
        raise ValueError('Requirements/catalog must contain exactly the same ordered77 keys')
    return required


def inventory(requirements,catalog,matrix):
    keys=custom_keys(requirements,catalog)
    declared={}
    for line in matrix.splitlines():
        cells=[x.strip() for x in line.strip('|').split('|')]
        if len(cells)==4 and re.fullmatch(r'minecraft:[a-z_]+',cells[1]):
            if cells[1] in declared:raise ValueError('Duplicate CUSTOM row in gap matrix')
            declared[cells[1]]={'acquisition_note':cells[2],'declared_status':cells[3]}
    if set(declared)!=set(keys):raise ValueError('Gap matrix must account for all77 keys exactly')
    rows=[]
    for group,ids in (('category',['minecraft:'+x for x in CATEGORIES]),('custom',keys),
                      ('command',['/stats gui','/stats rank','/stats player']),
                      ('api',['getPlayerStats','getRankStats','getPlayerStatsPage','getRankStatsPage']),
                      ('support',['persistent_history','japanese_labels','japanese_web'])):
        for key in ids:
            rows.append({'group':group,'key':key,'matrix_declaration':declared.get(key),
                'registration':'NOT_INDEPENDENTLY_VERIFIED','per_key_compile':'NO_INDIVIDUAL_PROOF',
                'per_key_fixture':'NO_INDIVIDUAL_PROOF','candidate_bds_load':'NOT_ATTESTED',
                'actual_player_action':'NOT_VERIFIED','sqlite_capture':'NOT_VERIFIED',
                'in_game_gui':'NOT_VERIFIED','api_or_web':'NOT_VERIFIED','accepted':False})
    return {'kind':'lk-parity-requirement-inventory-v1','upstream_pin':'5bb8bfb7e92c28e03bdd02d93ba7cc8b6c15bb40',
        'full_parity_accepted':False,'rows':rows,'limits':
        'Matrix declarations are not audited API availability or gameplay proof. Enum/labels, compile, empty BDS smoke and fixtures do not accept any key.'}


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.parse_args()
    sources=[PROJECT/'LK_STATS_FULL_PARITY_REQUIREMENTS.md',PROJECT/'src/bedrock_insight/core/stats_catalog.hpp',
             PROJECT/'docs/LK_FULL_PARITY_MATRIX.md']
    payloads=[p.read_bytes() for p in sources]
    report=inventory(*(p.decode('utf-8-sig') for p in payloads))
    report['created_utc']=datetime.now(timezone.utc).isoformat()
    report['input_sha256']={str(p.relative_to(PROJECT)):hashlib.sha256(b).hexdigest().upper()
                            for p,b in zip(sources,payloads)}
    root=PROJECT/'.build'/'parity-audit'/('run-'+uuid.uuid4().hex);root.mkdir(parents=True)
    output=root/'inventory.json';output.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print(f'Parity requirement inventory: {len(report["rows"])} rows, zero accepted; {output}')


if __name__=='__main__':main()
