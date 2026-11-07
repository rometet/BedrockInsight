# 死亡元の種類・子飛び道具帰属（2026-11-07）

実装・fixture検証の説明です。実プレイ受入やLK-Stats全機能完成の証明ではありません。
AGPL実装はコピーせず、既存BedrockInsightの値イベント／単一ライターを拡張しました。
既存BDS、古いSQLite、稼働中18767、セキュリティ設定は変更しません。

## 記録と表示

| 記録 | 保存ソース | 意味 |
| --- | --- | --- |
| `player_death` | `ll26.60.player_dead_transition.v1` | 従来どおりPlayer死亡フラグ成立。`secondary_type=cause:N`。死亡原因と死亡数を維持 |
| `player_killed_by_entity` | `ll26.60.resolved_killer_dead_transition.v1` | 同じ死亡処理入口で解決できたsource actorの種類。`primary_type=namespace:type`、経路は`direct`／`child_projectile` |
| `direct_kill_observation` | `ll26.60.direct_attack_dead_transition.v1` | 従来の直接攻撃のみ。非child、原因2、source/damaging ID一致、同じLevelのPlayer |
| `direct_kill_observation` | `ll26.60.projectile_owner_dead_transition.v1` | 子飛び道具専用。原因3、異なる非0 source/damaging ID、同じLevelのPlayerへsource IDを解決 |

`minecraft:killed_by` のGUI／C APIは**新しい種類イベントだけ**を読取ります。
古い`cause:5`等、過去の混在形式、曖昧な所有者から種類を推測・移行しません。
新しい種類イベントは`player_daily_types.metric=killed_by_entities`にも保存しますが、
被害者のdeathsやkillsを増やしません。スキーマ4と古い履歴を維持します。
`minecraft:killed`とCUSTOMのmob/player killsは両方のキル保存ソースを対象にします。
Webでは従来のdirect_killsに飛び道具を混ぜず、別の`projectile_kills`とします。

## エンジン境界

公式LL26.60固定ソース`072b9ffe0e9d9e0ec846f11d294880e892fe7770`の
[ActorDamageSource](https://github.com/LiteLDev/LeviLamina/blob/072b9ffe0e9d9e0ec846f11d294880e892fe7770/src/mc/world/actor/ActorDamageSource.h)、
[ActorDamageByActorSource](https://github.com/LiteLDev/LeviLamina/blob/072b9ffe0e9d9e0ec846f11d294880e892fe7770/src/mc/world/actor/ActorDamageByActorSource.h)、
[ActorDamageByChildActorSource](https://github.com/LiteLDev/LeviLamina/blob/072b9ffe0e9d9e0ec846f11d294880e892fe7770/src/mc/world/actor/ActorDamageByChildActorSource.h)
の型付き仮想宣言を使用します。子クラスのdamaging actorと基底source actorの区別を
source所有者／飛び道具の候補帰属に使用しています。この宣言に基づく解釈は、まだ
実際の矢・トライデント・他Mod下の所有権を検証した結果ではありません。
MCNAPIコンストラクター／thunkや推測vtable・オフセットは使用しません。

入口でID・種類をコピーし、origin後はsource・古い攻撃者・古い被害者を読取りません。
WeakEntityRefで同じ未削除被害者を解決し、false→true死亡フラグ成立後だけ送信します。
再入、自分、異なるLevel、未知/消失source、block、環境原因は種類統計から除外。
帰属の取得失敗で従来の死亡観測まで捨てません。originは1回、例外は従来どおり伝播。
死亡と帰属は同じ時刻ですが独立のbounded queue投入です。片方だけ欠落し得るため、
両カウンターの厳密な対応・連続収集・ワールド保存を保証しません。

SQL／Pythonの帰属読取は同じ経路条件・型・符号付き64bit IDを検証し、重複JSONキー、
数値をboolにした偽装、巨大ID、不正種類、未来時刻等を除外します。公開JSONへUUID、
XUID、攻撃者ID、飛び道具IDを返しません。WebはtextContentのみで表示します。
他の既存統計全てに同等の厳密な検証が済んだという意味ではありません。

## 見る方法と受入境界

新規コンパニオンを利用者が別の空きloopbackポートで起動した場合だけ、`/actions`に
「死亡元エンティティ」「子飛び道具」の一覧と日／月／全期間ランキングを表示します。
起動認証／SSH転送は[ANALYTICS_DASHBOARD.md](ANALYTICS_DASHBOARD.md)参照。
18767を更新・再起動しないでください。今回常駐viewerやBDSは起動していません。
未記録はnull／未計測で、環境死は原因一覧にだけ残します。

fixtureは、実際のStorageWriter→SQLite→独立StatsReader、閉じたDBのintegrity_check、
種類別SQL／4関数API、JST境界・選択・順位・ページ、認証HTTP401/200→日本語DOMを確認。
DLLの公式ヘッダーcompile/linkと、BDSでの実プレイ証明は別ゲートです。
新DLLの実BDSロード・直接攻撃・射撃・PvP・死亡・GUI・他Mod呼出しはnormal-host担当で
新規world／未使用portsを使って検証する必要があります。既存worldは再利用しません。

爆発、炎の遅延、反射、メイス、消失所有者、全死亡経路、全装備／精錬／取引、
全77 CUSTOM等は完全同等性未達です。PROJECT_STATUSはIN_PROGRESSのままです。
