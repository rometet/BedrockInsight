# 耐久処理中の主所持品スロット消失（候補・実ゲーム未検証）

追加するのは**破損確定数ではなく限定的な観測**です。既存の使用試行、使用true応答、
所持品の種類別減少とは別の `item_hurt_slot_loss` イベントとして保存します。
スキーマv4を維持し、履歴の書き換え・再集計や既存DLLの置換はしません。

## 収集条件

公式26.60.0固定ヘッダーの
[ItemStackBase::hurtAndBreak](https://github.com/LiteLDev/LeviLamina/blob/072b9ffe0e9d9e0ec846f11d294880e892fe7770/src/mc/world/item/ItemStackBase.h)
（MCAPI、bool、intとActor*引数）をLowest優先度で包みます。

1. 入口で正のdeltaDamage、未削除Playerへの型検査、非ゼロUUIDを確認。
2. 入口アイテムが1個で、最大128枠の主所持品に**同じオブジェクトの参照**がある場合のみ、
   名前空間付き種類・枠番号・Player弱参照・比較用の同一性を保存。
3. originを一度だけ呼び、正常にtrueを返した場合だけ照合。
4. 弱参照から同じ未削除Playerを再取得し、同じ主所持品・サイズ・スロット参照で
   個数0となった場合だけ1回を保存。origin後に古いowner/thisを参照解除しない。

入れ子は両方の観測を棄却。false・エンジン例外はそのまま返し、観測側の例外は
エンジンの結果を置き換えません。無効化後はpass-through、古いactivationの
callbackは新writerへ入れません。モジュールは既存のpin方針を維持します。
スロット探索1回（最大128枠）と処理後の1枠だけを読み、変更API・NBT・SQLiteを
ゲーム処理から呼びません。所有済み値のみを既存のbounded単一writerへ渡します。

## 意味と欠測

- source: `ll26.60.hurt_slot_empty.v1`、amount=1、primary_type=入口種類。
- details: hook/phase/scope、delta_damage、slots/slot、entry_units=1/after_units=0、
  nested=false、break_completed_verified=false。時刻は正常終了後のUTC epoch ms。
- 主所持品スロットを通らないコピー、装備、副手、別容器、入れ子、再装填、
  呼び出し元が後から空にする経路は欠測です。全破損数とは一致しません。
- 同じ呼び出し中の別Mod変更も含み得ます。耐久閾値到達・因果関係・破損成立・
  ワールド保存を証明しません。API名やtrueだけから破損成功を推測しません。
- 記録なしはnull／未計測。過去の全破損数を0にせず、既存items_usedなども増やしません。

## 閲覧と検証

新しい別プロセスの `/actions` で「耐久処理true応答・主所持品スロットの1個消失」を
選びます。APIの指標は `hurt_slot_losses`、内訳は `item_hurt_slots`。
JST日/月/全期間、プレイヤー選択、正の記録のみの順位、上位100種類、初回/最終観測と
sourceを表示。元の18767は更新・再起動しません。

コンパイル・synthetic SQL/API/DOM・fake lifecycleは実ゲーム証拠ではありません。
最新ビルドとテスト結果はSTATUS.md先頭を参照。新候補はまだBDSで有効化もしていません。
実行が許可された通常ホストの新規world/未使用portで、実プレイヤーが生存モードで
通常使用した1個の道具の消失を目視し、別に記録した観測数とDB/API/UIを照合する
必要があります。元world、18767、認証、ファイアウォールは変更しません。
detailsや所持品コピーの経路から欠測した場合は未検証のままにし、期待値をDBに
合わせたり、架空イベントを挿入してPASSにしません。

固定版LeafKnife/Statsの
[該当フック](https://github.com/LeafKnife/Stats/blob/5bb8bfb7e92c28e03bdd02d93ba7cc8b6c15bb40/src/mod/Hook/Item/ItemStackBase.cpp)
はMCAPIの使用という事実のみ参照。AGPL-3.0のコード・コメント・翻訳はコピーせず、
当プロジェクトの既存bool結果observerと弱参照・single-writer設計で独立実装しました。
