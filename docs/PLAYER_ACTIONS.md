# 行動統計：使用true応答・直接攻撃の死亡帰属・ジャンプ加速・拾得差分ほか

## 2026-11-07：全機能要求が優先

以下は限定的な観測アダプタの履歴で、LK全機能相当の完成を意味しません。
[9分類・77キーのマトリクス](LK_FULL_PARITY_MATRIX.md)とSTATUS.md先頭が現行の判断基準です。
新しいネイティブ読取は未終了セッションを無期限に伸ばさず、CUSTOMの異なる単位を合算しません。
open/eat/playの前処理通知、攻撃回数や処理前ダメージ量は確定CUSTOM実績から除外。
元イベントは保持します。ゲーム内フォームの前後ページ・CUSTOMキー選択、日本語77ラベルを
追加しましたが、実プレイヤーの操作・新候補からのSQLite/GUI/API検証は未受け入れです。

## 第8チェックポイント：成功したブロック採掘・設置（同座標の状態変化。実機BDS未検証）

新規に `src/bedrock_insight/levilamina/player_block.{hpp,cpp}` と
`src/bedrock_insight/core/block_observation.hpp` を追加しました。公式26.60の
`GameMode::$destroyBlock(BlockPos const&, uchar)` と
`GameMode::$buildBlock(BlockPos const&, uchar, HandSlot, bool)` をLowestで囲みます。
`ll/api/memory/Hook.h` の指示どおり仮想関数は `$` 接頭辞のthunkをhookします。

- 入口: 型付き `GameMode::mPlayer`（`Player&`）をLLの型確認 `Player::tryGetFromEntity` と同一性照合。
  UUID不明・座標異常・対象外Handは除外。`Player::getDimensionBlockSource().getBlock(pos).getTypeName()`
  で入口の名前空間付きブロックIDを値コピーします（`BlockSource::getBlock` は公式virtual、
  型名はinline accessor。新しいoffset/仮想slot/パケット解析は使いません）。
- originは必ず1回。true復帰後、公式弱参照から同じ未削除Player・同次元を再確認し、同じ座標を再読込します。
  **mined**: 入口が空気でなく、処理後の型が入口と異なる場合だけ、入口の型を成功採掘として1回記録。
  **placed**: 処理後の型が空気でなく入口と異なる場合だけ、処理後の型を成功設置として1回記録。
  同型のまま・空気・false・例外・入れ子・消滅/別Actor/次元変更は除外します。
- event_kind=`block_mined_result` / `block_placed_result`、cause=`ll26.60.gamemode_destroy_block.v1` /
  `ll26.60.gamemode_build_block.v1`、amount=1、primary_type=採掘前/設置後の名前空間ID。
  placedのsecondary_typeは`main_hand`/`off_hand`。detailsはhook/phase/scope/before_type/after_type/
  dimension/nested/state_transition_verified/world_save_verifiedと、placedのみhand/sim_tickを保存。
- **制限:** `GameMode::destroyBlock`を通らない経路（クリエイティブ即時破壊等）や他Modの処理は欠測し得ます。
  ドア等の複数ブロック設置は主要座標の1個のみ。ドロップ品・素材個数・NBTは含みません。
  全破壊/設置数やワールド保存の証明ではなく、実プレイヤーによる実機BDS確認も未実施です。
- schemaは変更せず既存eventsへ追記のみ。単一writer・有界キュー・起動世代/pass-throughを継承。
  既存の破壊/設置**試行**（audit_block_change）とは別sourceで、成功集計へ混ぜません。

コミュニケータ（`companion/block_source.py`、`analytics.py`、`web/actions.{html,js}`）は
`blocks_mined` / `blocks_placed` の種類別上位100、選択Player、JST日/月/全期間ランキング
（`mined_blocks` / `mined_block_actions` / `placed_blocks` / `placed_block_actions`）と日本語の定義を表示します。
未記録は0と断定せずnull（不明）です。

証拠（この変更で実行）:
- 公式pinned 26.60ヘッダ・キャッシュ済みclang-clオプションで `player_block.cpp`、`runtime.cpp`、`event.cpp` の
  コンパイルPASS（オブジェクトのみ）。
- prelinkを新オブジェクト込みで実行し、`?$destroyBlock@GameMode...`、`?$buildBlock@GameMode...`、
  `?getDimensionBlockSource@Actor...` が `bedrock_runtime_api.lib` に解決されることを確認。
  `scripts/validate_server_release.ps1` の遅延import許可リスト（最大14・固定名）と組合わせ検査を更新。
- `tests/block_observation_tests.cpp` PASS、Python全95件PASS（新規4件、実HTTP401/200→日本語DOM含む）、
  fake SDKのnative runtimeテストPASS。新しい完成DLL候補の作成・配置・BDS起動は行っていません。



最新候補F9D1は `.build/server-candidate/run-d0b293d34d2f42c4bb7e918f9bab131e/BedrockInsight.dll`。
Python75件・公式26.60の10翻訳単位・DLLリンク・包装・40 LL import解決がPASS。
**新候補のstage／BDS起動／実行動は未検証**。旧18767へ適用していません。
使用試行・true応答・主所持品減少・消費成功を分けた[最新の定義とハッシュ](#use-results)を参照してください。
第6以下は履歴です。全項目の実機検証済みを意味しません。

追加の受け入れチェック実装後はPython全85件がPASS。通常ホストで本物の行動を実施した後の
生イベント／観測メモ／認証API／JST日・月・全期間／実日本語JSを照合する手順を
[通常ホスト検証](PLAYER_ACTION_HOST_VALIDATION.md)に追記しました。fixtureのPASSを実機結果へ
流用しません。ネイティブ48ファイルのハッシュとF9D1候補は不変、再ビルド・配備なしです。
クラフトは[API調査](CRAFT_API_FINDINGS.md)の完了順序が未確定であり、成立数を有効化していません。

## 第6チェックポイントの履歴：直接攻撃sourceの死亡フラグ成立とPlayer帰属

当時の候補は `.build/server-candidate/run-e7cd517e08a34462a57df10916cf1ce0/BedrockInsight.dll`、
SHA256 `E1940880DFB943D6CBB8DA7B399999F8F9F5A1902F4F3508CE85CEC31088A6C3`。
公式26.60の9翻訳単位、prelink、40 LL import解決、DLLリンク・包装がPASS。
45件のソース／ヘッダーハッシュも一致。**この候補はBDS未配置／未起動です。**

`Mob::$die` のLowestで、同じ未削除被害者の死亡フラグが処理前false→正常復帰後trueに
変わった場合だけ記録します。帰属は入口のDamageSourceが次の全条件を満たす場合に限定します。

- 固定26.60のEntityAttack(2)、entity sourceであり、child／block sourceではない。
- getEntityUniqueIDとgetDamagingEntityUniqueIDが非0で一致。同じ被害者LevelのfetchEntity(id,false)で
  解決でき、公式の型確認付きPlayer::tryGetFromEntityから同じ実Playerを取得できる。
- 攻撃者が被害者自身ではなく、UUIDが利用可能。表示名ではなくUUIDで帰属する。

**全キル数・飛び道具や間接攻撃の所有者帰属・真のキラー・永続死亡の証明ではありません。**
この観測は「その処理入口で報告された直接攻撃sourceのPlayer」に帰属します。
環境死・飛び道具・間接source・不明／不一致は除外し、最後の攻撃者やgetOwnerから補完しません。
EntityAttack以外（MaceSmash等）も未収集です。特にPlayer死亡がこのMob経路を通らない場合は欠測します。

- 被害者が入口ですでに死亡フラグ成立済みなら数えません。入れ子は内外とも除外。
  originは常に1回。処理後はDamageSource・攻撃者・旧被害者を参照せず、公式弱参照から同じActorを
  再取得し、同一性を先に確認してから死亡フラグを読みます。消滅・別Actor・未成立は欠測です。
- 攻撃者名・ID、被害者種類は入口で値コピー。被害者は公式Player判定と型名を照合し、
  `player` / `mob` を区別します。種類は入口の名前空間付きID、UUID等の被害者識別情報は保存しません。
- event_kind=`direct_kill_observation`、cause=`ll26.60.direct_attack_dead_transition.v1`、
  amount=1、actor=帰属Player、primary_type=入口被害者の種類、secondary_type=`player|mob`、
  時刻=処理後UTC、session_id=計測起動世代。detailsはhook/phase/原因2と厳密な判定フラグ。
  world_death_verified=falseを保持し、既存の死亡処理前通知や被害者側死亡数とは別sourceです。
- schema4・migration・単一writerは変更しません。既存の全キル成功用集計へ混ぜず、hook内SQLiteもなし。
  有界キュー、pinned pass-through disable、in-flight所有権、weak Capture／起動世代を継承します。
- 新しい `/actions` の `direct_kills` / `direct_mob_kills` / `direct_player_kills` に、
  JST日／月／全期間の正のランキング、選択Player、入口被害者種類上位100、最初／最後、定義を表示。
  片方だけの観測から他方の0を推測せず、旧DLL・未収集期間はnull（不明）です。

公式pin `072b9ffe0e9d9e0ec846f11d294880e892fe7770` の
[MobDie publisher](https://github.com/LiteLDev/LeviLamina/blob/072b9ffe0e9d9e0ec846f11d294880e892fe7770/src/ll/api/event/entity/MobDieEvent.cpp)
はorigin前通知であり、それ自体をキルにしません。
[DamageSource](https://github.com/LiteLDev/LeviLamina/blob/072b9ffe0e9d9e0ec846f11d294880e892fe7770/src/mc/world/actor/ActorDamageSource.h)、
[Level](https://github.com/LiteLDev/LeviLamina/blob/072b9ffe0e9d9e0ec846f11d294880e892fe7770/src/mc/world/level/Level.h)
のconst virtual宣言を通常のC++ dispatchで使用し、MCNAPIのgetEntityUniqueID／fetchEntity thunkや
推測したslot・offsetを使いません。新しいMCAPIは実コンパイルで確認したMob::$dieだけです。
LL Actor::getTypeName、isDead、型確認・弱参照APIを使い、health／吸収量／未対応ABSORPTIONは読みません。
Level/Source virtual ABI、同一被害者の死後weak解決と実際の呼出し経路は実機試験が必要です。

このsourceでdeathフラグが解除されて再成立した場合は新たな観測であり、任意Mod下での物理的死亡の
完全な一意性を保証しません。消滅が早い場合や別の死亡経路は保守的に欠測します。
全70 Pythonテスト（新規5件、実HTTP401/200→日本語DOM）、C++の32状態組合せ・帰属除外・
再呼出し／respawn／例外、fake SDKの161行drain・起動失敗・旧世代拒否がPASS。
候補・テスト証拠はSTATUS.md最上部、[通常ホスト手順](PLAYER_ACTION_HOST_VALIDATION.md)に従い、
旧smoke／実MSPTを新しい行動の実行証明に流用しないでください。既存BDS／18767は未変更です。

## 第5チェックポイントの履歴：ジャンプ処理の上向き加速観測（実機未検証）

当時の候補は `.build/server-candidate/run-146e37a42ac749b2b2e4794fbb5af9fb/BedrockInsight.dll`、
SHA256 `7AA2CAB4501E7AA93EC5E47957E37ED3AE6E76FFE436B96F5B9ADE61D687CB66`。
公式26.60の8翻訳単位、prelink、39 LL import解決、DLLリンク・包装がPASS。
42件の記録済みソース／ヘッダーハッシュも一致。**この候補はBDS未配置／未起動です。**

公式MCAPI `Mob::jumpFromGround()` をLowestで囲み、Playerの直接の状態ベクトル
`getPosDelta().y` が正常復帰後に正の値へ増加した場合だけ記録します。
**キー入力通知数・空中へ移動したジャンプ完了数・移動距離ではありません。**

- 入口は公式LL Player::tryGetFromEntityによる型確認と同一Mob照合を行い、UUID不明を除外。
  接地中・非乗車・非死亡のPlayerだけを対象に、弱参照・同一性・次元・Y成分をコピーします。
- originは必ず1回。処理後は旧Mob/Playerを参照せず、弱参照から同じ未削除Playerを再取得。
  同一次元・非乗車・非死亡、正のY成分が入口より増加した場合だけamount=1。
  空中からの呼出し、失敗／例外、同値の再呼出し、入れ子、消滅／別Actor、乗車・次元変更は除外。
  処理直後の接地フラグがまだtrueでも、空中移動を証明したとは表示しません。
- 状態Y成分は×1,000,000に丸めた整数で比較。非有限値と絶対値16超を除外し、
  丸めで同値になる小さい変化も欠測します。この閾値を超えるMod独自ジャンプは未計測です。
  状態成分を移動距離やTPSの時間単位に換算しません。
- event_kind=`player_jump_impulse`、cause=`ll26.60.jump_pos_delta.v1`、amount=1、
  primary_type=`minecraft:player`、時刻=処理後のUTC観測時刻、session_id=計測起動世代。
  detailsにhook/phase/measurement、grounded_before=true、vehicle_before/after=false、
  before_y_micro/after_y_micro/dimension、world_jump_verified=falseを保存。
- 同じ呼出し内の別Modの状態変更も含み得ます。この関数を通らない経路や他Modの変更による欠測は
  復元できず、無記録は不明です。全ジャンプやクライアント操作と1対1に対応する保証はありません。
- schema4・有界キュー・単一writerを保持し、hook内でSQLiteを呼びません。
  pinned pass-through disable、in-flight token、weak Capture／起動世代で古いcallbackを拒否します。
  新しい `/actions` の `jump_impulses` でプレイヤー値、JST日／月／全期間ランキング、
  最初／最後・対象・ソース定義・未計測状態を表示します。元18767番／旧静的ファイルは更新しません。

公式pin `072b9ffe0e9d9e0ec846f11d294880e892fe7770` の
[Mob宣言](https://github.com/LiteLDev/LeviLamina/blob/072b9ffe0e9d9e0ec846f11d294880e892fe7770/src/mc/world/actor/Mob.h)、
[ActorのgetPosDelta/getVehicle](https://github.com/LiteLDev/LeviLamina/blob/072b9ffe0e9d9e0ec846f11d294880e892fe7770/src/mc/world/actor/Actor.h)、
[LL isOnGround / getVelocity実装](https://github.com/LiteLDev/LeviLamina/blob/072b9ffe0e9d9e0ec846f11d294880e892fe7770/src/mc/world/actor/Actor.cpp)
を確認しました。isOnGroundはOnGroundFlagComponentの確認です。
**getVelocityはMob/乗物ではpost-tick成分を読むため、この即時加速の計測に使いません。**
getPosDeltaは同pinの公開inline getterで、推測offsetや未対応isJumping MCNAPIを使いません。
typed PlayerJumpEventは処理後のpacket条件による通知と取消teleportなので、この成功側の数に流用しません。
新しいMCAPI importは実コンパイルで確認したjumpFromGround/getVehicleのみを厳密に許可します。

実エンジンでこの経路が発火すること、state-vector/weak解決、実行動→SQLite→画面は未検証です。
全65 Pythonテスト（新規5件、認証HTTP→日本語DOM）、C++の接地／乗車／死亡／次元／同一性／
入れ子／丸め・不正値／再呼出し・例外、fake SDKの159行drain・起動失敗・旧世代拒否がPASS。
候補・ビルド／テスト結果はSTATUS.md最上部、次の実機試験は[通常ホスト手順](PLAYER_ACTION_HOST_VALIDATION.md)。
既存の実MSPT受け入れや旧投棄smokeを、新しい行動の実行証明には流用しません。

## 第4チェックポイントの履歴：拾得処理中の主所持品増加（実機未検証）

当時の候補は `.build/server-candidate/run-1f019fef719e4b40b1659bcf7bb92b2a/BedrockInsight.dll`、
SHA256 `5FC02F422CBD84A70A1D34BCB478E9D85CF3305DB176BECABE16303A038B7FCF`。
公式26.60の7翻訳単位コンパイル、prelink、38 LL import解決、DLLリンク・包装がPASSです。
39件の記録済みソース／ヘッダーハッシュも一致。**この候補はBDS未配置／未起動です。**

`Player::take(Actor&, int, int)` のtrue戻り値に加え、同じプレイヤーの同じ主インベントリで、
同種類アイテムの合計所持数が処理前後に増えた場合を独立実装しました。**入口64個でも差分3個なら3個**です。
処理前のPlayerPickUpItemEventや入口個数を、そのまま成功した拾得個数にしていません。

- 入口のItemActorは公式LLの型確認付き `tryGetFromEntity(..., false)` で取得。
  型名・入口個数・UUID・名前・主インベントリの型別合計・枠数・弱参照を値コピーします。
  ItemStackやItemActorの参照をorigin後に読みません。orgCount/favoredSlotは変更しません。
- originを1回呼び、false／例外時は増加観測を作りません。計測側の例外はengineの返値・例外を変えません。
  入れ子のtakeは内外とも集計しません（差分の二重帰属を避ける下限）。
- 正常true復帰後、公式弱参照から同じ未削除Playerを再取得し、主インベントリの同一性・枠数を確認。
  同じ型名の所持数差分が1〜入口個数（最大255）のときだけ記録します。
  0／減少／入口より大きい増加、容器交換、サイズ変更、消滅・別Player、不明は除外します。
- 主インベントリ最大128枠だけを各2回以下走査。offhand／装備／カーソル／開いた容器は対象外です。
  NBT・耐久性等を区別せず型名ごとに合計します。他Modによる同じ型の増減も含み得るため、
  **主所持品の純増観測であり、全拾得数・入力操作数・特定の地面アイテムの転送因果・永続保存の証明ではありません。**
- event_kind=`item_pickup_delta`、cause=`ll26.60.take_inventory_delta.v1`、amount=所持数差分、
  primary_type=名前空間付きitem ID、時刻=処理後観測時刻、session_id=計測起動世代。
  detailsにhook/phase/scope、entry_units/slots/before_units/after_units、nested=false、
  durable_transfer_verified=falseを保存します。readerは型・範囲・差分一致・sourceを厳密検査します。
- schema4・既存migration・単一writerを保持し、有界キューを利用。処理内でSQLiteを呼びません。
  キュー拒否やクラッシュによる欠落は復元せず、未収集を0にしません。
- 新しい `/actions` の `pickup_inventory_items` / `pickup_delta_actions` は個数と観測回数を分離し、
  JST日／月／全期間、選択プレイヤー、正のランキング、種類上位100、最初／最後と定義を表示します。

公式pin `072b9ffe0e9d9e0ec846f11d294880e892fe7770` の
[Player::take宣言](https://github.com/LiteLDev/LeviLamina/blob/072b9ffe0e9d9e0ec846f11d294880e892fe7770/src/mc/world/actor/player/Player.h)、
[PlayerのLL getter](https://github.com/LiteLDev/LeviLamina/blob/072b9ffe0e9d9e0ec846f11d294880e892fe7770/src/mc/world/actor/player/Player.cpp)、
[ItemActorの型確認と読み取り](https://github.com/LiteLDev/LeviLamina/blob/072b9ffe0e9d9e0ec846f11d294880e892fe7770/src/mc/world/actor/item/ItemActor.cpp)、
[Containerのconst virtual API](https://github.com/LiteLDev/LeviLamina/blob/072b9ffe0e9d9e0ec846f11d294880e892fe7770/src/mc/world/Container.h)、
[FillingContainer](https://github.com/LiteLDev/LeviLamina/blob/072b9ffe0e9d9e0ec846f11d294880e892fe7770/src/mc/world/inventory/FillingContainer.h)
を確認しました。getItem/getContainerSizeは公式宣言を通常のC++ virtual呼出しで使用し、
MCNAPIの `$getItem` / `$getContainerSize` thunk、推測した仮想slot、NBT変更を使いません。
通常の初期化済みPlayerでLL getInventoryが有効なこと、実エンジンのvirtual/weak解決や他Modとの組合せは
新しい候補の実機試験で確認が必要です。ビルドだけでその境界をPASSとはしません。

全60 Pythonテスト（新規5件、実HTTP401/200→日本語DOM）、C++の部分差分／上限／容器変更／
origin順序・例外、fake SDKの158行drain・起動失敗・古い世代拒否がPASS。
候補ハッシュとビルド証拠はSTATUS.md最上部、次の実機検証は[通常ホスト手順](PLAYER_ACTION_HOST_VALIDATION.md)。
**実プレイヤーの拾得差分はまだ未観測です。既存BDS／18767番／旧静的ファイルは更新しません。**

## 第3チェックポイントの履歴：遅延したモードフラグ観測（実機未検証）

当時の候補は `.build/server-candidate/run-1e41eb1abef443dfbdae838a6bc33031/BedrockInsight.dll`、
SHA256 `01FCBC46371F7D090ABAAB132DE85E95AD2E8535526677883AEB82CD50780D8D`。
公式26.60の6ネイティブ翻訳単位、prelink、34個のLL import解決、DLLリンク・包装がPASSです。
**この候補はBDSへ配置／起動していません。投棄・死亡・モード変化の実プレイヤー観測は未検証です。**
旧候補のsmokeや実MSPT受け入れを、この候補・行動フックの実行証明に流用しません。

公式の開始／終了イベントは処理前通知なので、その通知自体は数えません。
通知時のサーバーフラグと要求された状態を保存し、250ms以降〜2秒以内に、同じ未削除
エンティティを公式弱参照で再取得してサーバーの `isSprinting()` / `isSneaking()` を読みます。
入口と要求が異なり、遅延したフラグが要求と一致した場合だけ観測を数えます。
取消済みスニーク通知、消滅／別エンティティ、期限切れ、不一致は除外します。

- **短い開始→終了は合流して欠測します。** 重複通知で期限を延長せず、通知数・キー入力数・
  正確な全切替数・特定の通知が状態変更を起こした因果証明ではありません。他Modの変更も含み得ます。
- 最大256人、観測は最大4Hz、各人に2個の保留状態と4個の固定カウンター。
  高頻度の通知からSQLiteへ書かず、5秒以上ごと／JST日替わり／退出／無効化で既存有界キューへ集約。
  上限を超えた人は未計測。短時間の切替や容量超過を架空の0で埋めません。
- 退出・停止時は**既に確認できたカウンターだけ**をflushし、保留入力から新しい変化を推測しません。
  不意の終了では未flush分（通常約5秒、tick停止中はそれ以上）が失われ得ます。
  キュー拒否分は再送しないため二重計上せず、既存writerの欠落診断に従います。
- JST日をまたぐ集約は作りません。時刻はflush時刻ではなく最後の実観測時刻で、detailsに最初／最後と
  JST日bucketを保存します。日／月ランキングはこの**観測日**基準です。UTC保存時刻は変更しません。
- event_kind=`player_mode_observation`、cause=`ll26.60.mode_flag_observed.v1`、
  primary_type=`sprint|sneak`、secondary_type=`start|stop`、amount=確認数の集約。
  detailsのphase=`delayed_flag_observation`、poll_us=250000、flush_us=5000000、
  coalesced=true、duration_measured=false、first_ms/last_ms/jst_dayを厳密に検査します。
  schema4は変更せず、成功した移動距離や既存の別集計へ混ぜません。
- `/actions` に4指標（`sprint_starts` / `sprint_stops` / `sneak_starts` / `sneak_stops`）の
  プレイヤー別値、JST日／月／全期間ランキング、モード内訳、最初／最後、ソース定義を表示します。
  **継続時間・距離・現在のモードは未計測**。片方だけ記録されても他方は0にしません。

全55 Pythonテスト（新規5件、実HTTP401/200→日本語DOMを含む）、C++の遅延／合流／TTL／
JST境界／flush／消滅／上限256人テスト、fake SDKの11 listener lifecycleと157行drainがPASS。
これは実際の26.60エンジン内での成功証明ではありません。
[通常ホスト手順](PLAYER_ACTION_HOST_VALIDATION.md)で実行・保存・表示を確認する必要があります。
既存18767番／元HTML・JS・CSSは未変更です。新しい画面の起動は下記「画面と既存プロセスの保護」を参照。

公式pin `072b9ffe0e9d9e0ec846f11d294880e892fe7770` の
[typed通知publisher](https://github.com/LiteLDev/LeviLamina/blob/072b9ffe0e9d9e0ec846f11d294880e892fe7770/src/ll/api/event/player/PlayerSneakAndSprintEvent.cpp)、
[Actor accessor](https://github.com/LiteLDev/LeviLamina/blob/072b9ffe0e9d9e0ec846f11d294880e892fe7770/src/mc/world/actor/Actor.h)、
[LL実装](https://github.com/LiteLDev/LeviLamina/blob/072b9ffe0e9d9e0ec846f11d294880e892fe7770/src/mc/world/actor/Actor.cpp)
を確認。LL `getStatusFlag` と公式inline wrapperを使い、新しいMCAPI importはありません。
MCNAPIのsetSneaking/setSprintingはhookせず、packetの直接interceptionやoffset推測もしません。

## 第2チェックポイントの履歴：死亡フラグ成立（実機未検証）

当時の候補は `.build/server-candidate/run-ef291d651a05440bb742ed4d8f0ef5ea/BedrockInsight.dll`、
SHA256 `80BDA5594F4153C7A039F51F80FA499CC6D7B4A8C8311E7F1247A82C734DF41A`。
公式26.60の5ネイティブ翻訳単位をコンパイル、prelink、33個のLL import解決、DLLリンク済みです。
**この新候補はBDS未起動で、実プレイヤーの死亡/投棄データは未観測です。**
下の95FE候補の有効化済みsmoke結果を、この新候補の動作証明に流用しません。

既存のPlayerDieEventは処理前通知のまま残し、新たに `Player::$die` をLowestで囲みます。
入口で公式LL `Actor::isDead()` がfalse、正常復帰後に**同じ未削除エンティティ**のisDeadがtrueに
変化した場合だけ1回記録します。isDeadは同pinのIsDeadFlagComponent存在確認であり、
health値や未対応の `Actor::$isAlive` MCNAPIから推測していません。

- 入口でUUID/表示名/原因コードとEntityContextのWeakEntityRefをコピー。
  origin後は元Playerを参照せず、公式WeakEntityRefの再取得と同一Actorポインターの比較を使います。
  registry/entity generationが無効、削除済み、別エンティティ、フラグ未成立なら記録しません。
- originは必ず1回。例外時は成立数を増やさず、engineの例外は保持。
  観測側のエラーはゲーム処理を中断しません。入れ子のdieは保守的に除外します。
- 既に死亡フラグ成立済みのdie再呼び出しは数えません。respawn後にフラグが解除され、
  次に新しく成立した場合は別の観測です。消滅や途中の別Mod変更による欠測は復元できません。
- event_kind=`player_death`、cause=`ll26.60.player_dead_transition.v1`、amount=1。
  primary_type=`minecraft:player`、secondary_type=`cause:<入口のActorDamageCause数値>`。
  -1/未知は「不明／未指定」。コード35(All)等の範囲外値は-1へ落とします。
  飛び道具・攻撃のコードがあっても、誰が倒したかを推測しません。
- details_jsonのhook/phase/前false/後true/帰属未計測を正確に検査してAPIへ渡します。
  既存PlayerDeathのlegacy行や処理前通知、異なるsource版、amount不正はこの指標に含めません。
- 既存schema4と単一writerを継承。player_daily.deathsへ成立観測だけを反映し、
  APIの `confirmed_deaths` は上記source限定です。未収集期間はnull。オンライン/現在の生死は推定しません。

新しい `/actions` に「死亡フラグ成立（処理後）」のプレイヤー/全体値・日/月/全期間ランキング・
入口原因別内訳・最初/最後の観測・ソース定義を表示します。処理前通知とは別欄です。
**ワールドへの死亡結果保存、独立した物理的死亡回数の完全捕捉、キラー帰属を保証する指標ではありません。**
全50 Pythonテスト（新規5件、実HTTP401/200→日本語DOM含む）、32通りの境界/例外/再呼出し/
respawn条件とfake SDKのwriter/lifecycle156イベントの検証がPASS。実機での成立確認は別途必要です。
[通常ホストでの次の検証手順](PLAYER_ACTION_HOST_VALIDATION.md)を使い、古いrootを再利用しないでください。

公式の根拠はpin `072b9ffe0e9d9e0ec846f11d294880e892fe7770` の
[Actor::isDead](https://github.com/LiteLDev/LeviLamina/blob/072b9ffe0e9d9e0ec846f11d294880e892fe7770/src/mc/world/actor/Actor.cpp)、
[WeakEntityRef](https://github.com/LiteLDev/LeviLamina/blob/072b9ffe0e9d9e0ec846f11d294880e892fe7770/src/mc/deps/ecs/WeakEntityRef.h)、
WeakStorageEntity/StackResultStorageEntity/EntityContext/ActorDamageCauseの同じ公式ヘッダーです。
新しいMCAPIはdieと弱参照の公式StackResultStorageEntity constructorのみ。
Playerの生ポインターや未知の仮想slot、被害者/キラーのoffset解析はしません。

## 第1チェックポイント（95FE smokeの履歴）

2026-11-06。MSPTは通常ホストの別BDSで受け入れ済みです（STATUS.md最上部の証拠参照）。
ここでは新しい行動ソースを追加します。**新しい行動を実機で観測した証拠はまだありません。**
既存18767番のPythonプロセス、既存DLL、過去のworld、SQLiteは更新していません。

ホスト側ビルド候補は `.build/server-candidate/run-302dfbce990d48f189a76d34729779b7/BedrockInsight.dll`、
SHA256 `95FE8E3B6E84FDE206DE8CD94F07C2416BB1CC4A2C74C74034B8B8944C40B45E`。
新規隔離root24156でload/enable・正常stop0を確認済みですが、プレイヤーなしのsmokeです。
同rootと769103/b63eは使用済みで再利用しません。13:20のソースハッシュ照合は一致しました。
45件のPythonテスト（新行動5件・実HTTP401/200→日本語DOMを含む）、C++境界テストと
fake SDKによるwriter/lifecycleテストはPASS。実投棄のSQLite保存は未検証です。

## 投棄の定義

公式LL26.60.0の `Player::$drop(ItemStack const&, bool)` をLowest優先度で囲みます。
非randomly呼び出しの入口でUUID、名前、型名、スタック個数を値コピーし、originを1回呼びます。
**originがtrueを返した場合だけ**既存の有界キューへ投入し、単一SQLite writerが保存します。
false、例外、randomly=true、UUID不明、0個、不正な型名は記録しません。
参照はorigin後に読みません。計測側の例外はengineの戻り値・例外を変更しません。
再帰中の入れ子呼び出しは数えず、二重計上を避ける保守的な下限です。

- event_kind=`item_drop_result`、cause=`ll26.60.player_drop_returned_true.v1`。
- amount=**入口スタック個数（1〜255）**。回数とは別です。UTC時刻も入口時点です。
- primary_type=namespaced item ID。128文字以下、namespace/pathを構文検査。NBTや素材の状態は保存しません。
- details_jsonに`phase=return_true`, `randomly=false`, `quantity=entry_stack_count`,
  `inventory_delta_verified=false`。session_idはModの起動世代IDです。
- **最終インベントリ差分・地面アイテム生成の永続化・クライアント操作との1対1対応を保証しません。**
  他Modの返値変更もあり得ます。「投棄 true 戻り値・入口個数」と表示します。
- schema v4のeventsのみを拡張利用します。既存enum末尾へ追加し、既存集計列・migrationは変更しません。
  kills/items_used等の成功集計に混ぜません。SQLiteリプレイのevent_id冪等性と停止drainは既存writerを使います。
- DLLは既存のload時にpin。hook登録の失敗はenable失敗、disable後はpass-through。
  hookをdetach時に外さず、in-flight所有権と起動世代で古いcallbackの書込みを拒否します。

APIは正の観測だけを集計し、無記録はnullです。JST日/月/全期間、個数/回数ランキング、
プレイヤー選択、種類別上位100件（個数降順→型名順）を提供します。
最初/最後の観測時刻は連続収集期間の証明ではありません。過去DLLの欠測を0で埋めません。

## 画面と既存プロセスの保護

新しいプロセスだけが `http://127.0.0.1:18768/actions` を提供します。
基本画面 `/` のHTML/JS/CSSは変更せず、新しい静的2ファイルを追加しています。
既存18767番は古いPythonコードをメモリに保持しており、新ルートを持ちません。再起動しないでください。

```powershell
# 既存BDS/DBを変更しない。新しい承認済みBDSのDBまたは私有テストコピーだけを指定。
python -m companion.server --database <承認済みDBパス> --port 18768
```

新しい起動端末に出るランダムトークンで認証します。18767番のトークンは使いません。
HTTPのHost/Origin検査・認証・GETのみ・query_only・127.0.0.1固定は既存実装のままです。
主PCは既存の私設SSHポート転送のみ。LAN/Tailscale/public bindやセキュリティ設定変更はしません。
フィクスチャの64個等は実プレイヤーの観測ではありません。

## 他のイベントを成功扱いしない理由

公式ソースpinは全て `072b9ffe0e9d9e0ec846f11d294880e892fe7770`。

| 項目 | 確認した公式publisherの意味 | この段階 |
| --- | --- | --- |
| 拾得 | typed通知はPlayer::takeのorigin前、取消可能。入口orgCountは移動成功個数ではない | 上記のtrue復帰＋主所持品差分を別sourceで追加。全拾得・永続転送は未計測 |
| ジャンプ | typed通知はAuthInput処理後のpacket条件、取消時teleport | 上記の別MCAPI＋直接状態による加速観測を追加。空中移動完了数・距離は未計測 |
| スプリント | PlayerAction packetのorigin前に開始/停止通知 | 上記の遅延フラグ観測を追加。全切替・時間・距離は未計測 |
| スニーク | 同じ処理前通知、取消可能 | 同上。取消済みは除外し、通知自体を成功数にしない |
| 死亡/使用 | 既存PlayerDieEvent/PlayerUseItemEventは処理前 | 通知/試行を保持。新しい死亡フラグ成立は別source（上記）、使用成功は未計測 |
| kill | 上記の直接攻撃source一致＋死後フラグだけを独立実装 | 全キル／飛び道具等は未計測、帰属を推測しない |
| craft/travel/break | 成功commit・teleport/容器/耐久性の追加検証が必要 | 未計測。架空の0を表示しない |

クラフトについては固定版の
[参照箇所](https://github.com/LeafKnife/Stats/blob/5bb8bfb7e92c28e03bdd02d93ba7cc8b6c15bb40/src/mod/Hook/ItemStackRequestActionHandler.cpp)
のCreatedOutputContainerからのtransferとItemStackNetResultを調べました。
公式26.60の `_handleTransfer` のbool引数はisSrcHintSlot/isDstHintSlot/isSwapで、
参照実装の変数名をABIの意味として採用しません。transferのSuccessだけでは、後続のscreen終了等を
含む要求全体のcommitを保証できるか未検証です。`_tryGetCurrentScreenData` / tryCraftや
request queue処理にはMCNAPIが残り、観測のために `_getOrInitSparseContainer` を呼んで
容器を生成する方法も使いません。**完全成功を確認する副作用のない経路が未確認のため未実装**です。
これは公式宣言からの保守的な判断で、上流が必ず誤集計すると断言するものではありません。

使用／破損も、公式Player::$useItem（void）やItemStackBase::hurtAndBreak（bool）の宣言はあります。
consumeItem引数や入口のdamage+deltaだけを、実消費／破損とみなしていません。
処理後に同じ所有者・アイテム状態を安全に再取得できる経路と呼出し時点を検証する必要があり、
耐久減少・使用試行・実消費・実破損を混同しない追加実装は引き続き未完了です。

公式の確認先：
[拾得](https://github.com/LiteLDev/LeviLamina/blob/072b9ffe0e9d9e0ec846f11d294880e892fe7770/src/ll/api/event/player/PlayerPickUpItemEvent.cpp)、
[ジャンプ](https://github.com/LiteLDev/LeviLamina/blob/072b9ffe0e9d9e0ec846f11d294880e892fe7770/src/ll/api/event/player/PlayerJumpEvent.cpp)、
[スニーク/スプリント](https://github.com/LiteLDev/LeviLamina/blob/072b9ffe0e9d9e0ec846f11d294880e892fe7770/src/ll/api/event/player/PlayerSneakAndSprintEvent.cpp)、
[ItemStackBaseのLL accessor](https://github.com/LiteLDev/LeviLamina/blob/072b9ffe0e9d9e0ec846f11d294880e892fe7770/src/mc/world/item/ItemStackBase.cpp)。
型名はLLのgetTypeName、個数は同pinの公開uchar mCountをコンパイラの型経由で読みます。
独自offset、仮想slot、packet interception、MCNAPI accessorは追加しません。

## ライセンスと検証境界

ユーザー指定の[LeafKnife/Stats v0.13.0](https://github.com/LeafKnife/Stats/tree/5bb8bfb7e92c28e03bdd02d93ba7cc8b6c15bb40)
のEvents.cpp、PlayerDropItem.cpp、ItemStackRequestActionHandler.cpp、PlayerCombatStatsHandler.cppで、
利用イベント・返値・処理順を確認する着眼点だけを調べました。
同版はAGPL-3.0。ソース、コメント、翻訳、ビルド成果物は本プロジェクトへコピーしていません。
この実装はBedrockInsightの所有権/queue/lifecycleに合わせて独立作成しています。
upstreamの実機成功を本候補の証拠に流用しません。

数学・origin呼出し順/例外境界、fake SDKでの有効化失敗/再開/SQLite drain、
fixture SQL/API/日本語DOMをテストします。実DLLは公式26.60でコンパイル・リンクを別途検証します。
**これらは新行動の実機成功ではありません。** 次の通常ホスト試験では新world/未使用portsで、
実プレイヤーによる1個/複数個投棄、失敗/死亡random dropの除外を観察し、SQLite→認証API→
この画面→正常stop/DB integrityを照合する必要があります。使用済みb63e/既存BDSは再利用しません。
CodexSandboxOfflineではオンラインBDSを起動せず、承認済み通常ホスト経路へ引き継ぎます。

<a id="use-results"></a>

## 使用true応答と主所持品差分（新規候補・実プレイヤー未検証）

最新候補は `.build/server-candidate/run-d0b293d34d2f42c4bb7e918f9bab131e`、
DLL SHA256 `F9D1DA65216DB0F6FC7AAC8148FB6B3E2981C9A33759C6783A260754EAD4959E`。
ZIP SHA256 `958E0368348F034C7387DE9DAB8E731A93B256352751671060DCD7A6829CAC23`。
公式22202ヘッダ・10ネイティブ単位・prelink・DLL/ZIP・40 LL import/961 export照合PASS。
48 source/header hash一致、旧client importなし、SymbolProvider維持。
Python75件、C++差分境界・bool/例外保持、fake runtime162行drain/世代/失敗cleanup、
認証HTTP401/200→日本語DOMと旧4カテゴリDOM PASS。実機BDSへはstage/loadしていません。
前段の旧候補／70件等の記載は当時の履歴で、最新候補の実機証明にはなりません。

独立実装 `player_use.cpp` は公式26.60 `GameMode::$useItem(ItemStack&,HandSlot)`
のLowest wrapperです。型付き `GameMode::mPlayer`、LLのchecked Player変換・UUID、
入口の種類と個数、同じ主インベントリの種類別総個数をコピーします。
処理後はweak参照から同じ未削除Playerを再取得し、同じ容器・同じ1〜128枠を照合。
古いGameMode/Player/ItemStackを処理後に読み直しません。

true応答かつ有効な前後照合だけを `item_use_result`、
`ll26.60.gamemode_use_return.v1`、amount=1として既存SQLite v4へ送ります。
secondary_typeは`main_hand`/`off_hand`（公式HandSlot=0/1）。detailsにはhook/phase/scope、
entry_units/slots/before_units/after_units/loss_units、nested=false、
effect_completed_verified=false、consumption_verified=falseを記録します。
減少はmax(0,前−後)、入口個数以下のみ。入れ子は内外とも除外、例外/false応答/
不正種類/空スタック/Playerや容器の交換・サイズ変更/曖昧な過大減少は除外します。
originは必ず1回、戻り値とengine例外を変えず、観測エラーはengineへ漏らしません。
DLL pin/shared token/disable pass-through・activation generation、bounded queue/single writerを維持。

`/actions`の「使用処理true応答・所持品照合済み」と
「使用true応答中・主所持品の減少個数」は別指標です。種類／手の上位100、
選択Player、JST日/月/全期間の正のSQLランキング、ソースと最初/最後の観測を表示。
試行通知、効果完了、消費成功、全使用数とは区別します。他Modによる所持品変更や
処理開始応答を含み得ます。副手/装備内の減少やNBT別はこの差分に含めません。
減少ゼロ/増加の照合でもtrue応答は記録できますが、減少の正の記録なしはnull（不明）。
観測間の連続収集保証はありません。旧18767は更新せず、許可された別companion起動のみ。

公式ソースのPlayerUseItemEvent publisherは処理前・取消可能でfalseを返すため、
成功には流用しません。Player::$useItemはvoid、consumeItem引数だけから実消費を
推定しません。ItemStackBase::hurtAndBreakの戻り値だけも破損完了の証明にはしません。
破損・クラフト・移動距離は引き続き未計測。AGPL参照は事実調査のみ、コード転載なし。

一次資料：

- [公式GameMode宣言](https://github.com/LiteLDev/LeviLamina/blob/072b9ffe0e9d9e0ec846f11d294880e892fe7770/src/mc/world/gamemode/GameMode.h)
- [公式使用試行publisher](https://github.com/LiteLDev/LeviLamina/blob/072b9ffe0e9d9e0ec846f11d294880e892fe7770/src/ll/api/event/player/PlayerUseItemEvent.cpp)
- [上流v0.13.0の破損hook参考](https://github.com/LeafKnife/Stats/blob/5bb8bfb7e92c28e03bdd02d93ba7cc8b6c15bb40/src/mod/Hook/Item/ItemStackBase.cpp)
