# 移動距離 v2 — ネイティブ捕捉と読み取りの定義

これは全LK互換の完成宣言ではありません。候補DLLのリンク、合成fixture、実プレイヤーの
操作→SQLite→GUI/API受入は別です。新しい実行動受入は未実施で、`PROJECT_STATUS: IN_PROGRESS`。
既存BDS・world・SQLite・viewer18767へは配備しません。MSPTの既受入は再試験しません。

## 修正した問題

- 旧車両分類は型名の部分一致で、piglinやMod側の似た名前までバニラ騎乗へ入る余地がありました。
  v2は明示した名前空間付きIDだけ。未知の乗り物をwalk/pig等へ置き換えません。
- 旧実装の飛行・泳ぎ等は水平XZだけでした。v2はこれらと騎乗・水系に上下を含めます。
- 泳ぎと水系距離を同時加算した旧v1に対し、v2の水系モードは排他的です。
- 毎tick丸めで失われたり過大になった小数cmは、窓内合計後の切捨てに変更しました。
- 移動中の次元、乗り物のID、生存状態、時刻・大きな座標差分を跨ぐ基準位置を破棄します。
- 終了時に古いlast_flushや後の切断時刻へ移すのではなく、実際の最終観測時刻で保存します。
- 登攀は固定26.60 `Actor.h`の`MCAPI bool onClimbableBlock() const`を使用します。
  旧ブロック名表による推測はv2の捕捉には使いません。ビルドにはこの正確な公開シンボルだけを追加。

## 方式と限界

既存の`ServerLevelTickEvent`／対応Level確認を使い、同じWeakEntityRefを解決できたPlayerの
サーバー座標とサーバーフラグを読みます。クライアントpacketの位置差分は取り込みません。
最大256人、各人18固定スロットの窓で集計し、通常5秒ごとだけ既存のboundedキューへ渡します。
ゲームthreadでSQLiteへ書かず、StorageWriterの単一workerとv4履歴を再利用します。
窓内処理で動的なモードmap/stringを毎tick作る方式は撤去しました。

| モード | v2の距離 |
| --- | --- |
| walk / sprint / crouch | XZ平面 |
| fall | 下降Yのみ |
| climb | 上昇Yのみ＋公式登攀predicate |
| swim / fly / aviate | XYZの長さ |
| boat / minecart / pig / strider / horse / nautilus / happy_ghast | XYZの長さ、同じ乗り物IDの連続観測のみ |
| walk_on_water / walk_under_water | 非swimmingの水内状態をunder-liquidで分離、XYZの長さ |

選択優先は乗車、滑空、飛行、水系、登攀、非接地の下降、地上フラグです。
水中でswimmingならswimだけで、同じ観測を別の水系へも加算しません。
`isUnderLiquid(Water)`という公式条件の観測であり、未知の液体や全移動経路への一般化はしません。

以下では前窓を確定して基準位置をリセットし、跨いだ距離を加算しません:

- 次元／乗車状態／乗り物ID／生存状態の切替、または失効したPlayer参照。
- steady時刻の逆転／重複、1秒超の観測gap、非有限位置、1観測10block超の3D差分。
- UTC時刻の逆転、JST日境界。境界を跨ぐ1差分は捨てる保守的な下限です。

10block以下の短距離teleportなどを全て見分けたという意味ではありません。
長いlagやjoin直後・死亡・次元移動・mount切替では正しい距離も欠測し得ます。
小数の切捨ては各モード／窓で1cm未満の欠測を残します。全移動距離や完全LK数値互換は未証明です。
queue拒否／callback例外／crash後の未保存分を補完したり、欠測を0と断定しません。

## v1履歴とv2 provenance

`event_kind=player_travel_distance`は維持し、causeを`ll26.60.poll_position_delta.v2`に分離します。
v1を削除・移行・再解釈せず、旧定義の履歴として読めます。旧v1の水平計算、tick丸め、
水系モード重複は遡って直せません。総合travel_cmは各モード観測の合計であり、v1を含む
場合に重複のない一意の道のりとは解釈しないでください。

v2のamountは切捨て済みcm、occurred_at_msは該当モードのlast_msです。
detailsにはadapter/phase/steady clock、mode、distance_cm、first_ms/last_ms、dimension、
sample_count、flush_interval_ms5000、max_gap_us1000000、max_step_cm1000、
precision=`floor_window_cm`、period_assignment=`last_observation`、world_movement_verified=falseを記録します。
first/lastは同じJST日内です。world_movement_verified=falseは完全経路の検証済みと誤読させないためです。
sample_countは正の位置差分の観測数で、実tick総数やpacket数ではありません。

日本語Web `/actions`・`/stats`・認証APIと、ネイティブStatsReaderの個人／ランキングに接続。
v2は型・範囲・source・JSON・重複キー・時刻・数量・mode・日境界を照合し、不正行は除外します。
ネイティブGUIと独自4呼出しAPIも同じStatsReader経路を使用しますが、実クライアント操作や
他Modとの実呼出しを受け入れたことにはなりません。

## 再現可能な合成テスト

既存キャッシュを指定した環境で:

```powershell
.\scripts\verify_travel_v2.ps1
.\scripts\verify_stats_library.ps1
```

前者は新規project-local fixture内でC++を実コンパイル／リンクし、窓・分類・境界を検査、
**本番と同じイベントbuilder→StorageWriter→SQLite→StatsReader／順位→integrity_check**を確認します。
既存parity／StatsReader／RemoteAPIテストも実行し、ソースの前後ハッシュを照合します。
これはnative adapterのゲーム内実行を模擬したPASSではありません。

`test_travel_v2.py`は17モード、JST日/月/全期間、v1共存、不正行についてPythonと
ネイティブと同じSQL predicateを照合します。実際のfixture HTTP応答を
`travel_ui_tests.cjs`へ渡して両方の日本語JSのcm・種類・順位・source・定義・消去を確認します。
`http_readback(Path(<停止したC++ fixture>))`でC++生成DBからのHTTP／DOM照合も可能です。
証拠のパス・実コマンド・DLLリンクの成否はSTATUS.mdの本追補を参照してください。

2026-11-07の検証結果（いずれも実プレイとは別）:

| 検査 | 結果と証拠 |
| --- | --- |
| 実ネイティブDLL |20単位compile＋prelink/link/PE/package PASS、`.build/server-candidate/run-38e460add06742628c8ff265945eaebc/`。DLL SHA256 `993F3858F64F164F90E2D819DEF70493516ECF0CE7BF88BE30011B6792ACF1F4`。91入力ハッシュ一致。未ステージ/未ロード |
| C++モデル・writer・native reader |4バイナリPASS、`.build/travel-v2-tests/run-1845c7962d3e421a9dcfe076abc39025/fixture-evidence.json` |
| C++生成DB→認証HTTP→日本語JS DOM | 同じfixture rootの`http-dom-readback.log`、匿名401/認証200、Alice24cm/Bob10cm、integrity=ok。ブラウザ描画やゲーム内GUIではない |
| 全Python回帰 |133件PASS、最適化inventory2件PASS、57入力不変。`.build/stats-view-tests/run-bb4f6a25dad74ca8a9644bdd5cd5e796/regression.json` |

17キー全ての読取と期間/順位はPythonの合成行、モード分類はC++のフラグfixtureで検査します。
本番builder→native writer→HTTPを通した停止DBの具体例はwalkです。他モードの実ゲーム行が
このDBに入っているという意味ではなく、全17モードの実操作ゲートは未検証のままです。

## 次の実機受入（ここでは未実施）

[通常ホスト手順](PLAYER_ACTION_HOST_VALIDATION.md)に従い、未使用world／ポートの別環境で
ユーザーが実操作します。Codex offline sandboxのonline BDS再試行、既存world再利用、
認証・firewall変更、既存BDS／18767の停止は禁止です。

最低限、歩行・sprint・sneak、上昇を含む飛行／泳ぎ、登攀／下降、各対応騎乗を確認し、
死亡・portal・乗り換え・大きなteleportで跨ぎ距離が入らないことを記録します。
新sourceのSQLite行と認証API・日本語画面・ゲーム内個人／順位を同じJST期間で照合し、
最後の5秒未満の移動も正常stop後に正しい観測時刻で残ること、closed DB integrity=okを確認します。
各モードの手動操作メモと独立した期待範囲がない場合、合成テストだけで実プレイPASSへ昇格しません。

## 参照と独立実装

機能・単位の比較には[固定v0.13.0の移動ハンドラ](https://github.com/LeafKnife/Stats/blob/5bb8bfb7e92c28e03bdd02d93ba7cc8b6c15bb40/src/mod/Stats/Handlers/PlayerMovementStatsHandler.cpp)
を参照しました。上流はpacket経路も使いますが、本実装はサーバー座標の独自固定窓・
独自JSON契約・既存SQLiteを使用し、そのソース／コメント／翻訳をコピーしません。
正確なAPI宣言はプロジェクトの固定LL26.60ヘッダーと候補ビルドで検査します。
