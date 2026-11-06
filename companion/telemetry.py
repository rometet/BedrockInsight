"""Explicit busy-tick/interval provenance, never interprets legacy MSPT."""
import json
import math

SAMPLE_CAUSE = "ll26.60.interval.v1"
END_CAUSE = "ll26.60.interval.end.v1"
START_CAUSE = "ll26.60.interval.start.v1"
STALE_MS = 20000
BUSY_CAUSE = "ll26.60.busytick.v1"
BUSY_START = "ll26.60.busytick.start.v1"
BUSY_END = "ll26.60.busytick.end.v1"
BUSY_INVALID = "ll26.60.busytick.invalid.v1"


def read_busy(db, now_ms):
    row = db.execute("""SELECT occurred_at_ms,cause,session_id,amount,details_json,online_players
        FROM events WHERE event_kind='server_tick' AND actor_kind='system'
        AND cause LIKE 'll26.60.busytick.%' ORDER BY event_id DESC LIMIT 1""").fetchone()
    if row is None:
        return None # only the old interval source is available
    return parse_busy_observation(row, now_ms)


def parse_busy_observation(row, now_ms):
    """Same strict sample semantics for live reads and closed-run verification.

    A historical parsed row is NOT current server state. read_busy still reads
    the latest durable lifecycle row, so end/invalid/start remain authoritative.
    """
    result = {"sampling_status":"unavailable", "measurement_method":"Level::$tick/Highest+Lowest",
              "effective_tps":None, "processing_mspt":None, "peak_mspt":None, "p95_mspt":None,
              "current_online":None, "sampled_peak_online":None, "sample_count":0,
              "last_sample_ms":None, "sample_age_ms":None, "last_effective_tps":None,
              "window_ms":None, "frame_count":0, "reason":"有効な処理時間サンプルなし。"}
    stamp, cause, run_id, ticks, raw, online = row
    if cause == BUSY_END:
        result.update(sampling_status="stopped", reason="Level処理タイマーは終了済み。現在MSPT/TPS/人数を表示しません。")
        return result
    if cause == BUSY_INVALID:
        result.update(sampling_status="invalid", reason="Level/スレッド/再帰/例外の検査で処理タイマーを無効化しました。")
        return result
    try:
        if (type(stamp) is not int or not 0 <= stamp <= now_ms or not isinstance(run_id,str)
                or not run_id.startswith("lltick-") or len(run_id)>128):
            raise ValueError()
        if cause == BUSY_START:
            if now_ms-stamp > STALE_MS:
                result.update(sampling_status="stale",reason="処理タイマー開始後のサンプルなし（20秒超）。")
            else:
                result.update(sampling_status="warming_up",reason="新起動のLevel処理タイマーを計測中。初回5秒窓を待っています。")
            return result
        if cause != BUSY_CAUSE or not isinstance(raw,str) or len(raw)>2048:
            raise ValueError()
        data=json.loads(raw)
        wall,busy,peak,frames,online_peak=(data[k] for k in
            ("wall_ns","busy_ns","max_inner_ns","frames","sampled_peak_online"))
        if (data.get("clock")!="steady" or data.get("hook")!="Level::$tick/Highest+Lowest"
                or any(type(v) is not int for v in (ticks,wall,busy,peak,frames,online,online_peak))
                or not 5_000_000_000<=wall<=30_000_000_000 or not 0<=busy<=wall
                or not 0<=ticks<=1_000_000 or not 1<=frames<=1_000_000
                or not 0<=peak<=busy or (ticks==0 and peak!=0)
                or not -1<=online<=100000 or not max(-1,online)<=online_peak<=100000):
            raise ValueError()
    except (ValueError,TypeError,KeyError):
        result.update(sampling_status="invalid",reason="最新の処理サンプルの方式・値・時刻が不正。過去の値へ戻しません。")
        return result
    age=now_ms-stamp
    result.update(last_sample_ms=stamp,sample_age_ms=age,sample_count=ticks,
                  window_ms=wall/1e6,frame_count=frames,last_effective_tps=round(ticks*1e9/wall,3))
    if age>STALE_MS:
        result.update(sampling_status="stale",reason="Level処理サンプルから20秒超。現在MSPT/TPS/人数は非表示。")
        return result
    result.update(sampling_status="measured" if ticks else "frozen",effective_tps=round(ticks*1e9/wall,3),
                  processing_mspt=round(busy/ticks/1e6,6) if ticks else None,
                  peak_mspt=round(peak/1e6,6) if ticks else None,
                  current_online=None if online<0 else online,
                  sampled_peak_online=None if online_peak<0 else online_peak,
                  reason=("Level::$tick処理中の実測時間÷実行回数（外部の待ち時間を除外）。TPSは実行回数÷壁時計窓、warpは20超も表示。"
                          + f" 窓{wall/1e9:.3f}秒 / 実行{ticks}回 / 最内周1tick最大{peak/1e6:.3f}ms（平均と計測範囲が異なります）。"
                          if ticks else "フレームは観測していますが実tick実行0回。0TPSを観測、MSPTは未定義です。"))
    return result


def read_telemetry(db, now_ms):
    result = {"sampling_status": "unavailable", "effective_tps": None,
              "processing_mspt": None, "current_online": None,
              "sample_count": 0, "last_sample_ms": None,
              "sample_age_ms": None, "sampled_peak_online": None,
              "last_effective_tps": None, "window_ms": None,
              "reason": "対応する実測tickサンプルなし。稼働DLLは未接続の場合があります。MSPT・オンラインを推測しません。"}
    columns = {r[1] for r in db.execute("PRAGMA table_info(events)")}
    if not {"session_id", "amount", "details_json", "online_players"} <= columns:
        return result
    busy = read_busy(db, now_ms)
    if busy is not None:
        return busy # busy source lifecycle is authoritative; no interval fallback
    # Most recent durable sequence, not wall-clock sort: clock corrections and
    # end-of-run invalidate a previous healthy sample. Never fall back to an
    # older valid row after a malformed/future/newer record.
    row = db.execute("""SELECT event_id,occurred_at_ms,cause,session_id,amount,
        details_json,online_players FROM events WHERE event_kind='server_tick'
        AND actor_kind='system' AND cause IN (?,?,?) ORDER BY event_id DESC LIMIT 1""",
        (SAMPLE_CAUSE, END_CAUSE, START_CAUSE)).fetchone()
    if row is None:
        return result
    if row[2] == END_CAUSE:
        result.update(sampling_status="stopped", reason="計測停止・無効化通知を受信。現在TPS・オンラインは表示しません。")
        return result
    if row[2] == START_CAUSE:
        stamp, run_id = row[1], row[3]
        if (type(stamp) is not int or not 0 <= stamp <= now_ms or not isinstance(run_id, str)
                or not run_id.startswith("lltick-") or len(run_id) > 128):
            result.update(sampling_status="invalid", reason="最新の計測開始通知が不正・未来時刻。過去のTPSは表示しません。")
        elif now_ms - stamp > STALE_MS:
            result.update(sampling_status="stale", reason="計測開始後の有効サンプルなし（20秒超）。現在TPS・人数は未計測。")
        else:
            result.update(sampling_status="warming_up", reason="新しい計測起動の初回5秒窓を待っています。前回起動のTPS・人数は流用しません。")
        return result
    try:
        stamp, run_id, count, raw, online = row[1], row[3], row[4], row[5], row[6]
        if not isinstance(raw, str) or len(raw) > 1024:
            raise ValueError()
        details = json.loads(raw)
        elapsed, peak = details["window_us"], details["sampled_peak_online"]
        if (not isinstance(run_id, str) or not run_id.startswith("lltick-") or len(run_id) > 128
                or details.get("clock") != "steady"
                or any(type(v) is not int for v in (stamp, count, elapsed, online, peak))
                or not 5_000_000 <= elapsed <= 30_000_000 or not 1 <= count <= 10000
                or not -1 <= online <= 100000 or not max(-1, online) <= peak <= 100000
                or not 0 <= stamp <= now_ms):
            raise ValueError()
        tps = min(20.0, count * 1_000_000 / elapsed)
        if not math.isfinite(tps) or tps <= 0:
            raise ValueError()
    except (ValueError, TypeError, KeyError, json.JSONDecodeError):
        result.update(sampling_status="invalid", reason="最新の計測行が不正・未来時刻。以前の行で現在値を埋めません。")
        return result
    age = now_ms - stamp
    result.update(last_sample_ms=stamp, sample_age_ms=age, sample_count=count,
                  window_ms=elapsed / 1000, last_effective_tps=round(tps, 3))
    if age > STALE_MS:
        result.update(sampling_status="stale", reason="最終実測から20秒超。停止・遅延・時計変更の可能性があり現在値は非表示。")
    else:
        result.update(sampling_status="measured", effective_tps=round(tps, 3),
                      current_online=None if online < 0 else online,
                      sampled_peak_online=None if peak < 0 else peak,
                      reason="同一ServerLevel::_subTick通知間隔の実測TPS（上限20）。処理時間MSPTではありません。"
                             + (" この計測起動中の5秒観測人数ピーク: " + str(peak) + "人。" if peak >= 0 else " 人数は未計測。"))
    return result
