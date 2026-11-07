# LK-Stats v0.13.0 完全同等性 ギャップマトリクス

## broken v2追補（副手・人型防具4枠候補、実機未受入）

既存hurtAndBreak内で主Inventory／副手／防具4枠を一意に照合し、
同Player/Level/次元・同stackの1→0と入力耐久閾値を記録するv2を追加。
native GUI/C API/Webも同じbounded証拠契約を検査。v1履歴は保持し、旧定義のままです。
[捕捉と欠測・因果・保存の限界](BROKEN_CAPTURE_V2.md)。盾アイテムの破損候補は盾防御量の実装ではなく、
CUSTOM盾は引き続きMISSING。全96行の実操作/SQLite/GUI/API受入は昇格しません。

## 採掘・設置v2追補（型付きframe確認・native/Web整合、実機未受入）

既存GameModeフックの弱参照再解決後、削除／Player／Level／次元を検査してからBlockSourceを読み、
実際のBlockPosを保存するv2を追加。ネイティブmined個人／種類／順位／C APIも証拠JSONを検査し、
空JSON・過大個数・試行・偽frame・未来行を合算しません。v1履歴は限定定義のまま保持。
[定義と次元型／同座標設置の制限](BLOCK_RECORDS_V2.md)。最終compile／fixture証跡はSTATUS参照。
全96行の実プレイ／SQLite／GUI／互換API受入とCUSTOM72/4/1の宣言は昇格しません。

## 表示名ファイル追補（言語機能候補・実機未受入）

独立作成138件の共有日本語辞書をWeb／native GUI／C APIに接続。
有界schema1ファイル、任意上書き設定、未知IDフォールバック、CUSTOM観測ラベル優先を実装。
ID・値・順位・旧DB・producerを変えず、辞書項目の存在は捕捉対応／実機証明に数えません。
[設定と厳密な制限](TYPE_LABELS.md)。最終compile／回帰証跡はSTATUSを参照。
全96行の実操作／SQLite／GUI／互換API受入、CODE_ONLY72／UNIT_ONLY4／MISSING1は変更しません。

## HEALTH属性減少の追補（damage_taken/dealtの独立ソース・未受入）

既存Player/Mob hurtフック内でHEALTH属性の型付き前後値を独立採取し、
`ll26.60.hurt_health_delta.v1` としてwriterへ渡す候補を追加。
処理前ActorHurtEvent通知・ABSORPTIONとは合算せず、個人/順位/C API/日本語Webは
新causeと厳密な同一性・型・丸め・帰属を検証します。旧通知だけなら新CUSTOMはnull。
「最終ダメージ」や専属因果の断定はせず、正味のHP属性減少観測と明記します。
[条件と限界・実機比較](HEALTH_LOSS_CAPTURE.md)。compile/fixtureの最終証跡はSTATUSを参照。
2キーともCODE_ONLY、全96の実操作/SQLite/GUI/API受入は引き続き未達です。

## 盾のraw証拠採取追補（統計producerではない）

既定無効の型付きPlayer hurt前後観測と私有DB用CLIを追加。
exact process-local opt-in=1だけでフック登録、最大256名・個人5秒間隔で有界writerへ保存。
新規event_kind player_shield_evidence / amount0 / eligible_for_statistics=false。
このraw観測があってもnative個人/順位・認証Webの盾は未対応/nullのままです。
[操作・フィールド・厳密な限界](SHIELD_CAPTURE_FINDINGS.md)。既存BDS/18767へ配備せず、
実ガード/無敵/キャンセル等の比較は未実施。成功・実軽減量の取得条件が未確定なので
盾はMISSINGを維持し、CODE_ONLY72 / UNIT_ONLY4 / MISSING1を変更しません。
全96行の実操作・SQLite・GUI/API受入も昇格しません。

| 対象 | compile / 独立fixture | 実操作 / 実SQLite / GUI/API受入 |
| --- | --- | --- |
| player_shield_evidence（CUSTOM統計ではない） |26native units/DLL/厳密PE PASS: candidate run-e3d0fa4739134cf9b65917d34b290ce8。11C++/8readbacks PASS: fixture run-0cc6c961b94d4b12bdeea082d42f3b7f。167Python PASS: run-c9a2fcc279cb44bfb2296e2833cc06b8 | 未実施。rawがあっても盾の値/順位を作らず、成功条件・数量の実機比較が必要 |

## 相関した耐性参照式追補（適用量の実測・実プレイ未受入）

公開Player/Mob hurt内の同一対象/sourceのエンチャント後stageを一回だけ対応付け、
同じ効果・Level/次元と外側DamageProcessedを確認して耐性量の参照式を計算します。
最終HP差・実適用軽減量ではなく、ラベル・JSON・状態にその区別を明記。
[定義](RESISTANCE_CAPTURE.md)。現在76の限定投影。盾は[未実装](SHIELD_CAPTURE_FINDINGS.md)。
CODE_ONLY72 / UNIT_ONLY4 / MISSING1。全96行の実操作・実SQLite・GUI/API受入は未達です。

| キー | 実装 / compile | 合成SQLite / UI | 実操作 / 実SQLite / GUI / 互換API |
| --- | --- | --- | --- |
| minecraft:damage_resisted | player_resistance.obj、25units/DLL/厳密PE PASS。candidate run-9bb957e117ed4ae9941f9a4f98cf9eb7 | builder/writer/native個人・順位/integrity/認証HTTP/実JS合成DOM PASS。fixture run-1fdd9a6d7599411e8e3b436201940ca5 | 全て未受入。全耐性レベル・併用/拒否/死亡・実engine段階を要照合 |
| minecraft:damage_dealt_resisted | 同candidate、直接/子飛び道具source所有者を再解決 | 同fixture。各キーAlice30/Bob20、計50、誤帰属/不正行除外 | 全て未受入。Mob/PvP/所有飛び道具・環境負例を要照合 |

コンパイル／合成の個別証跡は実機acceptedへ昇格しません。旧damage通知・吸収差・休息後snapshotは維持。

## 休息後Player tick追補（実プレイ未受入）

公開ServerPlayer::$normalTickの完了後、睡眠状態を原点として実Player呼出し数を保持。
初期・再接続・途絶は不明、観測した睡眠中は既知0。個人別最新スナップショットを使い、
過去の正の値をSUM/MAXしません。[定義](REST_CAPTURE.md)。休息追補時74、耐性式追補後76の限定投影は完成ではなく、
盾はMISSING。全96行の実プレイ/SQLite/GUI/APIゲートは未受入です。

| キー | 実装 / compile | 合成SQLite / UI | 実BDS操作 / 実SQLite / ゲームGUI / 互換API |
| --- | --- | --- | --- |
| minecraft:time_since_rest | 専用player_rest.objを含む24units・DLLリンク・PE検査PASS。candidate run-a280aac3d1a74788b70d09f18fb7938d | builder→writer→最新0/不明/順位・integrity・認証HTTP/実JS合成DOM PASS。fixture run-3850f5b182cd46399ac2aea5e73569ce | すべて未受入。通常ホスト新rootで就寝/起床/再接続を要検証 |

これは対象キーの段階別証跡であり、全96行を保守的に列挙するinventoryのaccepted=falseを変更しません。

## 吸収属性減少追補（実engine・実プレイ未受入）

公開Player/Mob::$_hurtの前後で、同じActor・属性のABSORPTION減少を観測する独立producerを追加。
Player→同じMob親1回は別に数えず、それ以外の再入・交換・不正値を除外。被害者本人と、
解決した直接／子飛び道具source Playerへ別帰属します。[定義](ABSORPTION_CAPTURE.md)。
通知量からのmin推計・最終HP損失ではなく、丸めた属性差。吸収追補時73、休息追補後74、耐性式追補後76の限定投影は
全機能完成を意味せず、盾はMISSING。全96行の実機ゲートは昇格しません。

## 襲撃関連効果追補（実襲撃・実プレイ未受入）

固定Actor.hのaddEffect／getEffect(uint)はMCAPIでした。旧「付与API非公開」を訂正し、
同じPlayerの付与前後を比較する独立観測を追加。RaidOmen600tick／VillageHero48000tickの
新規・refreshだけを読み、無変化・拒否・再入れ子は除外します。コマンド／他Mod由来は
区別できず、実際の襲撃発生・勝利・参加回数には昇格しません。[定義](RAID_EFFECT_CAPTURE.md)。
未記録null。全96行の実操作／SQLite／GUI／API受入は依然未完了です。

## fish_caught追補（実プレイ未受入）

固定ヘッダーのPlayer::interact／BucketItem::$_useOnはMCAPIでした。旧非公開宣言を訂正し、
同じ同期呼出し・主手water bucket・success/swing・対象退役・対応バケツ+1を照合する独立捕捉を追加。
釣り竿の釣果や全捕獲ではなく、弱参照解決不可とremovedフラグも区別します。
[定義と境界](FISH_BUCKET_CAPTURE.md)。コード・合成SQLite/UI・DLLビルドを実機受入へ昇格しません。

## total_world_time追補（実機未受入）

上流の個人CUSTOM表示へ注入される値はプレイヤー時間ではなくLevelの共有tick IDでした。
公開仮想`Level::getCurrentServerTick()`の値を5秒窓で1回保存し、native/Webで最後の値だけを
読みます。busyサンプル件数合計を代用しません。共有値の順位は明示的に対象外。
[定義・証拠・再起動の限界](WORLD_TICK_METRIC.md)。全96行の実機accepted=falseは維持。

## 移動v2追補（実機ゲートを昇格しない）

17移動モードを新しい連続サーバー座標窓へ接続し、型名部分一致・上下成分欠落・水系の
重複・tick丸め・終了時刻を修正。登攀は固定26.60公開`Actor::onClimbableBlock()`を使用。
下表の移動行もv2へ更新しました。ブロック名ヒューリスティックはv2では使いません。
[定義と限界](TRAVEL_CAPTURE_V2.md)。StorageWriter／SQL／HTTP／DOM fixtureやDLLリンクは
各モードの実プレイ証明ではなく、全96行のaccepted=falseを維持します。

## Web追補：全選択肢と限定読取（受入ゲートは昇格しない）

新しい別プロセス用 `/stats` に9分類／77キーのセレクターと、日本語・種類別明細・
期間／Player／種類検索・独立ページ／ランキングを接続しました。
[Web読取の正確な定義](STATS_LIBRARY.md)。未対応はnull。
後続sourceも保持し、対応判定はAPIのcatalogで確認できます。処理前damage申告量は
`pre_origin_notification`と専用ラベルで区別し、確定HPへ変換せず、ラベルを収集証明にしません。
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
| MISSING | 正当な統計の成功条件・数量が未確定。raw調査フックを**統計の実装済み扱いにしない** |
| BLOCKED | 公式API/権限/ライセンス上、現時点で取得不可 |

全項目に共通: **実機（実プレイヤー操作→SQLite→UI/API）は未検証**。`PROJECT_STATUS: IN_PROGRESS`。

## 1. 9統計カテゴリ

| カテゴリ | 取得経路（実装） | 試行/成功の区別 | 状態 | 残課題 |
| --- | --- | --- | --- | --- |
| `minecraft:custom` | 移動/確定相互作用/実ダメージ/睡眠成立/スニーク時間/ジャンプ/直接キル/死亡/退出/取引成立/play_time | 観測・遷移・実時間・エンジン成功応答のみ | PARTIAL | 下記77キーの未対応分 |
| `minecraft:mined` | `GameMode::$destroyBlock` 同座標状態変化v2、弱参照Player/Level/次元と実BlockPos | v1履歴保持、v2のframe確認へ昇格せず。native/Webともtyped JSON・amount1等を検証 | CODE_ONLY（26units/DLL・fixture PASS、実操作/GUI/API未受入） | 全経路・最終保存・他Mod専属因果、int8次元の範囲外 |
| `minecraft:broken` | `ItemStackBase::hurtAndBreak` + maxDamage/入力閾値 + 同一stack1→0。v2は主所持品/副手/防具4枠・同Player/Level/次元 | 耐久処理true・入力閾値と消失の同時観測。全破損因果ではない | CODE_ONLY | 実プレイ/SQLite/GUI/API未受入。コピー/枠交換/別経路/最終保存は未証明、v1は旧主所持品のみ |
| `minecraft:crafted` | `CraftHandlerBase::postRequest(true)` + `mCraftResults` | 失敗要求は除外 | CODE_ONLY | インベントリ最終コミット・NBT未検証 |
| `minecraft:used` | `GameMode::$useItem(true)` + 所持差分 | true応答と効果完了を区別 | COMPLETED_CAPTURE（意味限定） | 効果完了は未計測 |
| `minecraft:picked_up` | `Player::take` 前後差分 | 入口個数ではない | COMPLETED_CAPTURE | 全拾得・容器は対象外 |
| `minecraft:dropped` | `Player::$drop(true)` 入口スタック | true戻り値のみ | COMPLETED_CAPTURE | 永続化・生成物未検証 |
| `minecraft:killed` | `Mob::$die` 直接攻撃／子飛び道具所有者の死亡フラグ成立 | 通知/試行と別 | COMPLETED_CAPTURE（意味限定） | 環境・間接原因は欠測 |
| `minecraft:killed_by` | `Player::$die`の同一weak死亡成立＋解決済みsource種類 | 原因コードと別、新sourceのみ | CODE_ONLY | 実BDS・直接/射撃/PvP・UI受入未完 |

## 2. CUSTOM 77キー

凡例: 距離=`ServerLevelTickEvent` 上の連続座標窓（`ll26.60.poll_position_delta.v2`、v1履歴も保持）。各移動行は候補DLLリンク済み・分類/読取fixtureあり、実プレイ/実行動SQLite/GUI/APIは全て未検証。[証拠と制限](TRAVEL_CAPTURE_V2.md)。
Current native semantics: Block::use post-origin true and dedicated Cake/Cauldron/FlowerPot/NoteBlock causes (32 interaction keys); real ActorHurtEvent damage; CreatedOutputContainer success in player_item_transfer.cpp for trade. Pre-outcome notifications do not count. Real-player validation pending.
ダメージ=`ActorHurtEvent` の処理前申告量（1/10単位、`ll26.60.actor_hurt_real_damage.v1`という旧cause名でも最終HP量とは扱わない）、
Trade (current producer): player_item_transfer.cpp _handleTransfer with CreatedOutputContainer and ItemStackNetResult::Success -> ll26.60.trade_created_output.v1. The 33/49 -> 12/28/29/59 preview route is NOT implemented as a standalone hook.
処理前通知 `ll26.60.player_interact_block_event.v1` は保存のみで、どのCUSTOM値にも加算しません。

| # | key | 実装経路 | 状態 |
| --- | --- | --- | --- |
| 1 | minecraft:animals_bred | `BreedableComponent::mate` 返却offspring非空＋love cause解決 | CODE_ONLY |
| 2 | minecraft:clean_armor | CauldronBlock::_sendCauldronUsedEventToClient -> ll26.60.cauldron_clear_item.v1 (unverified in real player) | CODE_ONLY |
| 3 | minecraft:clean_banner | CauldronBlock::_sendCauldronUsedEventToClient -> ll26.60.cauldron_clear_item.v1 (unverified in real player) | CODE_ONLY |
| 4 | minecraft:open_barrel | Block::use returned true -> ll26.60.block_use_success.v1 (unverified in real player) | CODE_ONLY |
| 5 | minecraft:bell_ring | Block::use returned true -> ll26.60.block_use_success.v1 (unverified in real player) | CODE_ONLY |
| 6 | minecraft:eat_cake_slice | CakeBlock::removeCakeSlice -> ll26.60.cake_remove_slice.v1 (unverified in real player) | CODE_ONLY |
| 7 | minecraft:fill_cauldron | CauldronBlock::_useInventory -> ll26.60.cauldron_use_inventory.v1 (unverified in real player) | CODE_ONLY |
| 8 | minecraft:open_chest | Block::use returned true -> ll26.60.block_use_success.v1 (unverified in real player) | CODE_ONLY |
| 9 | minecraft:damage_absorbed | Player/Mob::$_hurtの同一吸収属性減少・DamageProcessed型・丸め、親1回以外の再入を除外。hurt_absorption_delta.v1、実機未受入 | CODE_ONLY |
| 10 | minecraft:damage_blocked_by_shield | 公開Player hurtで型付きraw前後値を採取する既定無効の別event_kind/amount0を追加。予測・flag74・timestamp・戻りの観測であり、成功/量を未証明。統計/順位/Webへ計上しない。SHIELD_CAPTURE_FINDINGS.md。実機比較待ち | MISSING |
| 11 | minecraft:damage_dealt | 同一hurtのHEALTH属性減少・直接/子飛び道具Player再解決、hurt_health_delta.v1。通知/吸収とは非合算、正味変化で専属因果/最終結果未証明。HEALTH_LOSS_CAPTURE.md | CODE_ONLY |
| 12 | minecraft:damage_dealt_absorbed | 同じ吸収属性差を解決済み直接／子飛び道具source Playerへ帰属。不明/自己/連鎖は欠測、最終HP損失ではない。実機未受入 | CODE_ONLY |
| 13 | minecraft:damage_dealt_resisted | player_resistance.cpp / core/resistance_observation.hpp。公開stageと同じhurt/sourceの対応・外側DamageProcessed・安定効果から参照式を算出。直接/子飛び道具Player帰属。compile/link/合成SQLite/UI PASS、実適用量ではなく実機未受入 | CODE_ONLY |
| 14 | minecraft:damage_resisted | 同producer。Player本人の相関したstage基準の参照式、最寄りmilli後1/10HPへ丸め。事前通知/理論式を実測HP差へ昇格しない。compile/link/合成SQLite/UI PASS、実BDS/GUI/API未受入 | CODE_ONLY |
| 15 | minecraft:damage_taken | 同一Player hurtのHEALTH属性減少・同じ属性実体・DamageProcessed、hurt_health_delta.v1。通知/吸収とは非合算、回復/世界コミット等は未証明。HEALTH_LOSS_CAPTURE.md | CODE_ONLY |
| 16 | minecraft:inspect_dispenser | Block::use returned true -> ll26.60.block_use_success.v1 (unverified in real player) | CODE_ONLY |
| 17 | minecraft:nautilus_one_cm | v2・正確なnautilus型ID、同一乗り物IDのXYZ窓 | CODE_ONLY |
| 18 | minecraft:happy_ghast_one_cm | v2・正確なhappy_ghast型ID、同一乗り物IDのXYZ窓 | CODE_ONLY |
| 19 | minecraft:boat_one_cm | v2・明示boat/raft型、同一乗り物IDのXYZ窓 | CODE_ONLY |
| 20 | minecraft:aviate_one_cm | v2・gliding優先、XYZ窓 | CODE_ONLY |
| 21 | minecraft:horse_one_cm | v2・明示horse系型、同一乗り物IDのXYZ窓 | CODE_ONLY |
| 22 | minecraft:minecart_one_cm | v2・明示minecart型、同一乗り物IDのXYZ窓 | CODE_ONLY |
| 23 | minecraft:pig_one_cm | v2・正確なpig型（piglin不可）、同一乗り物IDのXYZ窓 | CODE_ONLY |
| 24 | minecraft:strider_one_cm | v2・正確なstrider型、同一乗り物IDのXYZ窓 | CODE_ONLY |
| 25 | minecraft:climb_one_cm | v2・上昇＋公開Actor::onClimbableBlock、上昇Y窓（候補で解決/リンク済み、実機未） | CODE_ONLY |
| 26 | minecraft:crouch_one_cm | v2・接地sneaking、XZ窓 | CODE_ONLY |
| 27 | minecraft:fall_one_cm | v2・非接地、下降Yのみ | CODE_ONLY |
| 28 | minecraft:fly_one_cm | v2・flying、XYZ窓 | CODE_ONLY |
| 29 | minecraft:sprint_one_cm | v2・接地sprinting、XZ窓 | CODE_ONLY |
| 30 | minecraft:swim_one_cm | v2・in_waterかつswimming、他水系と排他的なXYZ窓 | CODE_ONLY |
| 31 | minecraft:walk_one_cm | v2・接地通常歩行、XZ窓 | CODE_ONLY |
| 32 | minecraft:walk_on_water_one_cm | v2・in_waterかつ非swimmingかつ非isUnderLiquid(Water)、XYZ窓 | CODE_ONLY |
| 33 | minecraft:walk_under_water_one_cm | v2・in_waterかつ非swimmingかつisUnderLiquid(Water)、XYZ窓 | CODE_ONLY |
| 34 | minecraft:inspect_dropper | Block::use returned true -> ll26.60.block_use_success.v1 (unverified in real player) | CODE_ONLY |
| 35 | minecraft:open_enderchest | Block::use returned true -> ll26.60.block_use_success.v1 (unverified in real player) | CODE_ONLY |
| 36 | minecraft:fish_caught | 公開Player::interact＋BucketItem::$_useOnの同呼出し、主手バケツ型別+1／対象退役の限定観測。fish_bucket_delta.v1、釣り竿は対象外、実機未受入 | CODE_ONLY |
| 37 | minecraft:leave_game | 既存 `session_end` → custom集計 | UNIT_ONLY |
| 38 | minecraft:inspect_hopper | Block::use returned true -> ll26.60.block_use_success.v1 (unverified in real player) | CODE_ONLY |
| 39 | minecraft:interact_with_anvil | Block::use returned true -> ll26.60.block_use_success.v1 (unverified in real player) | CODE_ONLY |
| 40 | minecraft:interact_with_beacon | Block::use returned true -> ll26.60.block_use_success.v1 (unverified in real player) | CODE_ONLY |
| 41 | minecraft:interact_with_blast_furnace | Block::use returned true -> ll26.60.block_use_success.v1 (unverified in real player) | CODE_ONLY |
| 42 | minecraft:interact_with_brewingstand | Block::use returned true -> ll26.60.block_use_success.v1 (unverified in real player) | CODE_ONLY |
| 43 | minecraft:interact_with_campfire | Block::use returned true -> ll26.60.block_use_success.v1 (unverified in real player) | CODE_ONLY |
| 44 | minecraft:interact_with_cartography_table | Block::use returned true -> ll26.60.block_use_success.v1 (unverified in real player) | CODE_ONLY |
| 45 | minecraft:interact_with_crafting_table | Block::use returned true -> ll26.60.block_use_success.v1 (unverified in real player) | CODE_ONLY |
| 46 | minecraft:interact_with_furnace | Block::use returned true -> ll26.60.block_use_success.v1 (unverified in real player) | CODE_ONLY |
| 47 | minecraft:interact_with_grindstone | Block::use returned true -> ll26.60.block_use_success.v1 (unverified in real player) | CODE_ONLY |
| 48 | minecraft:interact_with_lectern | Block::use returned true -> ll26.60.block_use_success.v1 (unverified in real player) | CODE_ONLY |
| 49 | minecraft:interact_with_loom | Block::use returned true -> ll26.60.block_use_success.v1 (unverified in real player) | CODE_ONLY |
| 50 | minecraft:interact_with_smithing_table | Block::use returned true -> ll26.60.block_use_success.v1 (unverified in real player) | CODE_ONLY |
| 51 | minecraft:interact_with_smoker | Block::use returned true -> ll26.60.block_use_success.v1 (unverified in real player) | CODE_ONLY |
| 52 | minecraft:interact_with_stonecutter | Block::use returned true -> ll26.60.block_use_success.v1 (unverified in real player) | CODE_ONLY |
| 53 | minecraft:drop | `ItemDropResult` → StatsReader custom集計（C++テスト済） | CODE_ONLY |
| 54 | minecraft:enchant_item | `ItemStackRequestActionHandler::_handleTransfer` のエンチャント出力転送成功→`ll26.60.enchant_output_transfer.v1`（ネイティブStatsReader・Web・ランキングの単体テスト済み、実機未確認） | CODE_ONLY |
| 55 | minecraft:jump | 本家LK-Stats準拠のLeviLamina `PlayerJumpEvent` → `ll26.60.player_jump_event.v1` → `PlayerJumpImpulse`。26.60ヘッダーコンパイル・ネイティブ読取/ランキング・Web単体テスト成功。実プレイヤーでのジャンプ再検証待ち。 | CODE_ONLY |
| 56 | minecraft:mob_kills | `DirectKillObservation(mob)`（既存・C++テスト済） | UNIT_ONLY |
| 57 | minecraft:play_record | `Player::$useItem` のレコード設置後 `ll26.60.play_record_place.v1`→CUSTOM統計・ランキング（単体テスト済み、実機未確認） | CODE_ONLY |
| 58 | minecraft:play_noteblock | NoteBlock::$attack -> ll26.60.noteblock_attack.v1 (unverified in real player) | CODE_ONLY |
| 59 | minecraft:tune_noteblock | Block::use returned true -> ll26.60.block_use_success.v1 (unverified in real player) | CODE_ONLY |
| 60 | minecraft:deaths | `PlayerDeath`（既存・C++テスト済） | UNIT_ONLY |
| 61 | minecraft:pot_flower | FlowerPotBlock::use -> ll26.60.flower_pot_place.v1 (unverified in real player) | CODE_ONLY |
| 62 | minecraft:player_kills | `DirectKillObservation(player)`＋子飛び道具所有者（既存・C++テスト済） | UNIT_ONLY |
| 63 | minecraft:raid_trigger | 公開Actor::addEffect/getEffect、RaidOmen600tickの付与後変化。raid_effect_changed.v1、襲撃発生・付与原因の証明ではない、実機未 | CODE_ONLY |
| 64 | minecraft:raid_win | 同上VillageHero48000tickの付与後変化。勝利・参加の証明ではない、実機未 | CODE_ONLY |
| 65 | minecraft:clean_shulker_box | CauldronBlock::_sendCauldronUsedEventToClient -> ll26.60.cauldron_clear_item.v1 (unverified in real player) | CODE_ONLY |
| 66 | minecraft:open_shulker_box | Block::use returned true -> ll26.60.block_use_success.v1 (unverified in real player) | CODE_ONLY |
| 67 | minecraft:time_since_death | 最終確定死亡からの完結セッション経過を50ms換算tickで算出 | CODE_ONLY |
| 68 | minecraft:time_since_rest | player_rest.cpp / core/rest_counter.hpp。公開ServerPlayer::$normalTick完了後の睡眠観測原点、初期/再接続/途絶不明・既知0・5秒間隔snapshot、native/Web個人/順位。compile・合成writer/SQLite/UIは検証対象、実就寝/起床/再接続とGUI/APIは未受入 | CODE_ONLY |
| 69 | minecraft:sneak_time | `PlayerSneakingEvent`→`PlayerSneakedEvent` の実時間をtick換算 | CODE_ONLY |
| 70 | minecraft:talked_to_villager | `ServerPlayer::$openTrading` 正常終了後の村人target解決 | CODE_ONLY |
| 71 | minecraft:target_hit | `TargetBlock::$onProjectileHit` 処理後＋飛び道具所有者Player解決 | CODE_ONLY |
| 72 | minecraft:play_time | 完結セッション duration_ms 合計→50ms換算tick | CODE_ONLY |
| 73 | minecraft:total_world_time | 公開仮想Level::getCurrentServerTickの共有値を5秒窓で保存。world_tick_snapshot.v1を最新時刻で読取、SUM/MAX/個人順位なし、実機未受入 | CODE_ONLY |
| 74 | minecraft:sleep_in_bed | サーバーtickの `Player::isSleeping()` false→true 遷移 | CODE_ONLY |
| 75 | minecraft:traded_with_villager | player_item_transfer.cpp CreatedOutputContainer success -> ll26.60.trade_created_output.v1; reader also accepts old trade_result_transfer_success.v1 but no preview-container hook exists | CODE_ONLY |
| 76 | minecraft:trigger_trapped_chest | Block::use returned true -> ll26.60.block_use_success.v1 (unverified in real player) | CODE_ONLY |
| 77 | minecraft:use_cauldron | CauldronBlock::_useInventory -> ll26.60.cauldron_use_inventory.v1 (unverified in real player) | CODE_ONLY |

宣言行の集計: CODE_ONLY 72 / UNIT_ONLY 4 / BLOCKED 0 / MISSING 1（全77行）。**実機ゲートの集計ではなく、未対応を実装済みとは扱わない。**
旧BLOCKEDはAPI不在の独立監査ではありません。現在の固定ヘッダーではABSORPTION、
Player::$_hurt、Mob::$getDamageAfterEnchantReductionもMCAPI宣言でした。宣言の存在だけで
link/実機・段階/数量の正しさを証明しません。吸収差2キー・休息後tick・耐性参照式2キーには限定producerを追加し、盾の1キーはraw調査候補があってもMISSINGです。
効果付与・バケツ捕捉・sleep thunksの公開性も上記で訂正し、確定数量を推測で埋めません。
盾・耐性2項目・休息時間の4キーはWeb投影もなく、必須残課題です。
登攀の旧「フラグなし/取得なし」は上記v2の公開predicate解決により訂正しました。

上流READMEの例外（`time_since_death`/`time_since_rest`の取消線、`nautilus_one_cm`/`happy_ghast_one_cm`の表外）はキー列挙を削らず保持し、実装状況を本表で区別します。

## 3. ゲーム内コマンド

| 機能 | 実装 | 状態 |
| --- | --- | --- |
| `/stats gui [StatsType]` | `ll::command` + `SimpleForm`、9カテゴリボタンで切替 | CODE_ONLY（リンク/PE検証・隔離BDS enable PASS、実コマンド実行は未） |
| `/stats rank <StatsType> [page]` | 10件/頁、10頁上限、フォーム/テキスト。CUSTOMは `custom:<key>` 必須 | CODE_ONLY |
| `/stats player <playerName> <StatsType>` | `StatsReader` で名称/UUID解決、詳細フォーム | CODE_ONLY |
| 権限 | `CommandPermissionLevel::Any`（読取専用） | CODE_ONLY |
| 日本語表示 | カテゴリ・CUSTOM77の観測ラベル、共有138種類名・有界任意設定、未知ID表示。ID/値/順位は不変 | UNIT_ONLY（カタログ・設定・C API・認証HTTP/実JS合成DOM。native GUI実機は未受入） |

## 4. 互換API（4関数）

DLL export（`GetProcAddress` で他Modが解決）:
`getPlayerStats`, `getRankStats`, `getPlayerStatsPage`, `getRankStatsPage`。

- 戻り値: 書き込みバイト数（>=0）、負値はエラー。JSONは呼出側バッファ、`ok:false`+`error`。
- カテゴリ: 9カテゴリの `minecraft:*` / 短縮名 / 日本語ラベルを受理。CUSTOMランキングは `custom:<明示キー>`（例 `custom:minecraft:walk_one_cm`）のみ。裸の `custom` は単位混在防止のため拒否。
- ページング: `page`(0..1000), `limit`(1..100)。上限外は拒否。
- 状態: 4関数とも in-process C++ テスト PASS（JSON・ページング・拒否経路）。実BDSで別Modからの `GetProcAddress` 呼出は未検証。
- LegacyRemoteCall/LIPへの直接依存は追加していない。独立C ABIのみで、wire互換ではない。0.23系SDK/依存ライセンス確認は未了（LegacyRemoteCall自体をAGPLと断定しない）。詳細は [NATIVE_STATS_API.md](NATIVE_STATS_API.md)。

## 5. 永続化・表示

- 既存 bounded SQLite writer / schema v4 を維持。新規イベント種別は既存 `events` と `player_daily_types` を再利用（スキーマ変更なし）。
- 日本語Webダッシュボード: `companion/web/actions.{html,js}` にクラフト/移動/相互作用/ダメージのパネルとランキング指標を追加。
- JST日/月/全期間は既存CTEを継承し、新指標も同じ期間境界で集計。

## 6. 初期候補の検証記録（最新結果は後続追補とSTATUSを参照）

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

## 7. 独立C ABIの契約（LegacyRemoteCall非互換）

- `int getPlayerStats(const char* player, const char* category, char* out, int cap)`
- `int getRankStats(const char* category, int page, int limit, char* out, int cap)`
- `int getPlayerStatsPage(const char* player, const char* category, int page, int limit, char* out, int cap)`
- `int getRankStatsPage(const char* category, int page, int limit, char* out, int cap)`

戻り値>=0はNUL終端JSONの長さ、`-1`引数不正、`-2`ページ/容量不正、`-3`バッファ不足、`-4`不明カテゴリ、それ以外は応答内 `ok:false`。**生のplayer_id/XUIDは応答に含めない**（表示名のみ）。`page` 0..1000、`limit` 1..100。
## 8. 2026-11-07 source-aligned interaction/trade correction

This section supersedes the earlier account of a GameMode useItemOn success hook and a 33-key interaction list. The current tree has neither that producer nor a 33-key catalog.

- Native interaction implementation: `src/bedrock_insight/levilamina/player_block_interaction.cpp` hooks `Block::use` after it returns true (`ll26.60.block_use_success.v1`), plus `CakeBlock::removeCakeSlice` (`ll26.60.cake_remove_slice.v1`), `CauldronBlock::_useInventory` (`ll26.60.cauldron_use_inventory.v1`), `CauldronBlock::_sendCauldronUsedEventToClient` (`ll26.60.cauldron_clear_item.v1`), `FlowerPotBlock::use` (`ll26.60.flower_pot_place.v1`) and `NoteBlock::$attack` (`ll26.60.noteblock_attack.v1`). `PlayerInteractBlockEvent` attempts remain excluded.
- Catalog and reader: `core/interaction_observation.hpp` has 32 `kInteractionKeys` (24 generic + 8 dedicated). `interaction_key_cause()` and `core/stats_reader.cpp` use the real cause for each key. This is source-level logic only, not real-player acceptance.
- Trade: `levilamina/player_trade.cpp` contains the `ServerPlayer::$openTrading` hook for `talked_to_villager`. The current `_handleTransfer` hook is in `levilamina/player_item_transfer.cpp`: it confirms a CreatedOutputContainer transfer succeeded, emits `ll26.60.trade_created_output.v1`, and the reader includes it in `traded_with_villager`. Legacy `ll26.60.trade_result_transfer_success.v1` is reader-compatible but not currently produced by a separate preview-container transfer hook.
- Combat: native `ActorHurtEvent` and the dedicated real damage-dealt producer are read as `damage_taken`/`damage_dealt` (tenths); attack attempts are not substituted.
- Tests and build: native candidate DLL SHA256 `EAB1A3026A89B15E3A90DFE47EB86871C8FC46E97B40CD5786E2B79756B2F63D` passed compile/link/PE validation; Python 126 tests, fake native safety tests and C++ stats reader fixtures passed. Fresh isolated BDS startup/load/enable/stop succeeded, SQLite created, but **actual player interactions to SQLite and GUI/API remain unverified**.

**Acceptance: NOT_READY.** 9 categories / 77 CUSTOM keys remain catalog/implementation targets, not a 96-item gameplay pass.
## 2026-11-07追加：独立C ABIの実呼出とページ契約（API実機受入は未達）

C11/C++20公開ヘッダ `core/stats_api.h`、総件数/has_moreを同じSQL読取から返す
`StatsReader::ranking_page`、独立C EXE→4つのGetProcAddress→core専用DLL→SQLiteの
動的境界テストを追加。`./scripts/verify_native_stats_api.ps1` PASS
（`.build/c-api-tests/run-65ca73b9bcd0456ba28b0994ae0f0a7b/`）。
全DBバイト不変・closed integrityok・JSONエスケープ・空/最終/範囲外ページを確認。
これはBDSを起動しないfixtureであり、4 APIの実BDS/他Mod/RemoteCall受入を昇格しません。
96 mandatory entriesの実操作受入は引き続き0、CUSTOMはCODE_ONLY72/UNIT_ONLY4/MISSING1。

以前の「同等機能C ABI」の記述は**wire互換を意味しません**。上流のLK-Stats namespace/
UUID文字列/整数enum/map/1始まりと現行のCシンボル/名前または内部ID/文字列カテゴリ/
JSON/0始まりは異なります。LegacyRemoteCall0.23 SDK/ライセンス確認と別Mod実呼出は
未了です。使用法・安全境界・差分は [NATIVE_STATS_API.md](NATIVE_STATS_API.md)。
実BDS候補ビルドの証跡はSTATUSの当日項目を参照。PROJECT_STATUS: IN_PROGRESS。
