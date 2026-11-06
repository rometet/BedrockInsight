"""Coalesced, delayed server-flag observations. Not inputs, durations or distance."""
import json

MODE_SOURCE='ll26.60.mode_flag_observed.v1'


def parsed_details(raw):
    if not isinstance(raw,str) or len(raw)>512:
        return None
    try:
        value=json.loads(raw)
        return value if isinstance(value,dict) else None
    except (ValueError,RecursionError):
        return None


def valid_mode_details(raw,mode,edge,stamp,amount):
    d=parsed_details(raw)
    if d is None or mode not in ('sprint','sneak') or edge not in ('start','stop'):
        return 0
    values=(stamp,amount,d.get('first_ms'),d.get('last_ms'),d.get('jst_day'),d.get('poll_us'),d.get('flush_us'))
    if not all(type(x) is int for x in values):
        return 0
    first,last=d['first_ms'],d['last_ms']
    return int(d.get('phase')=='delayed_flag_observation' and d.get('coalesced') is True and
               d.get('duration_measured') is False and d['poll_us']==250000 and d['flush_us']==5000000 and
               1<=amount<=1000000 and 0<=first<=last==stamp and
               (first+32400000)//86400000==(last+32400000)//86400000==d['jst_day'])


def first_mode_ms(raw):
    d=parsed_details(raw)
    return d['first_ms'] if d and type(d.get('first_ms')) is int else None
