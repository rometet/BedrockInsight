# LK-Stats v0.13.0 完全同等性 ギャップマトリクス

## Web追補：全選択肢と限定読取（受入ゲートは昇格しない）

新しい別プロセス用 `/stats` に9分類／77キーのセレクターと、日本語・種類別明細・
期間／Player／種類検索・独立ページ／ランキングを接続しました。
[Web読取の正確な定義](STATS_LIBRARY.md)。CUSTOMの27限定読取以外はnullで、
処理前通知を確定値へ変換せず、ラベルを収集証明にしません。
これらのfixture／HTTP／DOMテストは本表の各キーのBDS実操作・SQLite捕捉・
ゲーム内GUI／他Mod API受入を満たしません。96行accepted=falseを維持します。
既存18767・BDSへの配備なし。全機能実装・実プレイ検証が引き続き必要です。

## 最新追補：種類別死亡元・飛び道具候補とページング

`killed_by`を環境原因コードから分離し、新しいsource actor種類観測に接続しました。
直接攻撃と子飛び道具のキルは別保存ソースで読み、Webも別指標です。
[定義と未検証範囲](KILLED_BY_ATTRIBUTION.md)。個人GUI／APIの200種類打ち切りは
撤去し、全件数／合計とページを検証しています。旧窓の記述は履歴です。
これらも実BDS・実操作・保存・GUI・他Modの受入は未検証のため、以下の宣言表や
全96行のacceptedをPASSへ昇格しません。全9分類／77キーが必須のままです。

## 2026-11-07：受け入れ状態の訂正と個別ゲート

**下の過去の `COMPLETED_CAPTURE` は全機能受け入れではありません。** 新候補の
全9分類・CUSTOM77キー・実プレイヤー→SQLite→ゲーム内GUI/APIは未受け入れです。
列挙・日本語ラベル・コンパイル・空BDS起動だけで各キーを取得可能とは判定しません。
下の集計数と経路には古い宣言も残り、APIが絶対に存在しないという独立監査結果ではありません。

`python scripts/audit_lk_parity.py` は要件・カタログ・本表の全77キー一致を検査し、
9分類/77キー/3コマンド/4API/補助3の**96行それぞれ**に登録・コンパイル・単体・
BDSロード・実操作・SQLite・GUI・APIのゲートを出力します。最新出力場所はSTATUS.md。
証拠を対応付けていないゲートは未検証のまま、全行accepted=falseです。
これは試験を代行するものでも、全キーの動作検証が済んだという結果でもありません。

読取修正：処理前相互作用をopen/eat/play等の確定CUSTOMへ加算しません。
攻撃回数やActorHurtEventの処理前量も確定damage_taken/dealtから除外しました。
これらの元の観測イベントは削除せず、Webの限定的な観測と実績を混同しません。
`play_time` と `time_since_death` は**完結セッションのみの下限・50ms換算tick**に限定。
重複・入れ子・連続重複を統合し、未終了、異常、未来終了、オフライン時間を除外します。
真のオンラインtickカウンターとの完全互換性は未証明です。CUSTOMの合計はnullで、
異なる単位の総合得点を作りません。

ゲーム内フォームに前後ページとCUSTOM全77キーの選択を追加しましたが、
実クライアント操作は未検証、個人明細は現在200種類の窓までです。
独自C ABIはLegacyRemoteCallワイヤ互換とは未証明です。`killed_by`の原因コードは
倒したエンティティ種類を代替せず、broken/craft/移動等の残課題も必須のままです。

更新: 2026-11-07 JST / 対象: BedrockInsight native capture
固定 upstream: LeafKnife/Stats v0.13.0（AGPL-3.0・参照のみ、コード/翻訳のコピーなし）

## 状態の定義

| 状態 | 意味 |
| --- | --- |
| COMPLETED_CAPTURE | 既存の実装・単体・隔離BDSロード・SQLiteまで既に検証済み（本表の対象外） |
| UNIT_ONLY | 既存capture + C++単体/Python fixture は通過。実プレイヤー→SQLiteの新候補検証は未 |
| CODE_ONLY | 今回の新規アダプタが pinned 26.60 ヘッダでコンパイル通過。実プレイヤー/BDS実機は未検証 |
| PARTIAL | 一部経路のみ。意味の限定を明記 |
| MISSING | 正当なフック未確定。**実装済み扱いにしない** |
| BLOCKED | 公式API/権限/ライセンス上、現時点で取得不可 |

全項目に共通: **実機（実プレイヤー操作→SQLite→UI/API）は未検証**。`PROJECT_STATUS: IN_PROGRESS`。

## 1. 9統計カテゴリ

| カテゴリ | 取得経路（実装） | 試行/成功の区別 | 状態 | 残課題 |
| --- | --- | --- | --- | --- |
| `minecraft:custom` | 移動/確定相互作用/実ダメージ/睡眠成立/スニーク時間/ジャンプ/直接キル/死亡/退出/取引成立/play_time | 観測・遷移・実時間・エンジン成功応答のみ | PARTIAL | 下記77キーの未対応分 |
| `minecraft:mined` | `GameMode::$destroyBlock` 同座標状態変化 | 事前試行と別。`block_mined_result` | COMPLETED_CAPTURE（実プレイヤーは未） | クリエイティブ等別経路 |
| `minecraft:broken` | `ItemStackBase::hurtAndBreak` + `Item::isDamageable` + `maxDamage`照合 + 同一スロット1→0 | 耐久閾値到達と消失の同時成立のみ | CODE_ONLY | 防具・副手・全破損経路は対象外 |
| `minecraft:crafted` | `CraftHandlerBase::postRequest(true)` + `mCraftResults` | 失敗要求は除外 | CODE_ONLY | インベントリ最終コミット・NBT未検証 |
| `minecraft:used` | `GameMode::$useItem(true)` + 所持差分 | true応答と効果完了を区別 | COMPLETED_CAPTURE（意味限定） | 効果完了は未計測 |
| `minecraft:picked_up` | `Player::take` 前後差分 | 入口個数ではない | COMPLETED_CAPTURE | 全拾得・容器は対象外 |
| `minecraft:dropped` | `Player::$drop(true)` 入口スタック | true戻り値のみ | COMPLETED_CAPTURE | 永続化・生成物未検証 |
| `minecraft:killed` | `Mob::$die` 直接攻撃／子飛び道具所有者の死亡フラグ成立 | 通知/試行と別 | COMPLETED_CAPTURE（意味限定） | 環境・間接原因は欠測 |
| `minecraft:killed_by` | `Player::$die`の同一weak死亡成立＋解決済みsource種類 | 原因コードと別、新sourceのみ | CODE_ONLY | 実BDS・直接/射撃/PvP・UI受入未完 |

## 2. CUSTOM 77キー

凡例: 距離=`ServerLevelTickEvent` 上の座標差分ポーリング（`ll26.60.poll_position_delta.v1`、checkMovementStats は pinned prelink 非公開のため不使用）、
相互作用=`GameMode::$useItemOn` の戻り値 `InteractionResult::mSuccess` を処理後に確認し、最初のイベントのみ確定として計数（`ll26.60.gamemode_use_item_on_success.v1`）、
ダメージ=`ActorHurtEvent` の実量（1/10単位、`ll26.60.actor_hurt_event.v1` / `ll26.60.actor_hurt_real_damage.v1`）、
取引=`ItemStackRequestActionHandler::_handleTransfer` の `ItemStackNetResult::Success` と取引結果プレビュー→プレイヤー所持品転送（`ll26.60.trade_result_transfer_success.v1`）。
処理前通知 `ll26.60.player_interact_block_event.v1` は保存のみで、どのCUSTOM値にも加算しません。

| # | key | 実装経路 | 状態 |
| --- | --- | --- | --- |
| 1 | minecraft:animals_bred | `BreedableComponent::mate` 返却offspring非空＋love cause解決 | CODE_ONLY |
| 2 | minecraft:clean_armor | `GameMode::$useItemOn` 成功応答(mSuccess)＋最初のイベントのみ | CODE_ONLY |
| 3 | minecraft:clean_banner | `GameMode::$useItemOn` 成功応答(mSuccess)＋最初のイベントのみ | CODE_ONLY |
| 4 | minecraft:open_barrel | `GameMode::$useItemOn` 成功応答(mSuccess)＋最初のイベントのみ | CODE_ONLY |
| 5 | minecraft:bell_ring | `GameMode::$useItemOn` 成功応答(mSuccess)＋最初のイベントのみ | CODE_ONLY |
| 6 | minecraft:eat_cake_slice | `GameMode::$useItemOn` 成功応答(mSuccess)＋最初のイベントのみ | CODE_ONLY |
| 7 | minecraft:fill_cauldron | `GameMode::$useItemOn` 成功応答(mSuccess)＋最初のイベントのみ | CODE_ONLY |
| 8 | minecraft:open_chest | `GameMode::$useItemOn` 成功応答(mSuccess)＋最初のイベントのみ | CODE_ONLY |
| 9 | minecraft:damage_absorbed | BLOCKED: SharedAttributes::ABSORPTION は MCNAPI、吸収量のイベント値なし | BLOCKED |
| 10 | minecraft:damage_blocked_by_shield | BLOCKED: Player::$_hurt/_blockUsingShield は LL/prelink 非公開 | BLOCKED |
| 11 | minecraft:damage_dealt | `ActorHurtEvent` 実ダメージ(1/10)＋加害ActorUniqueID→追跡Player一致 | CODE_ONLY |
| 12 | minecraft:damage_dealt_absorbed | BLOCKED: 吸収後内訳のAPIなし | BLOCKED |
| 13 | minecraft:damage_dealt_resisted | BLOCKED: 耐性後内訳のAPIなし | BLOCKED |
| 14 | minecraft:damage_resisted | BLOCKED: 耐性後内訳のAPIなし | BLOCKED |
| 15 | minecraft:damage_taken | `ActorHurtEvent` 実ダメージ(1/10)、cause/entity種別を併記 | CODE_ONLY |
| 16 | minecraft:inspect_dispenser | `GameMode::$useItemOn` 成功応答(mSuccess)＋最初のイベントのみ | CODE_ONLY |
| 17 | minecraft:nautilus_one_cm | 距離（`ll26.60.poll_position_delta.v1`、モード別XZ/上下成分） | CODE_ONLY |
| 18 | minecraft:happy_ghast_one_cm | 距離（`ll26.60.poll_position_delta.v1`、モード別XZ/上下成分） | CODE_ONLY |
| 19 | minecraft:boat_one_cm | 距離（`ll26.60.poll_position_delta.v1`、モード別XZ/上下成分） | CODE_ONLY |
| 20 | minecraft:aviate_one_cm | 距離（`ll26.60.poll_position_delta.v1`、モード別XZ/上下成分） | CODE_ONLY |
| 21 | minecraft:horse_one_cm | 距離（`ll26.60.poll_position_delta.v1`、モード別XZ/上下成分） | CODE_ONLY |
| 22 | minecraft:minecart_one_cm | 距離（`ll26.60.poll_position_delta.v1`、モード別XZ/上下成分） | CODE_ONLY |
| 23 | minecraft:pig_one_cm | 距離（`ll26.60.poll_position_delta.v1`、モード別XZ/上下成分） | CODE_ONLY |
| 24 | minecraft:strider_one_cm | 距離（`ll26.60.poll_position_delta.v1`、モード別XZ/上下成分） | CODE_ONLY |
| 25 | minecraft:climb_one_cm | なし（登攀フラグを MovementFlags に持たず、classify_travel は Climb を返さない） | MISSING |
| 26 | minecraft:crouch_one_cm | 距離（`ll26.60.poll_position_delta.v1`、モード別XZ/上下成分） | CODE_ONLY |
| 27 | minecraft:fall_one_cm | 距離（`ll26.60.poll_position_delta.v1`、モード別XZ/上下成分） | CODE_ONLY |
| 28 | minecraft:fly_one_cm | 距離（`ll26.60.poll_position_delta.v1`、モード別XZ/上下成分） | CODE_ONLY |
| 29 | minecraft:sprint_one_cm | 距離（`ll26.60.poll_position_delta.v1`、モード別XZ/上下成分） | CODE_ONLY |
| 30 | minecraft:swim_one_cm | 距離（`ll26.60.poll_position_delta.v1`、モード別XZ/上下成分） | CODE_ONLY |
| 31 | minecraft:walk_one_cm | 距離（`ll26.60.poll_position_delta.v1`、モード別XZ/上下成分） | CODE_ONLY |
| 32 | minecraft:walk_on_water_one_cm | 距離（in_water かつ非 `isUnderLiquid(Water)`、水平成分） | CODE_ONLY |
| 33 | minecraft:walk_under_water_one_cm | 距離（in_water かつ `isUnderLiquid(Water)`、水平成分） | CODE_ONLY |
| 34 | minecraft:inspect_dropper | `GameMode::$useItemOn` 成功応答(mSuccess)＋最初のイベントのみ | CODE_ONLY |
| 35 | minecraft:open_enderchest | `GameMode::$useItemOn` 成功応答(mSuccess)＋最初のイベントのみ | CODE_ONLY |
| 36 | minecraft:fish_caught | BLOCKED: BucketItem::$_useOn 非公開、上流経路も釣り竿でない可能性 | BLOCKED |
| 37 | minecraft:leave_game | 既存 `session_end` → custom集計 | UNIT_ONLY |
| 38 | minecraft:inspect_hopper | `GameMode::$useItemOn` 成功応答(mSuccess)＋最初のイベントのみ | CODE_ONLY |
| 39 | minecraft:interact_with_anvil | `GameMode::$useItemOn` 成功応答(mSuccess)＋最初のイベントのみ | CODE_ONLY |
| 40 | minecraft:interact_with_beacon | `GameMode::$useItemOn` 成功応答(mSuccess)＋最初のイベントのみ | CODE_ONLY |
| 41 | minecraft:interact_with_blast_furnace | `GameMode::$useItemOn` 成功応答(mSuccess)＋最初のイベントのみ | CODE_ONLY |
| 42 | minecraft:interact_with_brewingstand | `GameMode::$useItemOn` 成功応答(mSuccess)＋最初のイベントのみ | CODE_ONLY |
| 43 | minecraft:interact_with_campfire | `GameMode::$useItemOn` 成功応答(mSuccess)＋最初のイベントのみ | CODE_ONLY |
| 44 | minecraft:interact_with_cartography_table | `GameMode::$useItemOn` 成功応答(mSuccess)＋最初のイベントのみ | CODE_ONLY |
| 45 | minecraft:interact_with_crafting_table | `GameMode::$useItemOn` 成功応答(mSuccess)＋最初のイベントのみ | CODE_ONLY |
| 46 | minecraft:interact_with_furnace | `GameMode::$useItemOn` 成功応答(mSuccess)＋最初のイベントのみ | CODE_ONLY |
| 47 | minecraft:interact_with_grindstone | `GameMode::$useItemOn` 成功応答(mSuccess)＋最初のイベントのみ | CODE_ONLY |
| 48 | minecraft:interact_with_lectern | `GameMode::$useItemOn` 成功応答(mSuccess)＋最初のイベントのみ | CODE_ONLY |
| 49 | minecraft:interact_with_loom | `GameMode::$useItemOn` 成功応答(mSuccess)＋最初のイベントのみ | CODE_ONLY |
| 50 | minecraft:interact_with_smithing_table | `GameMode::$useItemOn` 成功応答(mSuccess)＋最初のイベントのみ | CODE_ONLY |
| 51 | minecraft:interact_with_smoker | `GameMode::$useItemOn` 成功応答(mSuccess)＋最初のイベントのみ | CODE_ONLY |
| 52 | minecraft:interact_with_stonecutter | `GameMode::$useItemOn` 成功応答(mSuccess)＋最初のイベントのみ | CODE_ONLY |
| 53 | minecraft:drop | `ItemDropResult` → StatsReader custom集計（C++テスト済） | CODE_ONLY |
| 54 | minecraft:enchant_item | `ItemStackRequestActionHandler::_handleTransfer` のエンチャント出力転送成功→`ll26.60.enchant_output_transfer.v1`（ネイティブStatsReader・Web・ランキングの単体テスト済み、実機未確認） | CODE_ONLY |
| 55 | minecraft:jump | `PlayerJumpImpulse`（既存・C++テスト済） | UNIT_ONLY |
| 56 | minecraft:mob_kills | `DirectKillObservation(mob)`（既存・C++テスト済） | UNIT_ONLY |
| 57 | minecraft:play_record | `Player::$useItem` のレコード設置後 `ll26.60.play_record_place.v1`→CUSTOM統計・ランキング（単体テスト済み、実機未確認） | CODE_ONLY |
| 58 | minecraft:play_noteblock | `GameMode::$useItemOn` 成功応答(mSuccess)＋最初のイベントのみ | CODE_ONLY |
| 59 | minecraft:tune_noteblock | `GameMode::$useItemOn` 成功応答(mSuccess)＋最初のイベントのみ | CODE_ONLY |
| 60 | minecraft:deaths | `PlayerDeath`（既存・C++テスト済） | UNIT_ONLY |
| 61 | minecraft:pot_flower | `GameMode::$useItemOn` 成功応答(mSuccess)＋最初のイベントのみ | CODE_ONLY |
| 62 | minecraft:player_kills | `DirectKillObservation(player)`＋子飛び道具所有者（既存・C++テスト済） | UNIT_ONLY |
| 63 | minecraft:raid_trigger | BLOCKED: エフェクトID29/36の付与APIがLL26.60非公開、バージョン依存 | BLOCKED |
| 64 | minecraft:raid_win | BLOCKED: 襲撃勝利のイベント/判定APIなし | BLOCKED |
| 65 | minecraft:clean_shulker_box | `GameMode::$useItemOn` 成功応答(mSuccess)＋最初のイベントのみ | CODE_ONLY |
| 66 | minecraft:open_shulker_box | `GameMode::$useItemOn` 成功応答(mSuccess)＋最初のイベントのみ | CODE_ONLY |
| 67 | minecraft:time_since_death | 最終確定死亡からの完結セッション経過を50ms換算tickで算出 | CODE_ONLY |
| 68 | minecraft:time_since_rest | BLOCKED: 起床/休息原点イベントなし（上流READMEでも取消線） | BLOCKED |
| 69 | minecraft:sneak_time | `PlayerSneakingEvent`→`PlayerSneakedEvent` の実時間をtick換算 | CODE_ONLY |
| 70 | minecraft:talked_to_villager | `ServerPlayer::$openTrading` 正常終了後の村人target解決 | CODE_ONLY |
| 71 | minecraft:target_hit | `TargetBlock::$onProjectileHit` 処理後＋飛び道具所有者Player解決 | CODE_ONLY |
| 72 | minecraft:play_time | 完結セッション duration_ms 合計→50ms換算tick | CODE_ONLY |
| 73 | minecraft:total_world_time | プレイヤー別に取得できるAPIなし（play_timeで代替しない） | MISSING |
| 74 | minecraft:sleep_in_bed | サーバーtickの `Player::isSleeping()` false→true 遷移 | CODE_ONLY |
| 75 | minecraft:traded_with_villager | `ItemStackRequestActionHandler::_handleTransfer` 成功＋取引結果プレビュー→プレイヤー所持品転送 | CODE_ONLY |
| 76 | minecraft:trigger_trapped_chest | `GameMode::$useItemOn` 成功応答(mSuccess)＋最初のイベントのみ | CODE_ONLY |
| 77 | minecraft:use_cauldron | `GameMode::$useItemOn` 成功応答(mSuccess)＋最初のイベントのみ | CODE_ONLY |

集計: CODE_ONLY 61 / UNIT_ONLY 5 / BLOCKED 9 / MISSING 2。**未対応を実装済みとは扱わない。**
BLOCKEDの根拠: `SharedAttributes::ABSORPTION`、`Player::$_hurt`/`_blockUsingShield`、
`BucketItem::$_useOn`、襲撃エフェクト付与API、休息原点イベントは
LL26.60/prelink に公開されず、代替イベントもありません。MISSINGは登攀フラグとプレイヤー別ワールド時間です。

上流READMEの例外（`time_since_death`/`time_since_rest`の取消線、`nautilus_one_cm`/`happy_ghast_one_cm`の表外）はキー列挙を削らず保持し、実装状況を本表で区別します。

## 3. ゲーム内コマンド

| 機能 | 実装 | 状態 |
| --- | --- | --- |
| `/stats gui [StatsType]` | `ll::command` + `SimpleForm`、9カテゴリボタンで切替 | CODE_ONLY（リンク/PE検証・隔離BDS enable PASS、実コマンド実行は未） |
| `/stats rank <StatsType> [page]` | 10件/頁、10頁上限、フォーム/テキスト。CUSTOMは `custom:<key>` 必須 | CODE_ONLY |
| `/stats player <playerName> <StatsType>` | `StatsReader` で名称/UUID解決、詳細フォーム | CODE_ONLY |
| 権限 | `CommandPermissionLevel::Any`（読取専用） | CODE_ONLY |
| 日本語表示 | カテゴリ・主要CUSTOMキーのラベル、未知キーはキー表示 | UNIT_ONLY（カタログ単体） |

## 4. 互換API（4関数）

DLL export（`GetProcAddress` で他Modが解決）:
`getPlayerStats`, `getRankStats`, `getPlayerStatsPage`, `getRankStatsPage`。

- 戻り値: 書き込みバイト数（>=0）、負値はエラー。JSONは呼出側バッファ、`ok:false`+`error`。
- カテゴリ: 9カテゴリの `minecraft:*` / 短縮名 / 日本語ラベルを受理。CUSTOMランキングは `custom:<明示キー>`（例 `custom:minecraft:walk_one_cm`）のみ。裸の `custom` は単位混在防止のため拒否。
- ページング: `page`(0..1000), `limit`(1..100)。上限外は拒否。
- 状態: 4関数とも in-process C++ テスト PASS（JSON・ページング・拒否経路）。実BDSで別Modからの `GetProcAddress` 呼出は未検証。
- LegacyRemoteCall/LIP への直接依存は追加していない（AGPL/別配布依存を避け、独自C ABIで同等機能を提供）。

## 5. 永続化・表示

- 既存 bounded SQLite writer / schema v4 を維持。新規イベント種別は既存 `events` と `player_daily_types` を再利用（スキーマ変更なし）。
- 日本語Webダッシュボード: `companion/web/actions.{html,js}` にクラフト/移動/相互作用/ダメージのパネルとランキング指標を追加。
- JST日/月/全期間は既存CTEを継承し、新指標も同じ期間境界で集計。

## 6. 検証済み（本ターン）

| 項目 | 結果 |
| --- | --- |
| pinned 26.60 ヘッダでの新規7ソース + 変更3ソースのコンパイル | PASS |
| `scripts/build_server_candidate.ps1` フルビルド（prelink/link/PE import検証） | PASS。最終 `.build/server-candidate/run-f13c762e6ac449898744c27475e3f7e8`、DLL SHA256 `118F032C91103F657F6B9C2A9D209AA67E252F1C0C36A64024474C1A4A99258B`、ZIP `D3E175A83C5785AD8484F3BB7D4A8316659A9061792BA5DFEED37D4B7162ACDB` |
| `tests/parity_observation_tests.cpp` | PASS（移動モード/距離、相互作用対応、ダメージ、クラフト上限、9カテゴリ+77キー） |
| `tests/stats_reader_tests.cpp`（新規） | PASS（プレイヤー解決、カテゴリ集計、custom対応付け、play_time、time_since_death、睡眠/スニーク/破損、custom明示キーランキング拒否/成功） |
| `tests/remote_api_tests.cpp`（新規） | PASS（4公開関数、JSON単位、ページング、拒否経路、`custom:<key>`必須、生ID非公開） |
| Python 全スイート | 101 PASS |
| `node --check` + 認証HTTP→日本語DOM | PASS |
| 新規 isolated BDS scratch ロード | PASS: 最終 `.build/isolated-bds/run-fb9449daa4824f3f917c2746862a7b89`（DLL `118F03…`、ports 59900/59901）。native_loaded/enabled=true、DB作成=true、exit0、forced=false。空allowlistのため**実プレイヤーイベントは未検証** |
| 実プレイヤー→SQLite、コマンド実行、GetProcAddress実呼出 | **未検証** |

## 7. 互換APIの契約（実装済みC ABI）

- `int getPlayerStats(const char* player, const char* category, char* out, int cap)`
- `int getRankStats(const char* category, int page, int limit, char* out, int cap)`
- `int getPlayerStatsPage(const char* player, const char* category, int page, int limit, char* out, int cap)`
- `int getRankStatsPage(const char* category, int page, int limit, char* out, int cap)`

戻り値>=0はNUL終端JSONの長さ、`-1`引数不正、`-2`ページ/容量不正、`-3`バッファ不足、`-4`不明カテゴリ、それ以外は応答内 `ok:false`。**生のplayer_id/XUIDは応答に含めない**（表示名のみ）。`page` 0..1000、`limit` 1..100。

## 8. 2026-11-07 追加実装：確定相互作用・実ダメージ読取・取引成立

- `levilamina/player_interact.cpp`: `GameMode::$useItemOn`（MCAPI）をフックし、戻り値
  `InteractionResult::mSuccess` が真かつ最初のイベントのときだけ `PlayerInteractionResult`
  （`ll26.60.gamemode_use_item_on_success.v1`）を保存します。処理前の
  `PlayerInteractBlockEvent` は観測用に別causeで残し、確定値には加算しません。
  `core/interaction_observation.hpp` の `kInteractionKeys`（33件）が
  `interaction_key_valid()` とリーダSQLの双方の単一情報源です。
- `core/stats_reader.cpp`: CUSTOM集計とキー別ランキングが上記確定causeを受理。
  処理前causeは引き続き拒否。`custom:<key>` 指定時のみ順位付けします。
- ダメージ: 既存の `ActorHurtEvent` 実量（`ll26.60.actor_hurt_event.v1` /
  `ll26.60.actor_hurt_real_damage.v1`、1/10単位）をCUSTOM `damage_taken` /
  `damage_dealt` として読み取り・ランキング対応に接続しました。攻撃回数は
  ダメージへ換算しません。
- `levilamina/player_trade.cpp`: `ItemStackRequestActionHandler::_handleTransfer`
  （MCAPI）を追加フックし、`ItemStackNetResult::Success` かつ取引結果プレビュー
  （33/49）→プレイヤー所持品/カーソル（12/28/29/59）の転送のみを
  `traded_with_villager` として1回計数します。材料転送・失敗は除外。
- `scripts/validate_server_release.ps1`: 上記2つのMCAPIシンボルのみを
  レビュー済み許可リストへ追加し、`$useItemOn` はWeak参照APIとの対を要求。
- テスト: `tests/parity_observation_tests.cpp`（33キー整合・成功ゲート・取引分類）、
  `tests/stats_reader_tests.cpp`（確定相互作用/実ダメージ/取引の集計とキー別ランキング、
  処理前通知が加算されないこと）、`tests/test_stats_view.py`（同契約）。
- ビルド/ロード: フル候補ビルド PASS、隔離BDSで native_loaded/enabled/server_started/
  exit0/database_exists を確認。**実プレイヤー操作→SQLiteは未検証**。

| 検証 | 結果 |
| --- | --- |
| pinned 26.60 ヘッダでの変更4 TUコンパイル | PASS |
| `scripts/build_server_candidate.ps1`（prelink/link/PE import検証） | PASS |
| 候補DLL SHA256 | `F11BF8510590CA89513F67FB9389DC53DB5353C03D51727EA222270FD8D05322` |
| 候補ZIP SHA256 | `0183CF1ECBF87B06E4802A6921F6AF9F3F6407B98EEA8CEB549D081AC960A119` |
| 隔離BDS `.build/isolated-bds/run-782536caf3f24f52bfe79f70c340f364` | native_loaded/enabled=true, exit0, forced=false, DB作成=true |
| C++ 単体（parity/stats_reader/remote_api/killed_by） | PASS |
| Python `unittest discover tests` | 126 PASS |
| 実プレイヤー→SQLite→GUI/API | **未検証** |
