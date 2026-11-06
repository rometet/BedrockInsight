"""Main-inventory type-total gains during a successful Player::take call."""
import json

PICKUP_SOURCE='ll26.60.take_inventory_delta.v1'


def valid_pickup_details(raw,amount):
    if not isinstance(raw,str) or len(raw)>512:
        return 0
    try:
        d=json.loads(raw)
    except (ValueError,RecursionError):
        return 0
    if not isinstance(d,dict):
        return 0
    if not all(type(x) is int for x in (amount,d.get('entry_units'),d.get('slots'),
                                      d.get('before_units'),d.get('after_units'))):
        return 0
    entry,slots,before,after=d['entry_units'],d['slots'],d['before_units'],d['after_units']
    return int(d.get('hook')=='Player::take' and d.get('phase')=='return_true_inventory_delta' and
               d.get('scope')=='main_inventory_type_total' and d.get('nested') is False and
               d.get('durable_transfer_verified') is False and 1<=entry<=255 and 1<=slots<=128 and
               0<=before<after<=slots*255 and 1<=amount==after-before<=entry)
