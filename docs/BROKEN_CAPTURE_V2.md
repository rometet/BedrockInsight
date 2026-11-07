# 耐久処理・スロット消失のv2捕捉（候補、未配備・実プレイ未受入）

既存の `ItemStackBase::hurtAndBreak(int, Actor*)` フックを独立拡張しました。
新しいフック・アドレス推測・パケット改変・NBT読取・DBマイグレーションはありません。
AGPL Statsのコードや翻訳は使用していません。

## 捕捉条件と対象

正式26.60固定ヘッダーの公開 `Actor::getArmor(ArmorSlot)` と `getOffhandSlot()`、
従来の主インベントリ読取を使います。ArmorSlotはHead=0/Torso=1/Legs=2/Feet=3。
Body=4は人型4枠に含めません。SDKのLLアクセサであり、手書きオフセットは使いません。

1. 未削除Player所有、非空UUID、次元-1..127、正の耐久ダメージ。
2. damageable、入口1個、最大耐久2..32767、入口耐久0..最大、
   `入口耐久 + 入力ダメージ >= 最大耐久`。これは**入力閾値**であり、
   実適用ダメージや内部の乱数・耐久軽減結果を読み出した値ではありません。
3. 主インベントリ最大128枠、副手1枠、防具4枠を調べ、呼出し対象ItemStackBaseの
   アドレスが**ちょうど1枠**に一致する場合だけ入口を保持。
4. originを一回だけ呼び、true応答・入れ子なし・capture有効を確認。
   originの戻り値／例外は変更しません。
5. 弱参照から同じ未削除Playerを再解決。同Level・同次元を確かめてから枠を読み直す。
   主所持品は同じInventory・同サイズも必要。取得した同じ枠のItemStackアドレスと
   入口が一致し、個数1→0なら入口アイテム種類で1件記録。

origin後に元のownerや消えたstackを直接参照しません。値は既存bounded writerへ送るだけで、
tickごとのDB処理やアイテム変更はありません。非防具 `used` の従来定義は維持し、
その経路にも未削除・Level/次元確認を加えました。

## 保存・GUI・API・Web

新causeは `ll26.60.durability_item_destroyed.v2`。
`secondary_type` は `main_inventory` / `offhand` / `armor`。
JSONは種類・スロット・入口耐久・入力ダメージ・同Player/Level/枠・次元と、
`final_break_verified:false` / `world_save_verified:false` を含みます。
XYZは読み取っていません。default0を実際の位置と解釈しないでください。

旧v1履歴は消さず、その旧15フィールド契約と主所持品のみの意味で読みます。
v1をv2のframe確認・装備対応の証拠へ昇格しません。
native StatsReaderの個人/種類/順位/ページ数と認証Webのstats/actionsは同じ条件で、
空JSON、型不一致、誤スロット、重複/未知キー、閾値不足、未来日時、試行を除外します。
不正行は削除しません。不正行だけ／記録なしはnull、正の順位から除外です。

native GUI/C APIは全期間、WebはJST日・月・全期間。
`/stats` GUIと4 C APIには限定scopeを表示し、APIはv2/v1ソースと
`final_world_commit_verified:false` を追加します。C ABI自体は変更なし。
これはLegacyRemoteCall互換ではありません。

## 保証しないもの・実機で必要な比較

同じ枠が呼出し中に空になった観測であり、他Modの介入を排除した破損因果、
全道具/全防具の全経路、最終ワールド保存の証明ではありません。
コピーstack、ownerなし、未知UUID、装備objectの交換、同型再装填、
入れ子、次元移動・削除、別の破損経路は欠測します。主Inventoryサイズ不正も除外。
盾のアイテム破損は `broken` の候補ですが、**盾で防いだダメージ量とは無関係**です。
`damage_blocked_by_shield` は未対応/nullのままです。

通常ホストの新規隔離world/portsで、主手道具・副手盾・防具4枠/エリトラの実破損、
閾値手前、Unbreaking、Mending、耐久減少だけ、キャンセル、交換/コピー、再入、
死亡・ログアウト・次元遷移を比較する必要があります。入力・実操作・SQLiteのv2行・
ゲームGUI・別Mod API・認証Webの整合、正常終了とclosed DB integrityを保存してください。
既存BDSや18767へ配備する許可ではありません。Codex offline sandboxでオンラインBDSを再試行せず、
通常ホスト経路が利用不可なら実機結果を捏造しないでください。

実施したfixture/buildコマンドと証跡はSTATUS最新節を参照。
合成テスト・DLLリンクだけでは全機能受入を変更せず、PROJECT_STATUSはIN_PROGRESSです。
