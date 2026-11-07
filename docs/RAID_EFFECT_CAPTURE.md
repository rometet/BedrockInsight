# 襲撃関連効果の付与観測（襲撃そのものの確定統計ではない）

固定LK v0.13.0は`raid_trigger`／`raid_win`を、Playerへの特定効果付与で数える
[経路](https://github.com/LeafKnife/Stats/blob/5bb8bfb7e92c28e03bdd02d93ba7cc8b6c15bb40/src/mod/Hook/Player/PlayerAddEffect.cpp)と
[ハンドラー](https://github.com/LeafKnife/Stats/blob/5bb8bfb7e92c28e03bdd02d93ba7cc8b6c15bb40/src/mod/Stats/Handlers/PlayerCombatStatsHandler.cpp)を持ちます。
上流コード／コメント／翻訳をコピーせず、固定SDKに基づく独立実装です。

旧表の「効果付与API非公開」は訂正しました。26.60固定Actor.h:679／782には
`Actor::addEffect(MobEffectInstance const&)`／`getEffect(uint) const`がMCAPIとして存在します。
`MobEffectIds`と`EffectDuration`の公開宣言を使用し、数値オフセットや仮想スロットを推測しません。

## 正確な意味

| CUSTOMキー | 入力効果の公開enumと期間 | 日本語表示 |
| --- | --- | --- |
| minecraft:raid_trigger | RaidOmen=36、600tick | 襲撃前兆効果の付与観測 |
| minecraft:raid_win | VillageHero=29、48000tick | 村の英雄効果の付与観測 |

付与呼出し前の値をコピーし、originを**1回だけ**呼び、同じPlayerの弱参照を解決し直して
`getEffect`を比較します。UUIDのある未削除・生存Playerで、Level／次元が不変、再入れ子なし、
入力と同じ効果ID／期間／amplifierが処理後に存在し、新規または値が変化した場合だけ1件です。
amplifierは0..255。既存の同値再付与、拒否・打消し、別期間、失効・死亡・次元変更、再入れ子は欠測です。
同じ効果のリフレッシュも値が変われば観測します。上流の処理前付与要求すべてを数える意味とは異なります。
telemetryの準備・後処理・queue失敗はゲーム処理の再実行を起こさず、engine例外は保持します。
origin後に入力effectや元のActor参照を読みません。DBは既存bounded single writerへ渡します。

**襲撃の発生・参加・勝利、Playerの貢献、効果の原因、worldへの永続コミットは証明しません。**
コマンドや他Modが同じ効果を付けても条件を満たし得ます。通常の襲撃1回が何件になるかは
実BDS・実プレイ未検証です。効果件数を「実際の襲撃回数」と表示しません。

## 保存と表示

- kind=`player_interaction_result`、cause=`ll26.60.raid_effect_changed.v1`
- primary_type=CUSTOMキー、secondary_type=`effect_id:36`または`effect_id:29`、amount=1
- details: hook／phase=`post_origin_effect_change`、入力・前後の期間／amplifier、同一Player／Level、再入れ子なし
- raid_outcome_verified=false、effect_source_verified=false、durable_world_commit_verified=false

schema4／旧SQLite／MSPT履歴を維持します。厳格なnative SQLとPython validatorがsource・型・量・
phase・重複JSONキー・期間・前後差分を検査し、同じ個人CUSTOM・ランキング・GUI／API readerへ接続。
新しい別プロセス用`/stats`／認証付き`/api/v1/stats?key=minecraft:raid_trigger`（またはraid_win）で
JST日／月／全期間、Player選択、ページングに対応します。未記録はnull／未計測で、0を捏造しません。
起動は[既存のローカル限定手順](STATS_LIBRARY.md)。稼働18767への配備は行いません。

## 検証範囲

```powershell
python -W error::ResourceWarning -m unittest discover -s tests -p 'test_raid_effect.py' -v
.\scripts\verify_travel_v2.ps1 -WorldTick -FishBucket -RaidEffect
.\scripts\build_server_candidate.ps1
.\scripts\verify_stats_library.ps1
```

合成テストは新規／refresh／無変化・失敗・再入れ子・範囲・origin1回／例外保持、production
builder→StorageWriter→停止SQLite→native個人／順位／integrity、および同DB→匿名401／認証200→
実stats.jsの日本語DOMを検査します。native SQLとPythonの不正行除外も同じfixtureで照合。
この試験は実engineフック、実ブラウザ、実襲撃の受入ではありません。最新の実行証跡はSTATUS.md。

今後の通常ホスト・新規隔離BDSでは、通常襲撃、効果の拒否／再付与、コマンドによる同効果を
分けて実施し、SQLite／認証Web／ゲーム内GUI／他Mod APIまで比較する必要があります。
このドキュメントはコマンド実行・既存world変更・18767更新・セキュリティ変更の許可を拡大しません。
全9分類／77キーの受入はIN_PROGRESS、MSPTの既受入試験は繰り返しません。
