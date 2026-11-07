# 被ダメージ処理内の吸収属性減少（限定観測・実機未受入）

`minecraft:damage_absorbed`と`minecraft:damage_dealt_absorbed`を、通知量からの計算ではなく、
同じ被害者のABSORPTION属性の前後差へ接続しました。表示名は「吸収属性の減少観測（丸め）」と
「加害先の吸収属性減少観測（丸め）」です。**最終HP損失や全ダメージの証明ではありません。**

## 参照とAPI

固定LK v0.13.0の[combat handler](https://github.com/LeafKnife/Stats/blob/5bb8bfb7e92c28e03bdd02d93ba7cc8b6c15bb40/src/mod/Stats/Handlers/PlayerCombatStatsHandler.cpp)
は、入力された量と入口の吸収量から吸収分を見積もります。本実装はこれをコピーせず、
実際に取得した属性値の減少を独立に比較します。上流コード／コメント／翻訳はコピーしません。

以前のMCNAPIという断定は現在の固定ヘッダーと一致しません。コミット
`072b9ffe0e9d9e0ec846f11d294880e892fe7770`のPlayer::$_hurt、Mob::$_hurt、
SharedAttributes::ABSORPTION、Actor::getAttributeはいずれもMCAPI宣言です。
`ActorHurtResult::Type::DamageProcessed`、optionalのmDamageAmount、AttributeInstanceの
公開TypedStorageフィールドを使用し、enum値はコンパイル時にも照合します。TODO付きの
ActorHurtResult::operator boolには依存せず、アドレス・offset・virtual slotを推測しません。
宣言／コンパイル／リンクと、実際の26.60挙動は別の受入条件です。

## 何を保存するか

1. 正の有限な入力量、生存・未削除の被害者、公開属性APIでABSORPTIONを解決でき、値が正である。
2. Player::$_hurtまたはMob::$_hurtを1回実行。Player→同じ対象のMob親呼出しは1回だけ許容し、
   親からは別にイベントを出さない。それ以外の再入・他対象への連鎖・二度目の親は曖昧として除外する。
3. origin後、弱参照を同じ未削除Actorへ再解決でき、Level／次元が不変、同じ属性アドレスと定義を
   取得できる。元Actor／source参照や古い属性ポインターをorigin後に逆参照しない。
4. 戻り型がDamageProcessedで、mDamageAmountが存在し有限・非負。**この戻り値をHP損失や
   吸収量そのものには使わない。** 属性が減少し、その減少が入力量を超えない場合のみ採用する
   （0.001HP丸め誤差の許容は1）。
5. 各属性値を0.001HPへ四捨五入し、減少差を0.1HPへ四捨五入する。0.05HP未満相当で
   保存単位が0になるものは保存しない。各値の上限は100000HP。異常値は欠測として除外する。

| CUSTOM | 記録するPlayer | 追加条件 |
| --- | --- | --- |
| damage_absorbed | 被害者本人 | 被害者型minecraft:player、UUIDあり |
| damage_dealt_absorbed | 入口sourceを解決したPlayer | directの同一source/damaging ID、またはchild_projectileの別ID、同Level、自己以外。処理後も同じ生存Playerへ解決可能 |

攻撃者の帰属は既存の独立`DamageAttributionFacts`契約を使います。環境・自己・thorns・爆発・
不明owner等を推測しません。被害者と攻撃者の2行はbounded writerへ別々に投入され、queue失敗時の
同時保存は保証しません。Player親処理が実26.60でどう呼ばれるか、完全吸収でどの戻り型になるか、
無敵・致死・Mod介入で何件になるかは実機未受入です。

属性のポインター一致は同じ取得先の観測であり、他Modによる交換・同期変更の世代／因果を完全に
証明しません。damage_taken/dealtの処理前通知、耐性・盾の内訳、最終HP差へ流用・加算しません。

## 保存・表示

- 既存schema4、kind=`player_interaction_result`
- cause=`ll26.60.hurt_absorption_delta.v1`、primary_type=CUSTOM、secondary_type=被害者型
- amount=丸めた1/10HP。detailsにはhook、phase=`post_origin_attribute_decrease`、
  result_type、前後属性／入力／返却のmilli量、parent_calls、同一性、帰属・原因を保存
- final_health_loss_measured=false、exclusive_causation_verified=false、durable_world_commit_verified=false

既存SQLite履歴は変更しません。native個人／CUSTOM順位／GUI／独自APIの共通readerと、
新しい別プロセス用Web `/stats`・認証付き`/api/v1/stats?key=minecraft:damage_absorbed`
（またはdamage_dealt_absorbed）へ接続します。JST日／月／全期間、Player選択・ランキング・ページを
既存経路で提供。未記録はnull／未計測で、真の0や網羅性を捏造しません。
起動は[ローカル限定の手順](STATS_LIBRARY.md)。この作業では18767を更新・起動しません。

## 検証

```powershell
python -W error::ResourceWarning -m unittest discover -s tests -p 'test_absorption.py' -v
.\scripts\verify_travel_v2.ps1 -WorldTick -FishBucket -RaidEffect -Absorption
.\scripts\build_server_candidate.ps1
.\scripts\verify_stats_library.ps1
```

合成fixtureは親1回／二重呼出し・異なる対象・scope／値の型・範囲・丸め・帰属・origin1回と例外保持、
production builder→StorageWriter→停止SQLite→native個人／順位／integrity、同じDB→匿名401／
認証200→実stats.jsの日本語DOMを検証します。native SQLとPythonの不正JSON／重複キー／数量除外も
照合します。**これは実engineフック・実プレイ・実ブラウザ・他Mod連携の受入ではありません。**
最新の実行証跡とDLLの境界はSTATUS.md。

今後、通常ホストの新規隔離BDSで、吸収付きPlayerの直接／飛び道具／完全吸収／部分吸収／環境、
Player→吸収付きMob・PvP、無敵・拒否・再入を分け、SQLite・認証Web・ゲーム内GUI／他Mod APIまで
比較する必要があります。稼働BDS/world/DB/DLL、18767、認証・security・networkは変更しません。
MSPTは既受入で再試験不要。全9分類／77キーの完成宣言はしません。
