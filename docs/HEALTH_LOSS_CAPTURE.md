# HP属性の前後減少観測（候補・実機受入前）

CUSTOM `minecraft:damage_taken` / `minecraft:damage_dealt` の読取ソースを
`ll26.60.hurt_health_delta.v1` に分離しました。日本語表示は
「自身のHP属性減少観測（丸め）」「加害先のHP属性減少観測（丸め）」、単位は0.1HPです。
**全ダメージ経路・最終適用量・世界への永続コミットを証明する統計ではありません。**

既存BDS、旧SQLite、稼働viewer18767に候補を配置・再起動していません。
新候補の通常ホスト実プレイ受入は未実施。MSPTは受入済みのままで、再試験不要です。

## 取得しているもの

固定LL26.60.0 SDK `072b9ffe0e9d9e0ec846f11d294880e892fe7770` の公開
`Player::$_hurt` / `Mob::$_hurt`、`SharedAttributes::HEALTH()`、
`Actor::getAttribute()`、型付き `AttributeInstance::mCurrentValue` を使います。
整数に丸められる `getHealth()`、非公開offset、未公開アクセサ、受信パケット推計は使いません。

既存 `player_absorption.cpp` のhurtフック・再入判定・世代付き有界writer経路を共有します。
新しいhurtフックやtick書込を増やしていません。HEALTHとABSORPTIONは独立した属性・
前後値・イベントであり、混ぜて1つのダメージ合計を作りません。

採用条件は次のすべてです。

- 入口の有限な正のdamage、死亡前・未removedの対象、適切な属性インスタンス。
- 対象のweak参照を返却後に再解決し、同一Actor・Level・dimension・属性インスタンス。
  origin後は古いthisやdamage sourceの参照を再利用しません。
- `ActorHurtResult::Type::DamageProcessed`（固定SDK値2）と、存在する有限非負の返却量。
  **返却量をHP減少として採用するのではなく、結果の健全性確認にだけ使います。**
- HEALTHが実際に減少し、前後値は0..100000HP、減少が入口damage+丸め誤差1milli以下。
  NaN、Inf、負値、増加、変化なし、過大差分は欠測。
- Player→同一Mob親呼出は1回だけ許可。別対象、再帰、繰返し親呼出は外側を曖昧として除外。
  親を別行に計上しません。

前後値をそれぞれ最寄りmilliHPに丸め、差を最寄り0.1HPに丸めます。
丸め後0は行を作りません。例：20→17HPは30、20→18HPは20、微小な減少は記録されないことがあります。
致死後0も対象が同じ属性として残っていれば観測できますが、対象削除等は欠測です。
milli丸め後50milli未満の差を無理に1へ切り上げません。境界近傍はこの二段階丸めに従います。

## 帰属と不確実性

本人側はPlayerのHEALTH属性減少。加害側は公開sourceのcopied IDを使い、同じLevelで
再解決できる直接攻撃のPlayer、または区別できる子飛び道具sourceのPlayerに限定します。
不明、自己、環境、反射、爆発、commandの所有者を推測しません。
加害側Playerはorigin後も同一weak実体・未removed・生存・同じLevelであることを確認します。

HEALTHの**その呼出内での正味の減少**です。同じ呼出の回復・トーテム・他Modの変更を分解できず、
回復により観測が減る/消える場合があります。後からの復活、巻戻し、保存完了も検証しません。
HP差が存在しても「その攻撃だけの最終ダメージ」とは断定しません。

イベントは既存schema4の `player_interaction_result`、primary_typeは2つのCUSTOMキー、
secondary_typeは観測対象のエンティティ種類です。JSONには属性名・前後milli・入力/返却milli・
hook・親呼出数・同一性・帰属を保存し、常に以下を区別します。

本ソースは座標を採取しません。イベントの既定Position（0等）を実座標やdimensionの
証拠として使わず、同一dimensionの確認はnativeの連続性チェックとして解釈してください。

- `health_attribute_decrease_observed:true`
- `final_health_loss_measured:false`（最終/後続世界結果の保証ではない）
- `exclusive_causation_verified:false`
- `durable_world_commit_verified:false`

本人/加害者/HEALTH/ABSORPTIONの別行はトランザクション単位の同時admitではありません。
有界キュー満杯、無効化、観測例外により片側だけ失う可能性があり、総被害量と総加害量は
一致保証しません。originの実行回数・返却値・エンジン例外は変更しません。

## 古い通知量との互換

旧 `ll26.60.actor_hurt_event.v1` と `ll26.60.actor_hurt_real_damage.v1` は
ActorHurtEventの処理前量であり、HEALTHの減少ではありません。既存の行を削除/再解釈せず、
新しいCUSTOM個人・ランキング・Web読取には混ぜません。旧通知しかないDBは新CUSTOMでは
未計測/nullです。旧通知を保持する監査/行動表示は別の履歴として残ります。

Pythonとnative SQLは属性・型・丸め・同一性・帰属・cause・上限・重複JSONキーを検証します。
Webは日/月/全期間の同じJST境界と選択Player/種類/ランキングを利用し、表示状態も
「HP属性の前後減少を観測（最終結果・実機受入は未証明）」と明記します。
ゲーム内GUI・独立C APIは同じStatsReader投影を使いますが、実GUI操作/別Mod連携は未受入です。

## 検証・通常ホストで残ること

合成検証の再現コマンド（キャッシュ済みclang/SQLite設定後）：

```powershell
$env:PYTHONPATH='tests;.'
python -W error::ResourceWarning -m unittest discover -s tests -p test_health_loss.py -v
./scripts/verify_travel_v2.ps1 -WorldTick -FishBucket -RaidEffect -Absorption -Rest -Resistance -ShieldEvidence -HealthLoss
./scripts/verify_interval_runtime_import.ps1
./scripts/build_server_candidate.ps1
./scripts/verify_stats_library.ps1
```

fixtureからwriter→閉じたSQLite→native個人/順位/C API→認証HTTP→実stats.jsの合成DOMを照合します。
合成DOMは本物のブラウザ・ゲーム実操作の証拠ではありません。

通常ホストの**新規**隔離world/未使用portsだけで、通常攻撃/鎧/耐性/吸収/PvP/飛び道具/
環境/キャンセル/連続攻撃/致死/回復・トーテム/他Mod再入を比較してください。
入口申告量・HEALTH前後差・吸収差・永続行・日本語個人/順位・ゲーム内GUIを別々に照合し、
通常stop・closed integrity_checkまで確認が必要です。サンドボックスでonline BDSを再試行せず、
既存BDS・18767・認証/Firewallを変更しません。全96の実プレイ受入と全体IN_PROGRESSは維持します。

独立実装は既存のBedrockInsight属性観測ヘルパーと正式SDKを使用しました。
LK-Stats AGPL実装・コメント・翻訳のコピーは行っていません。
