"""Independent Japanese type labels; immutable, bounded, cosmetic only."""
import json
from pathlib import Path
import re
from types import MappingProxyType

DEFAULT_PATH=Path(__file__).parent/'lang'/'ja_JP.json'
MAX_BYTES=128*1024
MAX_ENTRIES=4096


def _object(pairs):
    value={}
    for key,item in pairs:
        if key in value:raise ValueError('duplicate type label field')
        value[key]=item
    return value


def load_type_labels(path,base=None):
    with Path(path).open('rb') as file:raw=file.read(MAX_BYTES+1)
    if len(raw)>MAX_BYTES:raise ValueError('type label file exceeds read budget')
    def invalid(_):raise ValueError('invalid JSON number')
    try:
        data=json.loads(raw.decode('utf-8'),object_pairs_hook=_object,parse_constant=invalid)
    except RecursionError as error:
        raise ValueError('type label JSON nesting exceeds parser budget') from error
    if (not isinstance(data,dict) or set(data)!={'schema_version','locale','labels'} or
        type(data['schema_version']) is not int or data['schema_version']!=1 or data['locale']!='ja_JP' or
        not isinstance(data['labels'],dict) or len(data['labels'])>MAX_ENTRIES):
        raise ValueError('unsupported type label schema/locale')
    labels=dict(base or {})
    for key,text in data['labels'].items():
        if (not 3<=len(key)<=128 or not re.fullmatch(r'[a-z0-9_.-]+:[a-z0-9_./-]+',key) or
            not isinstance(text,str) or not 1<=len(text.encode('utf-8'))<=256 or
            any(ord(c)<32 or 127<=ord(c)<=159 or ord(c) in (0x2028,0x2029) for c in text)):
            raise ValueError('invalid type identifier or UTF-8 single-line label')
        labels[key]=text
    if len(labels)>MAX_ENTRIES:raise ValueError('merged type labels exceed entry budget')
    return MappingProxyType(labels)
