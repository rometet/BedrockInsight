"""HEALTH attribute decrease, distinct from pre-origin notifications/absorption."""
from . import absorption_source

SOURCE='ll26.60.hurt_health_delta.v1'
KEYS=('minecraft:damage_taken','minecraft:damage_dealt')


def valid(primary,secondary,amount,d):
    if (primary not in KEYS or d.get('attribute')!='HEALTH' or
        d.get('health_attribute_decrease_observed') is not True):return 0
    # Reuse our own strict numeric/lifetime/attribution validator without
    # mutating caller data or accepting the ABSORPTION provenance as HEALTH.
    equivalent=dict(d,attribute='ABSORPTION')
    return absorption_source.valid(absorption_source.KEYS[KEYS.index(primary)],secondary,amount,equivalent)
