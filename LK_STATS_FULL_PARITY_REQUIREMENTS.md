# BedrockInsight — LK-Stats v0.13.0 全機能同等性の最低要件
制定：2026-11-06（ユーザーの明示した必須スコープ）

## 最上位要件

BedrockInsightの完成条件は、**LK-Stats v0.13.0が提供する全てのユーザー向け統計と機能を最低限として備えること**である。主要イベントを数個実装しただけでは完成扱いにしない。LK-Statsより多いWeb分析・MSPT・監査・BlockPlaced等は追加機能であり、LK-Stats相当の欠落を正当化しない。

参照固定：https://github.com/LeafKnife/Stats/tree/v0.13.0
固定ソース：https://github.com/LeafKnife/Stats/blob/v0.13.0/src/mod/Stats/StatsCustom.h
README：https://github.com/LeafKnife/Stats/blob/v0.13.0/README.md
このファイルは「公開された機能名と統計キー」を網羅した**要件リスト**で、上流のソース実装・文章・翻訳をコピーするものではない。AGPL-3.0のコードをそのまま移植しない。正式26.60 SDKに従い独立実装する。

## 必須の9統計カテゴリ（全部）

- `custom`：下記77キーを個別に計測
- `mined` / `minecraft:mined`：成功したブロック採掘数を種類別・プレイヤー別に記録。PlayerDestroyBlockEventの試行数だけでは不可
- `broken` / `minecraft:broken`：道具・防具等の破損回数をアイテム種類別に記録。耐久消費やスロット減少の観測だけでは不可
- `crafted` / `minecraft:crafted`：成立したクラフト／対象となる精錬・取引等をアイテム種類・個数別に記録。未成立の転送は不可
- `used` / `minecraft:used`：使用回数をアイテム・ブロック種類別に記録。単なる試行やtrue戻り値だけを全用途の成功と混同しない
- `picked_up` / `minecraft:picked_up`：拾得個数をアイテム種類別に記録
- `dropped` / `minecraft:dropped`：投棄個数をアイテム種類別に記録
- `killed` / `minecraft:killed`：プレイヤーが倒したエンティティの種類別回数、Mob/PvP、飛び道具等の正当な帰属
- `killed_by` / `minecraft:killed_by`：プレイヤーを倒したエンティティの種類別回数、環境死等は別扱い

## CUSTOM：v0.13.0上流コードで定義された77キーを一つも忘れない

全て `minecraft:` 接頭辞つきで識別する。次の定義は上流の公開された統計識別子を列挙したもの。既存実装・候補・単体テスト・実機証明を個別に追跡する。

1. `minecraft:animals_bred`
2. `minecraft:clean_armor`
3. `minecraft:clean_banner`
4. `minecraft:open_barrel`
5. `minecraft:bell_ring`
6. `minecraft:eat_cake_slice`
7. `minecraft:fill_cauldron`
8. `minecraft:open_chest`
9. `minecraft:damage_absorbed`
10. `minecraft:damage_blocked_by_shield`
11. `minecraft:damage_dealt`
12. `minecraft:damage_dealt_absorbed`
13. `minecraft:damage_dealt_resisted`
14. `minecraft:damage_resisted`
15. `minecraft:damage_taken`
16. `minecraft:inspect_dispenser`
17. `minecraft:nautilus_one_cm`
18. `minecraft:happy_ghast_one_cm`
19. `minecraft:boat_one_cm`
20. `minecraft:aviate_one_cm`
21. `minecraft:horse_one_cm`
22. `minecraft:minecart_one_cm`
23. `minecraft:pig_one_cm`
24. `minecraft:strider_one_cm`
25. `minecraft:climb_one_cm`
26. `minecraft:crouch_one_cm`
27. `minecraft:fall_one_cm`
28. `minecraft:fly_one_cm`
29. `minecraft:sprint_one_cm`
30. `minecraft:swim_one_cm`
31. `minecraft:walk_one_cm`
32. `minecraft:walk_on_water_one_cm`
33. `minecraft:walk_under_water_one_cm`
34. `minecraft:inspect_dropper`
35. `minecraft:open_enderchest`
36. `minecraft:fish_caught`
37. `minecraft:leave_game`
38. `minecraft:inspect_hopper`
39. `minecraft:interact_with_anvil`
40. `minecraft:interact_with_beacon`
41. `minecraft:interact_with_blast_furnace`
42. `minecraft:interact_with_brewingstand`
43. `minecraft:interact_with_campfire`
44. `minecraft:interact_with_cartography_table`
45. `minecraft:interact_with_crafting_table`
46. `minecraft:interact_with_furnace`
47. `minecraft:interact_with_grindstone`
48. `minecraft:interact_with_lectern`
49. `minecraft:interact_with_loom`
50. `minecraft:interact_with_smithing_table`
51. `minecraft:interact_with_smoker`
52. `minecraft:interact_with_stonecutter`
53. `minecraft:drop`
54. `minecraft:enchant_item`
55. `minecraft:jump`
56. `minecraft:mob_kills`
57. `minecraft:play_record`
58. `minecraft:play_noteblock`
59. `minecraft:tune_noteblock`
60. `minecraft:deaths`
61. `minecraft:pot_flower`
62. `minecraft:player_kills`
63. `minecraft:raid_trigger`
64. `minecraft:raid_win`
65. `minecraft:clean_shulker_box`
66. `minecraft:open_shulker_box`
67. `minecraft:time_since_death`
68. `minecraft:time_since_rest`
69. `minecraft:sneak_time`
70. `minecraft:talked_to_villager`
71. `minecraft:target_hit`
72. `minecraft:play_time`
73. `minecraft:total_world_time`
74. `minecraft:sleep_in_bed`
75. `minecraft:traded_with_villager`
76. `minecraft:trigger_trapped_chest`
77. `minecraft:use_cauldron`

**注意：** READMEで `time_since_death` と `time_since_rest` が取り消し線表示されている一方、v0.13.0のCustomType列挙には存在する。また、`nautilus_one_cm` / `happy_ghast_one_cm` はCustomType列挙に存在するがREADMEの表にはない。上流実装が本当に有効か調査し、未実装ならその根拠を明記する。単にEnumに存在するだけで「取得可能」とは判定しない。顧客の求める全機能の基準を勝手に引き下げない。

## 統計以外のLK-Stats機能も必須

1. ゲーム内 `/stats gui [StatsType]`：個人統計のフォーム／GUI。
2. ゲーム内 `/stats rank <StatsType> [type]`：種類別、プレイヤー別ランキングGUI。
3. ゲーム内 `/stats player <playerName:string> <StatsType>`：指定プレイヤーの統計フォーム。
4. 互換相当の呼出しAPI：`getPlayerStats`、`getRankStats`、`getPlayerStatsPage`、`getRankStatsPage`。9カテゴリ・名前空間ID指定・ランキング・ページング・値型・範囲・認証・他Modとの連携契約を明確化。現存するLegacyRemoteCall/LIP互換が必要なら正版SDK依存とライセンスを調べ、動く形で提供。
5. 統計の永続保存、プレイヤー識別・再接続・リスタート引き継ぎ、各タイプ別の値と単位、古いデータの安全な保持。
6. ブロック／アイテム／エンティティ名およびCUSTOMキーの適切な日本語表示、設定・言語ファイル、未知IDの安全なフォールバック。
7. 追加のWebダッシュボードにも上記全項目を検索・表示・ランキング可能にする。JST日/月/全期間は現行BedrockInsightの追加要件として維持する。

## 最低受け入れ条件（未達ならPROJECT_STATUS: IN_PROGRESS）

- 9カテゴリとCUSTOM77キーの**個別ギャップマトリクス**を作る。各行：上流仕様・取得経路・成功/試行の意味・登録済みか・DLLコンパイル・単体検証・隔離BDSロード・実プレイヤー操作・SQLite記録・GUI/APIの表示結果・残課題。
- 統計を空のカテゴリだけ登録して「実装済み」としない。適切なソースがない場合は「未対応・要API調査」と明示し、完了扱いにしない。
- まず既存BlockMined/BlockPlaced試行を解消し、`mined`種類別成功・`used`との意味を分離。その後クラフト・移動・戦闘・各種ブロック操作を幅広く揃える。
- 毎カテゴリ最低1実行動を独立の専用BDSワールドで行い、SQLite/API/ゲーム内GUIまでのデータ整合性を確認。移動モード・特殊ブロック操作は適切な代表ケースを用いる。モックや独立Fixtureだけは実機証明にしない。
- 現行のBounded SQLite writer、旧データ・世界・既存BDS・HTTP18767・セキュリティ設定を維持し、ビルド/テストで稼働系を止めない。
- 参照コードのAGPL-3.0を遵守。現行の別ライセンスのプロジェクトへコピペしない。上流ソースの参照と独立実装の違いを記録する。
- GitHubへのpush、リリース、既存BDS展開は別途ユーザー指示があるまで行わない。

## 現在の暫定ギャップ（2026-11-06 21時台確認）

- `mined` の成功計測なし。BlockPlaced成功も追加要件として欠落。
- `crafted` はコンパイル可能なAPI経路調査止まり。
- `broken` は耐久/主所持品の一部挙動の観測まで。完全な破損確定を証明していない。
- `killed`/ `killed_by` は直接原因の一部のみ。全死亡経路を網羅できていない。
- `custom` の77項目中、多く（移動モード距離・ダメージ量・作業台/チェスト等相互作用・特殊行動）が未取得。
- ドロップ・拾得・使用・ジャンプ・スニーク・スプリント等には新しいネイティブ実装があるが、全カテゴリ実プレイヤー→SQLite→表示の新候補での検証は未達。
- ゲーム内3コマンド／互換RemoteCall4API／全面的な日本語表示について、完成証拠はない。

**ユーザーの新しい優先順位：** GrimStats/Plan/CoreProtectの大きな目標の中で、まずLK-Statsの全部が最低条件。上記の欠落を「optional future」としない。
