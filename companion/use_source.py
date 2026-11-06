"""True useItem return plus bounded inventory snapshots; NOT completed use/consumption."""
import json

USE_SOURCE='ll26.60.gamemode_use_return.v1'


def use_loss(raw):
    """None=invalid; zero=valid snapshots without a positive net decrease."""
    if not isinstance(raw,str) or len(raw)>512:
        return None
    try:
        d=json.loads(raw)
    except (ValueError,RecursionError):
        return None
    if not isinstance(d,dict):
        return None
    if not all(type(d.get(k)) is int for k in ('entry_units','slots','before_units','after_units','loss_units')):
        return None
    entry,slots,before,after,loss=(d[k] for k in ('entry_units','slots','before_units','after_units','loss_units'))
    if (d.get('hook')!='GameMode::$useItem' or d.get('phase')!='return_true_inventory_snapshot' or
        d.get('scope')!='main_inventory_type_total' or d.get('nested') is not False or
        d.get('effect_completed_verified') is not False or d.get('consumption_verified') is not False or
        not 1<=entry<=255 or not 1<=slots<=128 or not 0<=before<=slots*255 or
        not 0<=after<=slots*255 or not 0<=loss==max(0,before-after)<=entry):
        return None
    return loss
