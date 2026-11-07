# 睡眠観測後のPlayer tick（候補・実プレイ未受入）

`minecraft:time_since_rest` は、実際に睡眠状態を観測した後の
`ServerPlayer::$normalTick` **完了呼出し数の最終保存値**です。
実時間、Level tick数、50ms換算、熟睡・夜明け完了の証明ではありません。
固定26.60 ServerPlayer.h の公開MCAPI宣言を使った独立実装で、
上流AGPLコード・コメント・翻訳は取り込んでいません。

## 取得と欠測

- 最初は不明。正常にoriginが完了した同じ弱参照Playerの `isSleeping()` がtrueなら0へリセット。
- その後の非睡眠時の完了呼出しを1ずつ加算。睡眠中は0を維持。
- 新しいactivation、再接続通知、Player/Level交換、30秒超の観測途絶、時計逆行、曖昧な入れ子は不明へ戻す。
- 不明から睡眠を観測するまでは0を値として表示しない。真の既知0と区別する。
- 完了後は弱参照を再解決し、破棄されたthisを読まない。死亡・removedのPlayerは観測しない。
- 初回と5秒以上の間隔で既存の有界単一writerへ投入。tickごとのSQLiteアクセスなし。
  256エントリ上限、休眠レコードを必要時に整理。異なる実行threadを検出したactivationは取得停止。
- 例外・再入・観測失敗でもoriginは1回だけ実行し、その例外を変更しない。
  disable時の最終端数は補完しない。queue dropもあり得るので完全収集は主張しない。

causeは `ll26.60.player_rest_ticks.v1`。既存events/schema4を使い、履歴は削除しません。
`baseline_known=false,amount=0` の不明マーカーも保存し、古い既知カウンタの再表示を防ぎます。
再起動後に過去値を現在値へ復元することはせず、次の取得は不明から開始します。
接続/切断時のmemory reset自体はDBマーカーではありません。次の完了tickまで、過去の保存観測値が
残ります。画面はオンライン・現在値ではなく**最終保存値**と明示します。

## SQLite・GUI・API・Web

native StatsReader（共通GUI/API読取）は、個人の最後の有効行を日時/event_id降順で選択。
Web `/stats` → CUSTOM →「睡眠観測後のPlayer tick（最終保存値）」も同じ契約を検証します。
日/月/全期間は**観測行を選ぶ期間**であり、期間先頭へカウンタを切り詰めません。
個人別に最新の有効行を選んでから不明を除外。SUM/MAXで全スナップショットを足しません。
同時刻の行はevent_idで決定。既知0は表示しますが、ランキングは正の既知値だけです。
「全員」は各個人の最新値の合計で、連続睡眠なしプレイヤーの累計経過時間ではありません。
停止後・古い日を選択中でも壁時計で進めません。観測日時は別表示します。

不正JSON、重複キー、型違い、時刻不一致、別ソース、未来行、不明なのに正の値などは除外。
不正行は信頼できる不明マーカーへも昇格しません。未記録・不明はnull／未計測。
UUIDをWeb表示せず、既存の認証・opaque player key・ループバック限定読取を維持します。
既存18767は更新していません。候補を確認する場合は
[companionの新規起動手順](ANALYTICS_DASHBOARD.md)で未使用の別loopbackポートを使います。

## 受入境界

合成テストは完了tick、睡眠reset、初期/途絶不明、overflow、origin例外、実builder→writer→SQLite、
個人/順位、JST期間境界、認証HTTP→実stats.jsの合成DOM、既知0/不明を検証します。
これは実BDS/player/browserの証拠ではありません。
実際のServerPlayer tick dispatch頻度、就寝/起床/死亡/切断/再接続/再起動と保存値、
3コマンドの実GUI、4関数の実互換呼出しは新しい通常ホスト隔離BDSで検証が必要です。
単なるDLLロードや空サーバーsmokeで受入にしません。全機能同等性は未達です。

通常ホストの担当者が、既存サーバーではない**新しい**隔離root・world・未使用portで行う確認:

| 操作 | SQLiteと認証Webで照合する期待値 |
| --- | --- |
| 一度も就寝せず参加して5秒以上待つ | 最新行baseline_known=false、表示null。過去の同UUID値を現在値として復元しない |
| ベッドで睡眠状態になり5秒以上維持 | 最新行baseline_known=true、amount=0。Web個人欄0 tick、順位には出ない |
| 起床して操作を継続 | 保存間隔ごとに実完了Player tick数が増える。全snapshotのSUMではない |
| 再び就寝 | 前の大きな値ではなく最新0を表示。MAXにしない |
| 切断・再接続して未就寝 | 次の完了tick以後は最新不明行、以前の既知値を表示しない |
| 正常stop・新rootで再開/同じDBを明示した別試験 | 旧履歴は残るが新観測は不明。offline時間/旧counterを足さない |
| 2人で各々就寝/起床、日/月切替と同値順位 | 個人別最新・JST行選択・安定tie順・既知0除外をSQL/API/Web/実GUIで一致確認 |

各操作の時刻、実行中DLLハッシュ、対応event_id、認証JSON、実GUI/ブラウザー表示、
normal-stop exit0、閉じたDBのintegrity_checkを証拠化します。推測PASSは付けません。
この手順は通常ホスト用の受入依頼で、Codex sandboxからのオンラインBDS再試行や
既存BDS/18767の再起動を許可するものではありません。
