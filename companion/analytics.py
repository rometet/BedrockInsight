"""Schema v1-v4 reader. No migrations, writes, checkpoint, or online inference."""
from __future__ import annotations

from contextlib import contextmanager, ExitStack
from datetime import datetime, timedelta, timezone
import hashlib
import hmac
from pathlib import Path
import re
import sqlite3
import time

from .telemetry import read_telemetry
from .death_source import DEATH_SOURCE, DAMAGE_CAUSES, cause_number, valid_death_details
from .killed_by_source import KILLED_BY_SOURCE, valid_killed_by_details
from .mode_source import MODE_SOURCE, valid_mode_details, first_mode_ms
from .pickup_source import PICKUP_SOURCE, valid_pickup_details
from .jump_source import JUMP_SOURCE, JUMP_EVENT_SOURCE, valid_jump_details
from .kill_source import KILL_SOURCE, valid_kill_details, PROJECTILE_KILL_SOURCE, valid_projectile_kill_details
from .use_source import USE_SOURCE, use_loss
from .hurt_source import HURT_SOURCE, valid_hurt_details
from .block_source import MINED_SOURCE, PLACED_SOURCE, MINED_V2_SOURCE, PLACED_V2_SOURCE, valid_block_details
from .broken_source import V1 as BROKEN_V1, V2 as BROKEN_V2, valid_broken_details
from .travel_source import valid_travel_v2

JST = timezone(timedelta(hours=9))
METRICS = {
    "playtime": ("確定プレイ時間", "ms"),
    "sessions": ("開始セッション数", "count"),
    "destroy_attempts": ("ブロック破壊の試行", "count"),
    "place_attempts": ("ブロック設置の試行", "count"),
    "joins": ("参加イベント", "count"),
    "leaves": ("退出イベント", "count"),
    "death_notifications": ("死亡処理の通知（処理前）", "count"),
    "item_use_attempts": ("アイテム使用の試行", "count"),
    "dropped_items": ("投棄 true 戻り値・入口スタック個数", "items"),
    "drop_actions": ("投棄 true 戻り値の回数", "count"),
    "confirmed_deaths": ("死亡フラグ成立（処理後）", "count"),
    "killed_by_entities": ("死亡元エンティティ解決済み観測", "count"),
    "sprint_starts": ("スプリント開始フラグ観測", "count"),
    "sprint_stops": ("スプリント終了フラグ観測", "count"),
    "sneak_starts": ("スニーク開始フラグ観測", "count"),
    "sneak_stops": ("スニーク終了フラグ観測", "count"),
    "pickup_inventory_items": ("拾得処理中・主所持品の増加個数", "items"),
    "pickup_delta_actions": ("拾得処理中・所持数増加の観測回数", "count"),
    "jump_impulses": ("ジャンプ通知（旧加速観測を含む）", "count"),
    "direct_kills": ("直接攻撃の死亡フラグ成立観測", "count"),
    "projectile_kills": ("子飛び道具・所有者解決済み死亡観測", "count"),
    "direct_mob_kills": ("直接攻撃・Mob死亡フラグ成立観測", "count"),
    "direct_player_kills": ("直接攻撃・プレイヤー死亡フラグ成立観測", "count"),
    "use_return_actions": ("使用の成功観測（消費系/耐久消費）", "count"),
    "use_inventory_loss": ("使用の種類別記録数", "count"),
    "hurt_slot_losses": ("耐久処理true応答・主所持品スロットの1個消失", "count"),
    "mined_blocks": ("ブロック採掘の成功（種類別）", "count"),
    "mined_block_actions": ("ブロック採掘成功の観測回数", "count"),
    "placed_blocks": ("ブロック設置の成功（種類別）", "count"),
    "placed_block_actions": ("ブロック設置成功の観測回数", "count"),
    "crafted_items": ("クラフト成立アイテム数（種類別）", "items"),
    "craft_actions": ("クラフト成立の回数", "count"),
    "travel_cm": ("移動距離（cm・モード別合計）", "cm"),
    "interactions": ("ブロック相互作用（CUSTOMキー別）", "count"),
    "damage_taken_tenths": ("受けたダメージの処理前申告量（1/10単位）", "tenths"),
    "damage_dealt_tenths": ("与えたダメージの処理前申告量（1/10単位）", "tenths"),
    "broken_items": ("耐久値による破損（種類別）", "count"),
    "sleep_successes": ("睡眠成立（フラグ遷移）", "count"),
    "sneak_time_ticks": ("スニーク継続（tick）", "ticks"),
}
OPTIONAL_ACTIVITY = ("death_notifications", "item_use_attempts", "dropped_items", "drop_actions", "confirmed_deaths", "killed_by_entities",
                     "sprint_starts", "sprint_stops", "sneak_starts", "sneak_stops",
                     "pickup_inventory_items", "pickup_delta_actions", "jump_impulses",
                     "direct_kills", "direct_mob_kills", "direct_player_kills", "projectile_kills", "use_return_actions", "use_inventory_loss", "hurt_slot_losses",
                     "mined_blocks", "mined_block_actions", "placed_blocks", "placed_block_actions",
                     "crafted_items", "craft_actions", "travel_cm", "interactions",
                     "damage_taken_tenths", "damage_dealt_tenths", "broken_items",
                     "sleep_successes", "sneak_time_ticks")
UNMEASURED = ["破壊成功数（GameMode経路の観測のみ。クリエイティブ等の別経路は未計測）", "設置成功数（GameMode経路の観測のみ。別経路は未計測）", "ブロック素材", "アイテム使用効果の完了・消費成功", "間接・飛び道具・環境のキル帰属／全キル数", "空中移動が完了したジャンプ数", "吸収・耐性・軽減後ダメージ", "飛び道具の所有権帰属", "村人取引・釣り・動物繁殖・エンチャント", "CUSTOMキーのうち対応フックのない項目"]


def valid_item_type(value):
    return int(isinstance(value, str) and len(value) <= 128 and
               re.fullmatch(r"[a-z0-9_.-]+:[a-z0-9_./-]+", value) is not None)


def period_bounds(period: str, date: str | None, now_ms: int):
    now = datetime.fromtimestamp(now_ms / 1000, JST)
    if period == "all":
        if date:
            raise ValueError("全期間では日付を指定できません")
        return 0, now_ms + 1, "全期間"
    if period == "day":
        key = date or now.strftime("%Y-%m-%d")
        if not re.fullmatch(r"[0-9]{4}-[0-9]{2}-[0-9]{2}", key):
            raise ValueError("日付は YYYY-MM-DD です")
        start = datetime.strptime(key, "%Y-%m-%d").replace(tzinfo=JST)
        end = start + timedelta(days=1)
    elif period == "month":
        key = date or now.strftime("%Y-%m")
        if not re.fullmatch(r"[0-9]{4}-[0-9]{2}", key):
            raise ValueError("月は YYYY-MM です")
        start = datetime.strptime(key, "%Y-%m").replace(tzinfo=JST)
        end = start.replace(year=start.year + 1, month=1) if start.month == 12 else start.replace(month=start.month + 1)
    else:
        raise ValueError("期間は day / month / all です")
    if start.year < 1970 or start.year > 9998:
        raise ValueError("日付が対応範囲外です")
    if int(start.timestamp() * 1000) > now_ms:
        raise ValueError("未来の期間は指定できません")
    return max(0, int(start.timestamp() * 1000)), int(end.timestamp() * 1000), key


def duration_text(ms: int):
    seconds = max(0, ms) // 1000
    return f"{seconds // 3600}時間 {(seconds // 60) % 60}分 {seconds % 60}秒"


@contextmanager
def readonly_source(path: Path, require_wal=False):
    """SQLite's read-only WAL reader, NOT immutable (which would miss live WAL).

    Existing WAL/SHM files are required if either exists. Never create missing
    companion files on behalf of a live writer. query_only is connection-local.
    """
    path = path.resolve(strict=True)
    wal, shm = Path(str(path) + "-wal"), Path(str(path) + "-shm")
    if wal.exists() != shm.exists() or (require_wal and not (wal.exists() and shm.exists())):
        raise sqlite3.OperationalError("WAL/SHM incomplete; refusing source changes")
    with ExitStack() as handles:
        # Keep existing WAL files open read-only during the backup. On Windows
        # these handles also prevent deletion/recreation while we read. No ACL,
        # journal mode, checkpoint, immutable-WAL shortcut or source write.
        if wal.exists():
            handles.enter_context(wal.open("rb"))
            handles.enter_context(shm.open("rb"))
        db = sqlite3.connect(path.as_uri() + "?mode=ro&cache=private", uri=True, timeout=0.15)
        try:
            db.execute("PRAGMA query_only=ON")
            yield db
        finally:
            db.close()


@contextmanager
def snapshot(path: Path):
    """One consistent in-memory backup per response, bounded busy wait/SQL work."""
    memory = sqlite3.connect(":memory:")
    deadline = time.monotonic() + 2
    try:
        live_root = Path(__file__).resolve().parents[1] / ".build" / "isolated-bds"
        with readonly_source(path, require_wal=path.resolve().is_relative_to(live_root)) as source:
            if source.execute("PRAGMA page_count").fetchone()[0] * source.execute("PRAGMA page_size").fetchone()[0] > 256 * 1024 * 1024:
                raise sqlite3.OperationalError("source exceeds 256 MiB reader budget")

            def progress(_status, _remaining, _total):
                if time.monotonic() > deadline:
                    raise TimeoutError("snapshot deadline")

            source.backup(memory, pages=128, progress=progress, sleep=0.01)
        memory.row_factory = sqlite3.Row
        if memory.execute("PRAGMA user_version").fetchone()[0] not in (1, 2, 3, 4):
            raise sqlite3.DatabaseError("unsupported analytics schema")
        memory.execute("PRAGMA query_only=ON")
        memory.create_function("bi_item_type", 1, valid_item_type, deterministic=True)
        memory.create_function("bi_death_details", 1, valid_death_details, deterministic=True)
        memory.create_function("bi_killed_by_details", 2, valid_killed_by_details, deterministic=True)
        memory.create_function("bi_pickup_details", 2, valid_pickup_details, deterministic=True)
        memory.create_function("bi_jump_details", 1, valid_jump_details, deterministic=True)
        memory.create_function("bi_kill_details", 1, valid_kill_details, deterministic=True)
        memory.create_function("bi_projectile_kill_details", 1, valid_projectile_kill_details, deterministic=True)
        memory.create_function("bi_use_loss", 1, use_loss, deterministic=True)
        memory.create_function("bi_hurt_details", 1, valid_hurt_details, deterministic=True)
        memory.create_function("bi_block_details", 5, valid_block_details, deterministic=True)
        memory.create_function("bi_broken_details", 4, valid_broken_details, deterministic=True)
        memory.create_function("bi_damage_cause", 1, lambda x: int(cause_number(x) is not None), deterministic=True)
        memory.create_function("bi_mode_details", 5, valid_mode_details, deterministic=True)
        memory.create_function("bi_mode_first_ms", 1, first_mode_ms, deterministic=True)
        memory.create_function("bi_travel_v2", 4, valid_travel_v2, deterministic=True)
        deadline = time.monotonic() + 2
        memory.set_progress_handler(lambda: int(time.monotonic() > deadline), 1000)
        yield memory
    finally:
        memory.close()


# Merge overlapping closed intervals per player before summing. A running MAX,
# not LAG(end), correctly handles nested/chained overlaps and duplicate joins.
# Half-open [start,end) clipping uses JST bounds converted to UTC epoch ms.
CTE = """
WITH valid AS (
 SELECT * FROM sessions WHERE ended_at_ms IS NOT NULL AND duration_ms IS NOT NULL
 AND started_at_ms>=0 AND ended_at_ms>=started_at_ms AND ended_at_ms<=:now
 AND duration_ms=ended_at_ms-started_at_ms
), clipped AS (
 SELECT player_id,session_id, MAX(started_at_ms,:start) AS b, MIN(ended_at_ms,:end) AS e
 FROM valid WHERE ended_at_ms>:start AND started_at_ms<:end AND ended_at_ms>started_at_ms
), ordered AS (
 SELECT *, MAX(e) OVER (PARTITION BY player_id ORDER BY b,e,session_id
 ROWS BETWEEN UNBOUNDED PRECEDING AND 1 PRECEDING) AS prior_e FROM clipped
), grouped AS (
 SELECT *, SUM(CASE WHEN prior_e IS NULL OR b>prior_e THEN 1 ELSE 0 END)
 OVER (PARTITION BY player_id ORDER BY b,e,session_id ROWS UNBOUNDED PRECEDING) AS g FROM ordered
), merged AS (
 SELECT player_id, MIN(b) AS b, MAX(e) AS e FROM grouped GROUP BY player_id,g
), times AS (
 SELECT player_id, SUM(e-b) AS playtime FROM merged GROUP BY player_id
), drops AS (
 SELECT * FROM events WHERE event_kind='item_drop_result'
 AND cause='ll26.60.player_drop_returned_true.v1' AND actor_kind='player' AND actor_id!=''
 AND typeof(amount)='integer' AND amount BETWEEN 1 AND 255 AND bi_item_type(primary_type)
 AND EXISTS(SELECT 1 FROM players WHERE player_id=events.actor_id)
 AND occurred_at_ms>=:start AND occurred_at_ms<:end AND occurred_at_ms<=:now
), drop_totals AS (
 SELECT actor_id,SUM(amount) AS dropped_items,COUNT(*) AS drop_actions FROM drops GROUP BY actor_id
), deaths AS (
 SELECT * FROM events WHERE event_kind='player_death'
 AND cause='ll26.60.player_dead_transition.v1' AND actor_kind='player' AND actor_id!=''
 AND typeof(amount)='integer' AND amount=1 AND primary_type='minecraft:player'
 AND bi_damage_cause(secondary_type) AND bi_death_details(details_json)
 AND EXISTS(SELECT 1 FROM players WHERE player_id=events.actor_id)
 AND occurred_at_ms>=:start AND occurred_at_ms<:end AND occurred_at_ms<=:now
), death_totals AS (
 SELECT actor_id,COUNT(*) AS confirmed_deaths FROM deaths GROUP BY actor_id
), killed_by AS (
 SELECT * FROM events WHERE event_kind='player_killed_by_entity'
 AND cause='ll26.60.resolved_killer_dead_transition.v1' AND actor_kind='player' AND actor_id!=''
 AND typeof(amount)='integer' AND amount=1 AND bi_item_type(primary_type)
 AND bi_killed_by_details(details_json,secondary_type)
 AND EXISTS(SELECT 1 FROM players WHERE player_id=events.actor_id)
 AND typeof(occurred_at_ms)='integer' AND occurred_at_ms>=0
 AND occurred_at_ms>=:start AND occurred_at_ms<:end AND occurred_at_ms<=:now
), killed_by_totals AS (
 SELECT actor_id,COUNT(*) AS killed_by_entities FROM killed_by GROUP BY actor_id
), pickups AS (
 SELECT * FROM events WHERE event_kind='item_pickup_delta'
 AND cause='ll26.60.take_inventory_delta.v1' AND actor_kind='player' AND actor_id!=''
 AND bi_item_type(primary_type) AND bi_pickup_details(details_json,amount)
 AND (secondary_type='' OR secondary_type IS NULL)
 AND EXISTS(SELECT 1 FROM players WHERE player_id=events.actor_id)
 AND occurred_at_ms>=:start AND occurred_at_ms<:end AND occurred_at_ms<=:now
), pickup_totals AS (
 SELECT actor_id,SUM(amount) AS pickup_inventory_items,COUNT(*) AS pickup_delta_actions FROM pickups GROUP BY actor_id
), jumps AS (
 SELECT * FROM events WHERE event_kind='player_jump_impulse'
 AND cause IN ('ll26.60.jump_pos_delta.v1','ll26.60.player_jump_event.v1') AND actor_kind='player' AND actor_id!=''
 AND typeof(amount)='integer' AND amount=1 AND primary_type='minecraft:player'
 AND (secondary_type='' OR secondary_type IS NULL) AND bi_jump_details(details_json)
 AND EXISTS(SELECT 1 FROM players WHERE player_id=events.actor_id)
 AND occurred_at_ms>=:start AND occurred_at_ms<:end AND occurred_at_ms<=:now
), jump_totals AS (
 SELECT actor_id,COUNT(*) AS jump_impulses FROM jumps GROUP BY actor_id
), observed_kills AS (
 SELECT * FROM events WHERE event_kind='direct_kill_observation'
 AND cause='ll26.60.direct_attack_dead_transition.v1' AND actor_kind='player' AND actor_id!=''
 AND typeof(amount)='integer' AND amount=1 AND bi_item_type(primary_type) AND bi_kill_details(details_json)
 AND ((secondary_type='player' AND primary_type='minecraft:player')
 OR (secondary_type='mob' AND primary_type!='minecraft:player'))
 AND EXISTS(SELECT 1 FROM players WHERE player_id=events.actor_id)
 AND occurred_at_ms>=:start AND occurred_at_ms<:end AND occurred_at_ms<=:now
), kill_totals AS (
 SELECT actor_id,COUNT(*) AS direct_kills,
 SUM(CASE WHEN secondary_type='mob' THEN 1 ELSE 0 END) AS direct_mob_kills,
 SUM(CASE WHEN secondary_type='player' THEN 1 ELSE 0 END) AS direct_player_kills
 FROM observed_kills GROUP BY actor_id
), projectile_kills AS (
 SELECT * FROM events WHERE event_kind='direct_kill_observation'
 AND cause='ll26.60.projectile_owner_dead_transition.v1' AND actor_kind='player' AND actor_id!=''
 AND typeof(amount)='integer' AND amount=1 AND bi_item_type(primary_type)
 AND bi_projectile_kill_details(details_json)
 AND ((secondary_type='player' AND primary_type='minecraft:player')
 OR (secondary_type='mob' AND primary_type!='minecraft:player'))
 AND EXISTS(SELECT 1 FROM players WHERE player_id=events.actor_id)
 AND typeof(occurred_at_ms)='integer' AND occurred_at_ms>=0
 AND occurred_at_ms>=:start AND occurred_at_ms<:end AND occurred_at_ms<=:now
), projectile_totals AS (
 SELECT actor_id,COUNT(*) AS projectile_kills FROM projectile_kills GROUP BY actor_id
), use_results AS (
 SELECT *,CASE WHEN cause='ll26.60.gamemode_use_return.v1'
  THEN COALESCE(bi_use_loss(details_json),0) ELSE 1 END AS loss FROM events
 WHERE event_kind='item_use_result'
 AND ((cause='ll26.60.gamemode_use_return.v1' AND secondary_type IN ('main_hand','off_hand')
       AND bi_use_loss(details_json) IS NOT NULL)
   OR (cause='ll26.60.item_hurt_used.v1' AND secondary_type='durability')
   OR (cause='ll26.60.player_use_item_consumed.v1' AND secondary_type IN
       ('eat','consume','throw','place','fill_bottle','pour_bucket','interact')))
 AND actor_kind='player' AND actor_id!=''
 AND typeof(amount)='integer' AND amount=1 AND bi_item_type(primary_type)
 AND EXISTS(SELECT 1 FROM players WHERE player_id=events.actor_id)
 AND occurred_at_ms>=:start AND occurred_at_ms<:end AND occurred_at_ms<=:now
), use_totals AS (
 SELECT actor_id,COUNT(*) AS use_return_actions,SUM(loss) AS use_inventory_loss FROM use_results GROUP BY actor_id
), hurt_slots AS (
 SELECT * FROM events WHERE event_kind='item_hurt_slot_loss'
 AND cause='ll26.60.hurt_slot_empty.v1' AND actor_kind='player' AND actor_id!=''
 AND typeof(amount)='integer' AND amount=1 AND bi_item_type(primary_type)
 AND (secondary_type='' OR secondary_type IS NULL) AND bi_hurt_details(details_json)
 AND EXISTS(SELECT 1 FROM players WHERE player_id=events.actor_id)
 AND occurred_at_ms>=:start AND occurred_at_ms<:end AND occurred_at_ms<=:now
), hurt_totals AS (
 SELECT actor_id,COUNT(*) AS hurt_slot_losses FROM hurt_slots GROUP BY actor_id
), mined AS (
 SELECT * FROM events WHERE event_kind='block_mined_result'
 AND cause IN ('ll26.60.gamemode_destroy_block.v1','ll26.60.gamemode_destroy_block.v2') AND actor_kind='player' AND actor_id!=''
 AND typeof(amount)='integer' AND amount=1 AND bi_item_type(primary_type)
 AND (secondary_type='' OR secondary_type IS NULL) AND bi_block_details(details_json,'mined',primary_type,cause,COALESCE(secondary_type,''))
 AND EXISTS(SELECT 1 FROM players WHERE player_id=events.actor_id)
 AND occurred_at_ms>=:start AND occurred_at_ms<:end AND occurred_at_ms<=:now
), mined_totals AS (
 SELECT actor_id,SUM(amount) AS mined_blocks,COUNT(*) AS mined_block_actions FROM mined GROUP BY actor_id
), placed AS (
 SELECT * FROM events WHERE event_kind='block_placed_result'
 AND cause IN ('ll26.60.gamemode_build_block.v1','ll26.60.gamemode_build_block.v2') AND actor_kind='player' AND actor_id!=''
 AND typeof(amount)='integer' AND amount=1 AND bi_item_type(primary_type)
 AND secondary_type IN ('main_hand','off_hand') AND bi_block_details(details_json,'placed',primary_type,cause,secondary_type)
 AND EXISTS(SELECT 1 FROM players WHERE player_id=events.actor_id)
 AND occurred_at_ms>=:start AND occurred_at_ms<:end AND occurred_at_ms<=:now
), placed_totals AS (
 SELECT actor_id,SUM(amount) AS placed_blocks,COUNT(*) AS placed_block_actions FROM placed GROUP BY actor_id
), crafted AS (
 SELECT * FROM events WHERE event_kind='item_craft_result'
 AND cause IN ('ll26.60.craft_request_success.v1','ll26.60.created_output_transfer.v1',
  'll26.60.furnace_output_extract.v1') AND actor_kind='player' AND actor_id!=''
 AND typeof(amount)='integer' AND amount BETWEEN 1 AND 4096 AND bi_item_type(primary_type)
 AND ((cause='ll26.60.craft_request_success.v1' AND (secondary_type='' OR secondary_type IS NULL))
   OR (cause='ll26.60.created_output_transfer.v1'
       AND secondary_type IN ('workbench','inventory','trade'))
   OR (cause='ll26.60.furnace_output_extract.v1' AND bi_item_type(secondary_type)))
 AND EXISTS(SELECT 1 FROM players WHERE player_id=events.actor_id)
 AND occurred_at_ms>=:start AND occurred_at_ms<:end AND occurred_at_ms<=:now
), crafted_totals AS (
 SELECT actor_id,SUM(amount) AS crafted_items,COUNT(*) AS craft_actions FROM crafted GROUP BY actor_id
), travels AS (
 SELECT * FROM events WHERE event_kind='player_travel_distance'
 AND (cause='ll26.60.poll_position_delta.v1' OR (cause='ll26.60.poll_position_delta.v2'
      AND bi_travel_v2(primary_type,amount,occurred_at_ms,details_json)))
 AND actor_kind='player' AND actor_id!=''
 AND typeof(amount)='integer' AND amount>0 AND length(primary_type) BETWEEN 1 AND 32
 AND EXISTS(SELECT 1 FROM players WHERE player_id=events.actor_id)
 AND occurred_at_ms>=:start AND occurred_at_ms<:end AND occurred_at_ms<=:now
), travel_totals AS (
 SELECT actor_id,SUM(amount) AS travel_cm FROM travels GROUP BY actor_id
), interactions AS (
 SELECT * FROM events WHERE event_kind='player_interaction_result'
 AND cause IN ('ll26.60.block_use_success.v1','ll26.60.cake_remove_slice.v1',
  'll26.60.cauldron_use_inventory.v1','ll26.60.cauldron_clear_item.v1',
  'll26.60.flower_pot_place.v1','ll26.60.noteblock_attack.v1')
 AND actor_kind='player' AND actor_id!=''
 AND typeof(amount)='integer' AND amount=1 AND bi_item_type(secondary_type)
 AND length(primary_type) BETWEEN 1 AND 64
 AND EXISTS(SELECT 1 FROM players WHERE player_id=events.actor_id)
 AND occurred_at_ms>=:start AND occurred_at_ms<:end AND occurred_at_ms<=:now
), interaction_totals AS (
 SELECT actor_id,COUNT(*) AS interactions FROM interactions GROUP BY actor_id
), damage_taken AS (
 SELECT * FROM events WHERE event_kind='player_damage_taken'
 AND cause='ll26.60.actor_hurt_event.v1' AND actor_kind='player' AND actor_id!=''
 AND typeof(amount)='integer' AND amount>0
 AND EXISTS(SELECT 1 FROM players WHERE player_id=events.actor_id)
 AND occurred_at_ms>=:start AND occurred_at_ms<:end AND occurred_at_ms<=:now
), damage_totals AS (
 SELECT actor_id,SUM(amount) AS damage_taken_tenths FROM damage_taken GROUP BY actor_id
), damage_dealt AS (
 SELECT * FROM events WHERE event_kind='player_damage_dealt'
 AND cause='ll26.60.actor_hurt_real_damage.v1' AND actor_kind='player' AND actor_id!=''
 AND typeof(amount)='integer' AND amount>0 AND bi_item_type(primary_type)
 AND EXISTS(SELECT 1 FROM players WHERE player_id=events.actor_id)
 AND occurred_at_ms>=:start AND occurred_at_ms<:end AND occurred_at_ms<=:now
), damage_dealt_totals AS (
 SELECT actor_id,SUM(amount) AS damage_dealt_tenths FROM damage_dealt GROUP BY actor_id
), broken AS (
 SELECT * FROM events WHERE event_kind='item_broken_result'
 AND cause IN ('ll26.60.durability_item_destroyed.v1','ll26.60.durability_item_destroyed.v2') AND actor_kind='player' AND actor_id!=''
 AND typeof(amount)='integer' AND amount=1 AND bi_item_type(primary_type)
 AND bi_broken_details(details_json,primary_type,cause,COALESCE(secondary_type,''))
 AND typeof(occurred_at_ms)='integer'
 AND EXISTS(SELECT 1 FROM players WHERE player_id=events.actor_id)
 AND occurred_at_ms>=:start AND occurred_at_ms<:end AND occurred_at_ms<=:now
), broken_totals AS (
 SELECT actor_id,COUNT(*) AS broken_items FROM broken GROUP BY actor_id
), sleeps AS (
 SELECT * FROM events WHERE event_kind='player_sleep_result'
 AND cause='ll26.60.sleep_flag_transition.v1' AND actor_kind='player' AND actor_id!=''
 AND typeof(amount)='integer' AND amount=1
 AND EXISTS(SELECT 1 FROM players WHERE player_id=events.actor_id)
 AND occurred_at_ms>=:start AND occurred_at_ms<:end AND occurred_at_ms<=:now
), sleep_totals AS (
 SELECT actor_id,COUNT(*) AS sleep_successes FROM sleeps GROUP BY actor_id
), sneaks AS (
 SELECT * FROM events WHERE event_kind='player_sneak_time'
 AND cause='ll26.60.sneak_duration.v1' AND actor_kind='player' AND actor_id!=''
 AND typeof(amount)='integer' AND amount>0
 AND EXISTS(SELECT 1 FROM players WHERE player_id=events.actor_id)
 AND occurred_at_ms>=:start AND occurred_at_ms<:end AND occurred_at_ms<=:now
), sneak_totals AS (
 SELECT actor_id,SUM(amount) AS sneak_time_ticks FROM sneaks GROUP BY actor_id
), modes AS (
 SELECT * FROM events WHERE event_kind='player_mode_observation'
 AND cause='ll26.60.mode_flag_observed.v1' AND actor_kind='player' AND actor_id!=''
 AND bi_mode_details(details_json,primary_type,secondary_type,occurred_at_ms,amount)
 AND EXISTS(SELECT 1 FROM players WHERE player_id=events.actor_id)
 AND occurred_at_ms>=:start AND occurred_at_ms<:end AND occurred_at_ms<=:now
), mode_totals AS (
 SELECT actor_id,
 SUM(CASE WHEN primary_type='sprint' AND secondary_type='start' THEN amount ELSE 0 END) AS sprint_starts,
 SUM(CASE WHEN primary_type='sprint' AND secondary_type='stop' THEN amount ELSE 0 END) AS sprint_stops,
 SUM(CASE WHEN primary_type='sneak' AND secondary_type='start' THEN amount ELSE 0 END) AS sneak_starts,
 SUM(CASE WHEN primary_type='sneak' AND secondary_type='stop' THEN amount ELSE 0 END) AS sneak_stops
 FROM modes GROUP BY actor_id
), starts AS (
 SELECT player_id, COUNT(*) AS sessions FROM sessions
 WHERE started_at_ms>=:start AND started_at_ms<:end AND started_at_ms<=:now GROUP BY player_id
), unresolved AS (
 SELECT player_id, SUM(CASE WHEN ended_at_ms IS NULL THEN 1 ELSE 0 END) AS open_sessions,
 SUM(CASE WHEN ended_at_ms IS NOT NULL AND (duration_ms IS NULL OR started_at_ms<0
 OR ended_at_ms<started_at_ms OR duration_ms!=ended_at_ms-started_at_ms OR ended_at_ms>:now)
 THEN 1 ELSE 0 END) AS invalid_sessions FROM sessions WHERE started_at_ms<=:now GROUP BY player_id
), actions AS (
 SELECT actor_id,
 SUM(CASE WHEN event_kind='audit_block_change' AND cause='player_destroy_attempt' THEN 1 ELSE 0 END) AS destroy_attempts,
 SUM(CASE WHEN event_kind='audit_block_change' AND cause='player_place_attempt' THEN 1 ELSE 0 END) AS place_attempts,
 SUM(CASE WHEN event_kind='session_start' THEN 1 ELSE 0 END) AS joins,
 SUM(CASE WHEN event_kind='session_end' THEN 1 ELSE 0 END) AS leaves,
 SUM(CASE WHEN event_kind='player_death_notification' AND cause='ll26.60.player_die_notification.v1' THEN 1 ELSE 0 END) AS death_notifications,
 SUM(CASE WHEN event_kind='item_use_attempt' AND cause='ll26.60.item_use_attempt.v1' THEN 1 ELSE 0 END) AS item_use_attempts
 FROM events WHERE actor_kind='player' AND occurred_at_ms>=:start AND occurred_at_ms<:end
 AND occurred_at_ms<=:now GROUP BY actor_id
), rows AS (
 SELECT p.player_id,p.last_name,CASE WHEN p.last_seen_ms<=:now THEN p.last_seen_ms ELSE NULL END AS last_active_ms,
 COALESCE(t.playtime,0) AS playtime,COALESCE(s.sessions,0) AS sessions,
 COALESCE(a.destroy_attempts,0) AS destroy_attempts,COALESCE(a.place_attempts,0) AS place_attempts,
 COALESCE(a.joins,0) AS joins,COALESCE(a.leaves,0) AS leaves,
 COALESCE(a.death_notifications,0) AS death_notifications,COALESCE(a.item_use_attempts,0) AS item_use_attempts,
 COALESCE(d.dropped_items,0) AS dropped_items,COALESCE(d.drop_actions,0) AS drop_actions,
 COALESCE(c.confirmed_deaths,0) AS confirmed_deaths,
 COALESCE(kb.killed_by_entities,0) AS killed_by_entities,
 COALESCE(m.sprint_starts,0) AS sprint_starts,COALESCE(m.sprint_stops,0) AS sprint_stops,
 COALESCE(m.sneak_starts,0) AS sneak_starts,COALESCE(m.sneak_stops,0) AS sneak_stops,
 COALESCE(k.pickup_inventory_items,0) AS pickup_inventory_items,COALESCE(k.pickup_delta_actions,0) AS pickup_delta_actions,
 COALESCE(j.jump_impulses,0) AS jump_impulses,
 COALESCE(v.direct_kills,0) AS direct_kills,COALESCE(v.direct_mob_kills,0) AS direct_mob_kills,
 COALESCE(v.direct_player_kills,0) AS direct_player_kills,
 COALESCE(pk.projectile_kills,0) AS projectile_kills,
 COALESCE(w.use_return_actions,0) AS use_return_actions,COALESCE(w.use_inventory_loss,0) AS use_inventory_loss,
 COALESCE(h.hurt_slot_losses,0) AS hurt_slot_losses,
 COALESCE(n.mined_blocks,0) AS mined_blocks,COALESCE(n.mined_block_actions,0) AS mined_block_actions,
 COALESCE(q.placed_blocks,0) AS placed_blocks,COALESCE(q.placed_block_actions,0) AS placed_block_actions,
 COALESCE(cf.crafted_items,0) AS crafted_items,COALESCE(cf.craft_actions,0) AS craft_actions,
 COALESCE(tv.travel_cm,0) AS travel_cm,COALESCE(it.interactions,0) AS interactions,
 COALESCE(dt.damage_taken_tenths,0) AS damage_taken_tenths,
 COALESCE(dd.damage_dealt_tenths,0) AS damage_dealt_tenths,
 COALESCE(br.broken_items,0) AS broken_items,
 COALESCE(sl.sleep_successes,0) AS sleep_successes,
 COALESCE(sn.sneak_time_ticks,0) AS sneak_time_ticks,
 COALESCE(u.open_sessions,0) AS open_sessions,COALESCE(u.invalid_sessions,0) AS invalid_sessions
 FROM players p LEFT JOIN times t ON p.player_id=t.player_id LEFT JOIN starts s ON p.player_id=s.player_id
 LEFT JOIN actions a ON p.player_id=a.actor_id LEFT JOIN unresolved u ON p.player_id=u.player_id
 LEFT JOIN drop_totals d ON p.player_id=d.actor_id
 LEFT JOIN death_totals c ON p.player_id=c.actor_id
 LEFT JOIN killed_by_totals kb ON p.player_id=kb.actor_id
 LEFT JOIN mode_totals m ON p.player_id=m.actor_id
 LEFT JOIN pickup_totals k ON p.player_id=k.actor_id
 LEFT JOIN jump_totals j ON p.player_id=j.actor_id
 LEFT JOIN kill_totals v ON p.player_id=v.actor_id
 LEFT JOIN projectile_totals pk ON p.player_id=pk.actor_id
 LEFT JOIN use_totals w ON p.player_id=w.actor_id
 LEFT JOIN hurt_totals h ON p.player_id=h.actor_id
 LEFT JOIN mined_totals n ON p.player_id=n.actor_id
 LEFT JOIN placed_totals q ON p.player_id=q.actor_id
 LEFT JOIN crafted_totals cf ON p.player_id=cf.actor_id
 LEFT JOIN travel_totals tv ON p.player_id=tv.actor_id
 LEFT JOIN interaction_totals it ON p.player_id=it.actor_id
 LEFT JOIN damage_totals dt ON p.player_id=dt.actor_id
 LEFT JOIN damage_dealt_totals dd ON p.player_id=dd.actor_id
 LEFT JOIN broken_totals br ON p.player_id=br.actor_id
 LEFT JOIN sleep_totals sl ON p.player_id=sl.actor_id
 LEFT JOIN sneak_totals sn ON p.player_id=sn.actor_id
)
"""


class Analytics:
    def __init__(self, database: Path, identity_key: bytes):
        self.database = database.resolve(strict=True)
        if not self.database.is_relative_to(Path(__file__).resolve().parent.parent):
            raise ValueError("このリポジトリ内のDBのみ指定できます")
        self.identity_key = identity_key

    def player_key(self, player_id: str):
        return hmac.new(self.identity_key, player_id.encode(), hashlib.sha256).hexdigest()[:32]

    def public_player(self, row):
        data = dict(row)
        data["key"] = self.player_key(data.pop("player_id"))
        data["name"] = data.pop("last_name")
        data["playtime_text"] = duration_text(data["playtime"])
        data["online"] = None
        data["estimated_live_ms"] = None
        data["unmeasured"] = UNMEASURED
        # No coverage is asserted for old DLLs or gaps. Absence of these new
        # notification records is NOT measured zero, even if another player
        # has observations. Positive values count stored notifications only.
        for metric in OPTIONAL_ACTIVITY:
            if data[metric] == 0:
                data[metric] = None
        return data

    def report(self, *, period="day", date=None, metric="playtime", player=None,
               page=0, limit=20, players_page=0, now_ms=None):
        if metric not in METRICS or not 0 <= page <= 1000 or not 1 <= limit <= 100 or not 0 <= players_page <= 1000:
            raise ValueError("ランキング指標またはページが範囲外です")
        if player is not None and not re.fullmatch(r"[0-9a-f]{32}", player):
            raise ValueError("プレイヤー選択が不正です")
        now_ms = time.time_ns() // 1_000_000 if now_ms is None else now_ms
        start, end, label = period_bounds(period, date, now_ms)
        args = {"start": start, "end": min(end, now_ms + 1), "now": now_ms,
                "limit": limit, "offset": page * limit, "players_offset": players_page * 100}
        with snapshot(self.database) as db:
            players = [self.public_player(r) for r in db.execute(CTE +
                "SELECT * FROM rows ORDER BY last_name COLLATE BINARY,player_id LIMIT 100 OFFSET :players_offset", args)]
            # Only enum-selected SQL identifiers, never user strings as SQL.
            ranked = db.execute(CTE + f"SELECT * FROM rows WHERE {metric}>0 ORDER BY {metric} DESC,last_name COLLATE BINARY,player_id LIMIT :limit OFFSET :offset", args)
            ranking = [{"rank": page * limit + i + 1, "key": self.player_key(r["player_id"]),
                        "name": r["last_name"], "value": r[metric],
                        "display": duration_text(r[metric]) if metric == "playtime" else str(r[metric])}
                       for i, r in enumerate(ranked)]
            totals = dict(db.execute(CTE + """SELECT COUNT(*) AS player_count,
                COALESCE(SUM(playtime),0) AS playtime,COALESCE(SUM(sessions),0) AS sessions,
                COALESCE(SUM(destroy_attempts),0) AS destroy_attempts,COALESCE(SUM(place_attempts),0) AS place_attempts,
                SUM(death_notifications) AS death_notifications,SUM(item_use_attempts) AS item_use_attempts,
                SUM(dropped_items) AS dropped_items,SUM(drop_actions) AS drop_actions,
                SUM(confirmed_deaths) AS confirmed_deaths,
                SUM(killed_by_entities) AS killed_by_entities,
                SUM(sprint_starts) AS sprint_starts,SUM(sprint_stops) AS sprint_stops,
                SUM(sneak_starts) AS sneak_starts,SUM(sneak_stops) AS sneak_stops,
                SUM(pickup_inventory_items) AS pickup_inventory_items,SUM(pickup_delta_actions) AS pickup_delta_actions,
                SUM(jump_impulses) AS jump_impulses,
                SUM(direct_kills) AS direct_kills,SUM(direct_mob_kills) AS direct_mob_kills,SUM(direct_player_kills) AS direct_player_kills,
                SUM(projectile_kills) AS projectile_kills,
                SUM(use_return_actions) AS use_return_actions,SUM(use_inventory_loss) AS use_inventory_loss,
                SUM(hurt_slot_losses) AS hurt_slot_losses,
                SUM(mined_blocks) AS mined_blocks,SUM(mined_block_actions) AS mined_block_actions,
                SUM(placed_blocks) AS placed_blocks,SUM(placed_block_actions) AS placed_block_actions,
                SUM(crafted_items) AS crafted_items,SUM(craft_actions) AS craft_actions,
                SUM(travel_cm) AS travel_cm,SUM(interactions) AS interactions,
                SUM(damage_taken_tenths) AS damage_taken_tenths,SUM(damage_dealt_tenths) AS damage_dealt_tenths,
                SUM(broken_items) AS broken_items,SUM(sleep_successes) AS sleep_successes,
                SUM(sneak_time_ticks) AS sneak_time_ticks,
                COALESCE(SUM(open_sessions),0) AS open_sessions,COALESCE(SUM(invalid_sessions),0) AS invalid_sessions FROM rows""", args).fetchone())
            totals["playtime_text"] = duration_text(totals["playtime"])
            for optional in OPTIONAL_ACTIVITY:
                if not totals[optional]:
                    totals[optional] = None
            selected = None
            found = None
            if player:
                # Map opaque, process-local UI key; never expose raw UUID/XUID.
                found = next((r[0] for r in db.execute("SELECT player_id FROM players LIMIT 100001") if self.player_key(r[0]) == player), None)
                if found is None:
                    raise LookupError("プレイヤーが見つかりません")
                selected = self.public_player(db.execute(CTE + "SELECT * FROM rows WHERE player_id=:id", args | {"id": found}).fetchone())
            drop_args = args | {"selected": found}
            drop_types = [dict(r) for r in db.execute(CTE + """SELECT primary_type AS type,
                SUM(amount) AS items,COUNT(*) AS actions FROM drops
                WHERE (:selected IS NULL OR actor_id=:selected)
                GROUP BY primary_type ORDER BY items DESC,primary_type COLLATE BINARY LIMIT 100""", drop_args)]
            drop_observed = dict(db.execute(CTE + """SELECT MIN(occurred_at_ms) AS first_ms,
                MAX(occurred_at_ms) AS last_ms FROM drops
                WHERE (:selected IS NULL OR actor_id=:selected)""", drop_args).fetchone())
            pickup_types = [dict(r) for r in db.execute(CTE + """SELECT primary_type AS type,
                SUM(amount) AS items,COUNT(*) AS actions FROM pickups
                WHERE (:selected IS NULL OR actor_id=:selected)
                GROUP BY primary_type ORDER BY items DESC,primary_type COLLATE BINARY LIMIT 100""", drop_args)]
            pickup_observed = dict(db.execute(CTE + """SELECT MIN(occurred_at_ms) AS first_ms,
                MAX(occurred_at_ms) AS last_ms FROM pickups
                WHERE (:selected IS NULL OR actor_id=:selected)""", drop_args).fetchone())
            jump_observed = dict(db.execute(CTE + """SELECT MIN(occurred_at_ms) AS first_ms,
                MAX(occurred_at_ms) AS last_ms FROM jumps
                WHERE (:selected IS NULL OR actor_id=:selected)""", drop_args).fetchone())
            kill_types = [dict(r) for r in db.execute(CTE + """SELECT primary_type AS type,secondary_type AS kind,
                COUNT(*) AS count FROM observed_kills WHERE (:selected IS NULL OR actor_id=:selected)
                GROUP BY primary_type,secondary_type ORDER BY count DESC,primary_type COLLATE BINARY,secondary_type LIMIT 100""",drop_args)]
            kill_observed = dict(db.execute(CTE + """SELECT MIN(occurred_at_ms) AS first_ms,
                MAX(occurred_at_ms) AS last_ms FROM observed_kills
                WHERE (:selected IS NULL OR actor_id=:selected)""",drop_args).fetchone())
            projectile_types = [dict(r) for r in db.execute(CTE + """SELECT primary_type AS type,
                secondary_type AS kind,COUNT(*) AS count FROM projectile_kills
                WHERE (:selected IS NULL OR actor_id=:selected) GROUP BY primary_type,secondary_type
                ORDER BY count DESC,primary_type COLLATE BINARY,secondary_type LIMIT 100""",drop_args)]
            use_types = [dict(r) for r in db.execute(CTE + """SELECT primary_type AS type,secondary_type AS method,
                COUNT(*) AS actions,NULLIF(SUM(loss),0) AS items FROM use_results
                WHERE (:selected IS NULL OR actor_id=:selected) GROUP BY primary_type,secondary_type
                ORDER BY actions DESC,primary_type COLLATE BINARY,secondary_type LIMIT 100""",drop_args)]
            use_observed = dict(db.execute(CTE + """SELECT MIN(occurred_at_ms) AS first_ms,
                MAX(occurred_at_ms) AS last_ms FROM use_results
                WHERE (:selected IS NULL OR actor_id=:selected)""",drop_args).fetchone())
            hurt_types = [dict(r) for r in db.execute(CTE + """SELECT primary_type AS type,
                COUNT(*) AS count FROM hurt_slots WHERE (:selected IS NULL OR actor_id=:selected)
                GROUP BY primary_type ORDER BY count DESC,primary_type COLLATE BINARY LIMIT 100""",drop_args)]
            hurt_observed = dict(db.execute(CTE + """SELECT MIN(occurred_at_ms) AS first_ms,
                MAX(occurred_at_ms) AS last_ms FROM hurt_slots
                WHERE (:selected IS NULL OR actor_id=:selected)""",drop_args).fetchone())
            mined_types = [dict(r) for r in db.execute(CTE + """SELECT primary_type AS type,
                SUM(amount) AS count FROM mined WHERE (:selected IS NULL OR actor_id=:selected)
                GROUP BY primary_type ORDER BY count DESC,primary_type COLLATE BINARY LIMIT 100""",drop_args)]
            mined_observed = dict(db.execute(CTE + """SELECT MIN(occurred_at_ms) AS first_ms,
                MAX(occurred_at_ms) AS last_ms FROM mined
                WHERE (:selected IS NULL OR actor_id=:selected)""",drop_args).fetchone())
            placed_types = [dict(r) for r in db.execute(CTE + """SELECT primary_type AS type,
                SUM(amount) AS count FROM placed WHERE (:selected IS NULL OR actor_id=:selected)
                GROUP BY primary_type ORDER BY count DESC,primary_type COLLATE BINARY LIMIT 100""",drop_args)]
            placed_observed = dict(db.execute(CTE + """SELECT MIN(occurred_at_ms) AS first_ms,
                MAX(occurred_at_ms) AS last_ms FROM placed
                WHERE (:selected IS NULL OR actor_id=:selected)""",drop_args).fetchone())
            crafted_types = [dict(r) for r in db.execute(CTE + """SELECT primary_type AS type,
                SUM(amount) AS items,COUNT(*) AS actions FROM crafted
                WHERE (:selected IS NULL OR actor_id=:selected)
                GROUP BY primary_type ORDER BY items DESC,primary_type COLLATE BINARY LIMIT 100""",drop_args)]
            crafted_observed = dict(db.execute(CTE + """SELECT MIN(occurred_at_ms) AS first_ms,
                MAX(occurred_at_ms) AS last_ms FROM crafted
                WHERE (:selected IS NULL OR actor_id=:selected)""",drop_args).fetchone())
            travel_modes = [dict(r) for r in db.execute(CTE + """SELECT primary_type AS mode,
                SUM(amount) AS cm FROM travels WHERE (:selected IS NULL OR actor_id=:selected)
                GROUP BY primary_type ORDER BY cm DESC,primary_type COLLATE BINARY LIMIT 20""",drop_args)]
            travel_observed = dict(db.execute(CTE + """SELECT MIN(occurred_at_ms) AS first_ms,
                MAX(occurred_at_ms) AS last_ms FROM travels
                WHERE (:selected IS NULL OR actor_id=:selected)""",drop_args).fetchone())
            interaction_types = [dict(r) for r in db.execute(CTE + """SELECT primary_type AS key,
                COUNT(*) AS count FROM interactions WHERE (:selected IS NULL OR actor_id=:selected)
                GROUP BY primary_type ORDER BY count DESC,primary_type COLLATE BINARY LIMIT 100""",drop_args)]
            interaction_observed = dict(db.execute(CTE + """SELECT MIN(occurred_at_ms) AS first_ms,
                MAX(occurred_at_ms) AS last_ms FROM interactions
                WHERE (:selected IS NULL OR actor_id=:selected)""",drop_args).fetchone())
            damage_rows = [dict(r) for r in db.execute(CTE + """SELECT secondary_type AS kind,
                SUM(amount) AS tenths,COUNT(*) AS count FROM damage_taken
                WHERE (:selected IS NULL OR actor_id=:selected)
                GROUP BY secondary_type ORDER BY tenths DESC,secondary_type COLLATE BINARY LIMIT 10""",drop_args)]
            damage_observed = dict(db.execute(CTE + """SELECT MIN(occurred_at_ms) AS first_ms,
                MAX(occurred_at_ms) AS last_ms FROM damage_taken
                WHERE (:selected IS NULL OR actor_id=:selected)""",drop_args).fetchone())
            broken_types = [dict(r) for r in db.execute(CTE + """SELECT primary_type AS type,
                COUNT(*) AS count FROM broken WHERE (:selected IS NULL OR actor_id=:selected)
                GROUP BY primary_type ORDER BY count DESC,primary_type COLLATE BINARY LIMIT 100""",drop_args)]
            sleep_rows = dict(db.execute(CTE + """SELECT COUNT(*) AS count,
                MIN(occurred_at_ms) AS first_ms, MAX(occurred_at_ms) AS last_ms FROM sleeps
                WHERE (:selected IS NULL OR actor_id=:selected)""",drop_args).fetchone())
            sneak_rows = dict(db.execute(CTE + """SELECT SUM(amount) AS ticks,
                COUNT(*) AS sessions FROM sneaks WHERE (:selected IS NULL OR actor_id=:selected)""",drop_args).fetchone())
            death_causes = [dict(r) for r in db.execute(CTE + """SELECT secondary_type AS code,
                COUNT(*) AS count FROM deaths WHERE (:selected IS NULL OR actor_id=:selected)
                GROUP BY secondary_type ORDER BY count DESC,secondary_type COLLATE BINARY LIMIT 36""", drop_args)]
            killed_by_types = [dict(r) for r in db.execute(CTE + """SELECT primary_type AS type,
                secondary_type AS path,COUNT(*) AS count FROM killed_by
                WHERE (:selected IS NULL OR actor_id=:selected) GROUP BY primary_type,secondary_type
                ORDER BY count DESC,primary_type COLLATE BINARY,secondary_type COLLATE BINARY LIMIT 100""", drop_args)]
            killed_by_observed = dict(db.execute(CTE + """SELECT MIN(occurred_at_ms) AS first_ms,
                MAX(occurred_at_ms) AS last_ms FROM killed_by
                WHERE (:selected IS NULL OR actor_id=:selected)""", drop_args).fetchone())
            for row in death_causes:
                row['label'] = DAMAGE_CAUSES[cause_number(row['code'])]
            death_observed = dict(db.execute(CTE + """SELECT MIN(occurred_at_ms) AS first_ms,
                MAX(occurred_at_ms) AS last_ms FROM deaths
                WHERE (:selected IS NULL OR actor_id=:selected)""", drop_args).fetchone())
            mode_rows = [dict(r) for r in db.execute(CTE + """SELECT primary_type AS mode,
                NULLIF(SUM(CASE WHEN secondary_type='start' THEN amount ELSE 0 END),0) AS starts,
                NULLIF(SUM(CASE WHEN secondary_type='stop' THEN amount ELSE 0 END),0) AS stops
                FROM modes WHERE (:selected IS NULL OR actor_id=:selected)
                GROUP BY primary_type ORDER BY primary_type COLLATE BINARY LIMIT 2""", drop_args)]
            mode_observed = dict(db.execute(CTE + """SELECT MIN(bi_mode_first_ms(details_json)) AS first_ms,
                MAX(occurred_at_ms) AS last_ms FROM modes
                WHERE (:selected IS NULL OR actor_id=:selected)""", drop_args).fetchone())
            peak = db.execute(CTE + """, endpoints AS (
                SELECT b AS t,1 AS delta FROM merged UNION ALL SELECT e,-1 FROM merged
                ), changes AS (SELECT t,SUM(delta) AS delta FROM endpoints GROUP BY t),
                online AS (SELECT SUM(delta) OVER (ORDER BY t) AS n FROM changes)
                SELECT MAX(n) FROM online""", args).fetchone()[0]
            latest_event = db.execute("SELECT MAX(occurred_at_ms) FROM events WHERE occurred_at_ms<=?", (now_ms,)).fetchone()[0]
            tables = {r[0] for r in db.execute("SELECT name FROM sqlite_master WHERE type='table'")}
            tick_rows, last_tick = (0, None)
            if "server_tick_samples" in tables:
                tick_rows, last_tick = db.execute("SELECT COUNT(*),MAX(occurred_at_ms) FROM server_tick_samples WHERE occurred_at_ms<=?", (now_ms,)).fetchone()
            telemetry = read_telemetry(db, now_ms)
        return {
            "companion": True, "read_only": True, "generated_at_ms": now_ms,
            "period": {"kind": period, "label": label, "timezone": "JST (UTC+09:00)", "start_ms": start, "end_ms_exclusive": args["end"]},
            "totals": totals, "players": players, "players_page": players_page,
            "selected_player": selected, "ranking": ranking, "page": page, "limit": limit,
            "metric": metric, "metric_label": METRICS[metric][0], "metric_unit": METRICS[metric][1],
            "activity_note": "死亡処理通知・使用試行・ブロック破壊/設置の試行は処理前で、処理後の死亡フラグ成立や投棄/使用/設置true応答とは別です。使用true応答・所持品減少も効果完了や消費成功の証明ではありません。ブロック成功はGameMode経路で同座標の状態変化を確認した観測で、全経路・ワールド保存の証明ではありません。新項目の記録なしは未計測期間を含むためnull（不明）。期間内の全行動や稼働DLLの接続済みを保証しません。",
            "item_drops": {"types": drop_types, "type_limit": 100, "observed": drop_observed,
                "scope": "選択プレイヤー" if selected else "全プレイヤー",
                "source": "ll26.60.player_drop_returned_true.v1",
                "definition": "Player::$drop がtrueを返した非randomly呼び出しの入口スタック個数。インベントリ差分・生成物の永続化は未検証。記録のない期間は0と断定しません。最初/最後の観測間も連続収集保証ではありません。"},
            "item_pickups": {"types": pickup_types, "type_limit": 100, "observed": pickup_observed,
                "scope": "選択プレイヤー" if selected else "全プレイヤー", "source": PICKUP_SOURCE,
                "definition": "Player::takeがtrueを返し、同じプレイヤー・主インベントリで同種類の所持数が増加した観測。個数は入口スタック数ではなく前後差分（入口個数以下）。主所持品のみ最大128枠、入れ子・容器/サイズ変更・不明差分は除外。他Modの変更も含み得て、全拾得数・永続転送・NBT別内訳の証明ではありません。無記録は不明です。"},
            "player_jumps": {"observed": jump_observed, "source": JUMP_EVENT_SOURCE,
                "legacy_sources": [JUMP_SOURCE],
                "scope": "選択プレイヤー" if selected else "全プレイヤー",
                "count": selected["jump_impulses"] if selected else totals["jump_impulses"],
                "definition": "現在はLeviLamina::PlayerJumpEventのpost_auth_input通知数です。発行側の接地・上昇判定を使い、後続リスナーのキャンセルや空中移動の完了を独立検証しません。履歴のMob::jumpFromGround正常終了後の同一Player/次元・PosDelta.y正の増加観測も含みます（旧値は×10^6丸め・絶対値16以下、入れ子等は除外）。空中移動完了数/距離でもありません。他Modの影響や未発火の経路は含められず、無記録は不明です。"},
            "direct_kills": {"types": kill_types, "type_limit": 100, "observed": kill_observed,
                "source": KILL_SOURCE, "scope": "選択プレイヤー" if selected else "全プレイヤー",
                "definition": "Mob::$dieの前後で同じ未削除被害者の死亡フラグが未成立→成立となった観測。EntityAttack(2)・非child/非block・攻撃者IDと加害者IDが一致し、同じLevelのPlayerへ解決できた入口の報告者だけへ帰属。自分・飛び道具・間接/環境死・不明は除外。全キル数・永続死亡・任意Mod下の真のキラーを保証せず、この経路を通らないPlayer死亡は欠測。無記録は不明です。"},
            "projectile_kills": {"types": projectile_types,
                "count": selected["projectile_kills"] if selected else totals["projectile_kills"],
                "source": PROJECTILE_KILL_SOURCE,
                "definition": "Mob死亡フラグ成立と、同じLevelのPlayerへ解決できた子飛び道具source所有者の観測。source IDと加害飛び道具IDは異なる非0値が必須。直接攻撃へ混ぜず、消失した所有者・環境・自己・間接全般は除外。全キル数・永続死亡・実プレイ受入は未検証、無記録は未計測。"},
            "item_use_results": {"types": use_types, "type_limit": 100, "observed": use_observed,
                "source": "ll26.60.player_use_item_consumed.v1 / ll26.60.item_hurt_used.v1 / ll26.60.gamemode_use_return.v1",
                "scope": "選択プレイヤー" if selected else "全プレイヤー",
                "definition": "LK-Stats v0.13.0と同じ使用判定。Player::$useItemが消費系の方法(Eat/Consume/Throw/FillBottle/PourBucket/Placeと骨粉Interact)で戻った後、または非防具の耐久消費が成功した後の観測。効果完了・消費成立・全用途の成功ではありません。旧来のGameMode::$useItemの所持数差分は履歴データとして残し、新規の使用は上記2経路のみで数えます。他Modの変更を含み得て全使用数は保証しません。無記録は不明です。"},
            "item_hurt_slots": {"types": hurt_types, "type_limit": 100, "observed": hurt_observed,
                "source": HURT_SOURCE, "scope": "選択プレイヤー" if selected else "全プレイヤー",
                "definition": "正の耐久ダメージを伴うItemStackBase::hurtAndBreakがtrueを返し、同じ未削除Player・主インベントリ・スロットで入口1個→処理後0個を観測。破損成立・因果関係・永続保存の証明ではありません。最大128枠、コピー・装備/副手・入れ子・容器/サイズ/スロット参照変更は対象外。同じ型を再装填する等の経路も欠測。記録なしは0と断定しません。"},
            "blocks_mined": {"types": mined_types, "type_limit": 100, "observed": mined_observed,
                "source": MINED_V2_SOURCE, "legacy_sources": [MINED_SOURCE], "scope": "選択プレイヤー" if selected else "全プレイヤー",
                "count": selected["mined_blocks"] if selected else totals["mined_blocks"],
                "definition": "GameMode::destroyBlockのtrue応答と同座標の種類変化を入口の型で1回記録。v2は再解決した未削除Player・同Level/次元を確認してから読む限定観測です。旧v1履歴も含みますが、旧行をv2のLevel/未削除確認の証明にはしません。再設置や同型のまま・失敗・取消・入れ子・不明は除外。ドロップ品・NBT・素材の個数、クリエイティブ等の別経路、全破壊数・最終永続化・専属因果の証明ではありません。無記録は不明です。"},
            "blocks_placed": {"types": placed_types, "type_limit": 100, "observed": placed_observed,
                "source": PLACED_V2_SOURCE, "legacy_sources": [PLACED_SOURCE], "scope": "選択プレイヤー" if selected else "全プレイヤー",
                "count": selected["placed_blocks"] if selected else totals["placed_blocks"],
                "definition": "GameMode::buildBlockのtrue応答と同座標の非空気への種類変化を処理後の型で1回記録。v2は再解決した未削除Player・同Level/次元を確認。旧v1履歴も含みますがv2のframe証明へ昇格しません。手の列とJSONも照合。同型のまま・空気・失敗・入れ子・不明は除外。隣接座標や複数ブロックの設置は追跡せず、全設置数・最終永続化・専属因果の証明ではありません。無記録は不明です。"},
            "items_crafted": {"types": crafted_types, "type_limit": 100, "observed": crafted_observed,
                "scope": "選択プレイヤー" if selected else "全プレイヤー",
                "count": selected["crafted_items"] if selected else totals["crafted_items"],
                "source": "ll26.60.created_output_transfer.v1 / ll26.60.furnace_output_extract.v1 / ll26.60.craft_request_success.v1",
                "definition": "LK-Stats v0.13.0と同じ生成物の産出。ItemStackRequestActionHandler::_handleTransferがCreatedOutputContainerからItemStackNetResult::Successで転送した個数を、開いている画面(作業台/インベントリ/取引)別に集計し、かまど・燻製器の完成品取り出しはモデル差分で加算します。旧来のpostRequest由来の記録は履歴として残します。転送失敗・画面外・上限外は除外し、レシピNBTや全クラフト経路は証明しません。無記録は不明です。"},
            "travel_distance": {"modes": travel_modes, "observed": travel_observed,
                "scope": "選択プレイヤー" if selected else "全プレイヤー",
                "cm": selected["travel_cm"] if selected else totals["travel_cm"],
                "source": "ll26.60.poll_position_delta.v1 / ll26.60.poll_position_delta.v2",
                "definition": "サーバー座標の距離観測。旧v1の水平・丸め・重複水系分類は履歴のまま。v2は地上XZ、落下/登攀は上下、飛行/泳ぎ/騎乗/水中は3D。5秒窓で小数cmを合算後切捨て、次元・乗り物・死亡・JST日境界・時刻逆転・1秒超gap・10block超差分を跨ぎません。最終観測時刻の期間へ配賦。短距離teleport等は未排除のため全移動距離の証明ではありません。無記録は不明です。"},
            "block_interactions": {"types": interaction_types, "type_limit": 100, "observed": interaction_observed,
                "scope": "選択プレイヤー" if selected else "全プレイヤー",
                "count": selected["interactions"] if selected else totals["interactions"],
                "source": "Block::use / CakeBlock::removeCakeSlice / CauldronBlock::_useInventory / CauldronBlock::_sendCauldronUsedEventToClient / FlowerPotBlock::use / NoteBlock::$attack (post-origin)",
                "definition": "LK-Stats v0.13.0と同じブロック専用フックの成功後観測のみ。Block::useがtrueを返した後に対応表と一致したブロック、ケーキ1切れ、大釜のバケツ/洗浄、植木鉢への植え付け、音符ブロックの演奏を数えます。対応表にないブロックや失敗応答は数えず、画面の実描画や取引成立は保証しません。村人取引・釣り・動物相互作用・エンチャント等この経路に現れない操作は欠測です。無記録は不明です。"},
            "damage": {"kinds": damage_rows, "observed": damage_observed,
                "scope": "選択プレイヤー" if selected else "全プレイヤー",
                "taken_tenths": selected["damage_taken_tenths"] if selected else totals["damage_taken_tenths"],
                "dealt_tenths": selected["damage_dealt_tenths"] if selected else totals["damage_dealt_tenths"],
                "source": "ll26.60.actor_hurt_event.v1 / ll26.60.actor_hurt_real_damage.v1",
                "observation_stage": "pre_origin_notification", "final_world_outcome_verified": False,
                "definition": "被弾・与ダメージともActorHurtEventの処理前申告量を1/10単位(1ハート=20)へ換算。与ダメージは加害エンティティの一意IDが追跡中プレイヤーと一致した場合のみ帰属し、攻撃行動の回数をダメージとして代用しません。後段の取消・変更や最終HP適用を確認した実ダメージではありません。吸収・耐性・軽減後は未計測、飛び道具の所有権帰属は欠測です。無記録は不明です。"},
            "broken_items": {"types": broken_types,
                "scope": "選択プレイヤー" if selected else "全プレイヤー",
                "count": selected["broken_items"] if selected else totals["broken_items"],
                "source": BROKEN_V2, "legacy_sources": [BROKEN_V1],
                "definition": "耐久処理true応答・入口耐久値と入力閾値・同じスロットの1→0を照合。v2は未削除Player・同Level/次元と主所持品/副手/防具4枠を確認。旧v1は主所持品のみでv2のframe証明にはしません。コピー・参照交換・入れ子・再装填等は欠測、全破損・専属因果・最終保存の証明ではありません。XYZは未読取、無記録は不明です。"},
            "sleep": {"count": sleep_rows.get("count") or 0, "first_ms": sleep_rows.get("first_ms"),
                "last_ms": sleep_rows.get("last_ms"),
                "scope": "選択プレイヤー" if selected else "全プレイヤー",
                "source": "ll26.60.sleep_flag_transition.v1",
                "definition": "Player::isSleeping() がサーバーtick観測で false→true に遷移した回数のみ。ベッドへの右クリック試行は数えません。起床時刻や睡眠時間は未計測です。"},
            "sneak": {"ticks": sneak_rows.get("ticks"), "sessions": sneak_rows.get("sessions"),
                "scope": "選択プレイヤー" if selected else "全プレイヤー",
                "source": "ll26.60.sneak_duration.v1",
                "definition": "PlayerSneakingEventからPlayerSneakedEventまでの実時間をtick(50ms)へ換算した合計。イベント欠落・切断時は停止時に確定します。フラグ観測の250ms遅延カウントとは別で、継続時間として扱います。"},
            "player_deaths": {"causes": death_causes, "observed": death_observed,
                "scope": "選択プレイヤー" if selected else "全プレイヤー", "source": DEATH_SOURCE,
                "definition": "Player::$dieの前後で同じ未削除エンティティの死亡フラグが未成立→成立に変化した観測。死亡処理前通知とは別です。原因は入口のコード、キラー帰属・ワールドへの死亡結果保存は未検証。無記録は不明、観測間も連続収集保証なし。"},
            "killed_by": {"types": killed_by_types, "observed": killed_by_observed,
                "count": selected["killed_by_entities"] if selected else totals["killed_by_entities"],
                "scope": "選択プレイヤー" if selected else "全プレイヤー", "source": KILLED_BY_SOURCE,
                "definition": "死亡フラグ成立の入口で、同じLevelの未削除source actorを解決した種類別観測。直接攻撃と子飛び道具のsource所有者を区別。環境・自己・所有者不明・再入は除外。原因コードを種類に変換しません。間接攻撃全般・所有者消失・ワールド保存・実プレイ受入は未検証。無記録は未計測です。"},
            "player_modes": {"modes": mode_rows, "observed": mode_observed, "source": MODE_SOURCE,
                "scope": "選択プレイヤー" if selected else "全プレイヤー",
                "definition": "処理前通知の250ms〜2秒後に同じエンティティのサーバーフラグを確認した開始/終了の観測数。短い切替・連打は合流/欠測し完全な回数ではありません。最大256人、250ms間隔の確認、5秒集約/JST日境界・退出・停止時flush。継続時間・移動距離・現在状態は未計測、無記録は不明です。"},
            "last_event_ms": latest_event, "source_age_ms": None if latest_event is None else max(0, now_ms-latest_event),
            "server": telemetry | {"peak_closed_lower_bound": peak,
                       "legacy_tick_row_count": tick_rows, "legacy_tick_last_ms": last_tick},
            "unmeasured": UNMEASURED,
            "semantics": "JST区間に重なる完結済みセッションの和集合のみを確定時間に算入。未完了・不正セッションは加算せず、オンラインも推定しません。行動は各ソース定義に従い、試行・通知・処理後フラグ・true戻り値・同座標の状態変化を区別。ブロック成功はGameMode経路の観測に限られ、素材の個数や収集漏れ・別経路は復元できません。",
        }
