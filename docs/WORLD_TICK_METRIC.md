# total_world_time — ワールド共有tickの観測値

`minecraft:total_world_time`は個人のプレイ時間ではありません。固定v0.13.0の
[表示経路](https://github.com/LeafKnife/Stats/blob/5bb8bfb7e92c28e03bdd02d93ba7cc8b6c15bb40/src/mod/Stats/Form/Form.cpp)
で使う値は`Level::getCurrentServerTick().tickID`です。旧マトリクスの「プレイヤー別APIなし」は
この共有値を取得できない根拠にならないため訂正しました。上流のソース・コメント・翻訳は
コピーせず、既存のSQLiteと独自JSON契約で記録します。

## 捕捉と保存

固定LL26.60 `Level.h`の公開仮想メソッドと`Tick.h`の宣言をそのままC++呼出しします。
privateの`$getCurrentServerTick`や推測したvtable番号、packet、壁時計換算は使いません。
既存ServerLevelTickEventの単一Level確認済みinterval窓（5秒以上）につき1回だけ読み、
既存bounded queue／単一SQLite writerへ渡します。各tickや各プレイヤーごとに書きません。
最後の5秒未満の未保存分やキュー拒否・例外・crash分は補完しません。

- kind: `server_tick`、cause: `ll26.60.world_tick_snapshot.v1`
- actor: system、actor_id=session_id=今回のcapture run ID（プレイヤーIDではない）
- primary_type: `minecraft:total_world_time`、amount: engineのtick ID（0も実観測なら有効）
- occurred_at_ms: 実観測時刻。detailsのtick_id／observed_at_msと一致させる
- details: adapter、engine_tick_snapshot、world_shared、latest_snapshot、5000ms cadence、
  player_time=false、restart_continuity_verified=false

新schemaや既存DBの編集は不要です。tick_msptは0、online_playersは-1のままなので、
従来の`server_tick_samples`へ偽MSPTを追加しません。MSPT受入は再試験しません。

## 表示と期間

新しい別プロセス用コンパニオンの`/stats`で「ワールド共有tick（最終観測値）」を選択します。
認証APIは`/api/v1/stats?key=minecraft:total_world_time`。起動・新規ランダムトークン・
loopback限定・SSH転送は[既存手順](STATS_LIBRARY.md)を使用し、稼働18767は変更しません。
例`http://127.0.0.1:18768/stats`は自動起動しません。

| 項目 | 定義 |
| --- | --- |
| 全期間／JST日／月 | 対象期間内で最後の有効スナップショット。期間中の増分ではない |
| 複数run／再起動 | 観測UTC時刻、同時刻ならevent_idで最後を選ぶ。SUM/MAXしない |
| Player選択／人数 | 共有値なので数値は変えない。人数倍の合計を作らない |
| 順位 | プレイヤーの貢献値ではないため対象外。GUI／独自4APIの順位要求も明示拒否 |
| 未記録 | null／未計測。0へ置き換えない。実際のtick ID0とは別 |
| 古い値 | 観測から30秒超なら過去の値と表示。停止後も壁時計で進めない |
| 安全なID | Webには元run IDを出さず、プロセス内キーによるrun_keyを返す |

tick IDのサーバー再起動・別worldを跨ぐ永続連続性は未証明です。過去のworld IDを指定する
APIではなく、このDBに保存されたcaptureの最終値です。ワールド年齢・日照時刻・個人play_time・
真のTPS／MSPTへ読み替えないでください。

既存busy-tickのamount合計は「保存できた処理tick数」であり、初回欠測・部分窓・再起動を含むため
engine tick IDとは同じではありません。今回その代用投影を外しました。旧busy/interval行は削除せず、
元のテレメトリーソースとして保持します。

## 検証と未受入範囲

```powershell
.\scripts\verify_travel_v2.ps1 -WorldTick
.\scripts\verify_stats_library.ps1
```

新しいworld_tick_testsは本番builder→StorageWriter→閉鎖SQLite→native StatsReader／独自APIを
通します。再起動で大きい値から小さい値、0、後続値へ変わること、共有値が人数倍にならないこと、
壊れたJSON／数量／sourceを除外し、MSPT行が作られないことを検査します。同じC++生成DBを
認証HTTP→実際のstats.js DOMへ渡す読取も検査します。これはBDSやブラウザの実操作証明ではありません。

全LK9カテゴリ／77キー受入は引き続きIN_PROGRESS。実BDS・認証プレイヤー・ゲーム内GUI・
別Modからの呼出しは、[通常ホスト手順](PLAYER_ACTION_HOST_VALIDATION.md)に従う別試験です。
CodexSandboxOfflineでonline BDSを再試行したり、既存BDS/world/18767を変更しません。

今回の確定結果: 候補`.build/server-candidate/run-732292c357514063a25ae74401d3e4ec/`は
20ネイティブ単位・prelink/link/PE/package PASS、DLL SHA256
`313E073E2AE42D89996F7BF2CED059EE2AF0082F488B0B68536E6B08A94A68E9`。
93入力ハッシュ一致、未ステージ／未ロードです。5本のC++ fixtureと同一DBの認証HTTP／DOMは
`.build/travel-v2-tests/run-081d647e18c34eeea991bf5755055b73/`でPASS。
全Python138件・最適化inventory2件、60入力不変の証拠は
`.build/stats-view-tests/run-632ee1da2fc4425cad9f3b43a38ea052/regression.json`。
通常ホストツールの接続確認は「承認が必要／承認ポリシーnever」で拒否され、別経路へ迂回せず
停止しました。担当者の通常ホスト側で新規隔離試験が必要です。全機能・実機PASSとはしません。
