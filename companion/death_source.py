"""Exact v1 dead-flag observation, not a pre-notification or killer attribution."""
import json
import re

DEATH_SOURCE = 'll26.60.player_dead_transition.v1'
# Pinned SharedTypes::Legacy::ActorDamageCause numeric codes, independent labels.
DAMAGE_CAUSES = dict(enumerate([
    '上書き指定', '接触', '直接攻撃', '飛び道具', '窒息', '落下', '火', '継続火炎',
    '溶岩', '溺水', 'ブロック爆発', 'エンティティ爆発', '奈落', '自壊', '魔法',
    'ウィザー', '空腹', '金床', '反射ダメージ', '落下ブロック', 'ピストン',
    '壁への衝突', 'マグマ', '花火', '落雷', '突進', '温度', '凍結', '鍾乳石',
    '石筍', '体当たり', '音波', '焚き火', '魂の焚き火', 'メイス打撃'
]))
DAMAGE_CAUSES[-1] = '不明／未指定'


def cause_number(value):
    if not isinstance(value, str) or not re.fullmatch(r'cause:(-1|0|[1-9][0-9]?)', value):
        return None
    number = int(value[6:])
    return number if number in DAMAGE_CAUSES else None


def valid_death_details(value):
    if not isinstance(value, str) or len(value) > 512:
        return 0
    try:
        data = json.loads(value)
        return int(isinstance(data, dict) and data.get('hook') == 'Player::$die' and
                   data.get('phase') == 'post_dead_transition' and
                   data.get('before_dead') is False and data.get('after_dead') is True and
                   data.get('killer_attribution') == 'unmeasured')
    except (ValueError, RecursionError):
        return 0
