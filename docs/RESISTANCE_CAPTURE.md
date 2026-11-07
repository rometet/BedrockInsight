# 耐性量（参照式・丸め）— 実軽減量ではない候補ソース

対象は `minecraft:damage_resisted`（自身）と
`minecraft:damage_dealt_resisted`（解決した加害先）。日本語Web・共通native読取に接続し、
単位は1/10HPです。**最終HP差・実際の耐性適用量の実測ではありません。**
全77項目／全機能の受入は未完了、稼働BDSと18767には配備していません。

## 参照・独立実装

固定LLヘッダーcommit `072b9ffe0e9d9e0ec846f11d294880e892fe7770` の
Mob.hにある公開const MCAPI `Mob::$getDamageAfterEnchantReduction` と、
Player/Mobの公開 `$_hurt`、Actor::getEffect、公開damage-source仮想関数を使用します。
推測アドレス・vtable番号・packet傍受・非公開thunkは使いません。

技術上の比較対象は[固定LK-StatsのMobDamageReduction](https://github.com/LeafKnife/Stats/blob/5bb8bfb7e92c28e03bdd02d93ba7cc8b6c15bb40/src/mod/Hook/MobDamageReduction.cpp)。
同版はエンチャント後の値と耐性レベルによる式を使用しており、HP差の直接観測ではありません。
これは公開ソースの挙動参照であり、当プロジェクト上での実行証拠ではありません。
AGPLコード・コメント・翻訳はコピーせず、独立した整数固定小数点・相関ガード・SQLite契約を実装しました。

## 実装上の成立条件

外側のPlayer/Mob `$_hurt` フレームと、その内部の**ただ一回**のエンチャント軽減処理を
同じActor/sourceオブジェクトへ対応付けます。外側Player→同じMob親1回は二重に数えません。
複数stage、stage内の再入hurt、同じhookへの再入、別対象・別source・別Level/次元は曖昧として除外。
独立した計算用getter呼出しや、ActorHurtEventの事前通知だけでは記録しません。

stage入口では公開 `isReducedByResistanceEffect()` がtrueで、耐性効果ID11を確認。
stage前後と外側完了時に同じ弱参照Actor、Level/次元、効果amplifier/durationを照合します。
amplifierは0..255、durationは正のint32だけ。無期限表現を推測しないため期間<=0は除外します。
外側の結果は `DamageProcessed`（enum2）かつoptionalの有限・非負値が必要。
NotHurt/EffectsOnly、未返却、NaN/inf、途中の効果変化・消失は採用しません。
完全耐性がNotHurtへ戻る実engine経路は、取得できない可能性があります。実機で未確認です。

## 数値の意味

| 値 | 取得点 | 解釈 |
| --- | --- | --- |
| hurt_input_milli | 外側hurt入口 | 入力の申告量、HP損失ではない |
| stage_input_milli | 相関したgetter入口 | 軽減計算への入力、最終量ではない |
| basis_milli | getter正常終了 | エンチャント後として返された値、耐性適用後の実HP差ではない |
| returned_milli | 外側hurt正常終了 | 戻り値の検証用、HP損失・軽減量へ代入しない |
| amount | basisとamplifierから独立計算 | 参照式の耐性量、1/10HP（丸め） |

float値を最寄り0.001HPへ丸め、`level=min(amplifier+1,5)` とします。
`amount=(basis_milli*level+250)/500` の整数除算で最寄り0.1HPへ丸めます。
レベルI..Vの20..100%という**参照式**で、engineの全軽減段階を観測した主張ではありません。
値は0..100000HPに制限し、stage入力<=hurt入力+1milli、basis<=stage入力+1milliを照合。
0単位へ丸まった量は加算しません。増幅・独自倍率などこの契約に適合しない経路は欠測です。

被害者がPlayerなら本人へ、加害側は入口sourceから同じLevelへ解決した直接／子飛び道具の
Playerだけへ帰属。完了後も加害者の弱参照・生存・Levelを再照合します。
環境・自己・反射・爆発・不明所有者は加害側から除外。2行のqueue投入は原子的なpairではなく、
drop等で片方だけ残る場合があります。未記録を0と断定しません。

## 保存・表示・検証境界

cause `ll26.60.processed_hurt_resistance_formula.v1`、既存events/schema4、有界単一writer。
game threadでSQLite処理をしません。originの回数・戻り値・例外は観測失敗でも変更しません。
旧damage_taken/dealtの事前通知、吸収属性差、休息後snapshot、MSPTは別ソースのまま保持します。
JSONに `resistance_applied_measured=false`、`final_health_loss_measured=false`、
`durable_world_commit_verified=false` を必須化し、native SQLとPythonの両方で再計算・検証します。
不正JSON/重複キー/型違い/量不一致/未来行等を除外。日/月/全期間はUTC保存時刻をJSTで選択。
個人・安定tie順位・ページング、opaque player key、loopback認証・read-onlyを維持。

WebのCUSTOMで「自身の耐性量（式・丸め）」「加害先の耐性量（式・丸め）」を選びます。
状態欄も「計算値（実軽減量・実機受入は未証明）」です。記録なしはnull／未計測。
既存18767は更新しません。別ポートの新コンパニオン手順は[ダッシュボード](ANALYTICS_DASHBOARD.md)。

合成テストはI..V/高amplifier上限、端数/範囲、stage/再入相関、origin例外、production builder→
writer→SQLite→native個人/順位、認証HTTP→実stats.jsの合成DOM、JST、誤帰属・不正行の除外を検証。
コンパイル/リンクや合成結果はBDS/player/browserの証拠ではありません。

通常ホストの**新root・world・未使用port**で、有限耐性I/II/V、無耐性、直接攻撃、所有飛び道具、
環境、PvP、拒否/無敵、吸収/防具併用、効果消失、死亡を実操作し、実際のstage呼出し回数・戻り型と
SQLite値、3コマンドの実GUI、4互換API、認証Webを照合する必要があります。
normal-stop0・閉じたDB integrityも必要。未観測をPASSにしません。
