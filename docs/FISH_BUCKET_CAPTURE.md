# fish_caught — 水生Mobのバケツ捕獲観測

このキーを「釣り竿で釣り上げた魚」と表示する旧ラベルは訂正しました。
固定LK v0.13.0の[相互作用経路](https://github.com/LeafKnife/Stats/blob/5bb8bfb7e92c28e03bdd02d93ba7cc8b6c15bb40/src/mod/Hook/Player/PlayerInteractActor.cpp)
はバケツ処理と水生Mobへの相互作用を関連付けます。上流のコード／コメント／翻訳はコピーしません。
本実装は遅延pending mapではなく、同じ同期呼出し内だけの独立した照合です。
旧表の「Bucket API非公開」は固定26.60の`Player.h`と`BucketItem.h`のMCAPI宣言に基づき訂正。

## 何を数えるか

`Player::interact`の1回について、次の全条件が揃った場合にのみ1件保存します。

1. 主手の、明示した水生Mob型への相互作用で、UUIDを持つ生存Playerと対象が同じLevelにいる。
2. 同期呼出し内の`BucketItem::$_useOn`が同じ対象ポインター・UniqueID・主手を扱い、入口が
   `minecraft:water_bucket`1個、戻り値のswingフラグがtrue。対象呼出しは1回だけ。
3. 外側の相互作用がsuccessを返し、再入れ子がなく、Playerを同じWeakEntityRef／identityへ
   解決できる。Level・次元・主所持品オブジェクトと枠数が変わっていない。
4. 対応する種類のバケツが同じ主所持品に正確に1個増加している（最大128枠の型別個数比較）。
5. 対象の弱参照が解決できなくなったか、同じ対象を解決してremovedフラグを観測した。

5の二通りはdetails.target_after=`weak_unavailable`／`removed_flag`で区別します。
**弱参照が解決できないことだけでは消去成功を証明しません。** 他の条件との同時観測であり、
生成バケツのNBT・永続保存・全捕獲の証明ではありません。ゲームthreadでDBを書きません。
catch前のActor／Player／ItemStack参照をorigin後に再読せず、Playerと対象を弱参照から解決します。
telemetryの準備／後処理／sink例外でoriginを再実行せず、originの戻り値・例外はそのまま維持します。

候補の完全一致型対応（実26.60での全ID／フラグ発火は未受入）:

| 対象型 | 増加を確認するアイテム |
| --- | --- |
| minecraft:cod / minecraft:fish（旧名候補） | minecraft:cod_bucket |
| minecraft:salmon | minecraft:salmon_bucket |
| minecraft:pufferfish | minecraft:pufferfish_bucket |
| minecraft:tropicalfish | minecraft:tropical_fish_bucket |
| minecraft:axolotl | minecraft:axolotl_bucket |
| minecraft:tadpole | minecraft:tadpole_bucket |

識別子は名前空間込みで比較し、Modの似た名前、dolphin、釣り針等は分類しません。
Bedrockの`tropicalfish`という対象IDは[公式のバニラ例](https://learn.microsoft.com/en-us/minecraft/creator/reference/source/vanillabehaviorpack_snippets/entities/tropicalfish?view=minecraft-bedrock-stable)
にもありますが、最新の資料は固定26.60での実行証拠には流用しません。

## 保存と表示

- kind=`player_interaction_result`、cause=`ll26.60.fish_bucket_delta.v1`
- primary_type=`minecraft:fish_caught`、secondary_type=対象型、amount=1
- detailsはPlayer/child hook、phase=`return_success_bucket_gain_target_retired`、main hand／
  入口water bucket、対象／出力型、同呼出し・1回・再入れ子なし、枠数／前後個数を記録
- fishing_rod=false、durable_world_commit_verified=false。座標の監査記録ではない

既存SQLite v4・bounded writerを維持し、旧イベントや試行は書き換えません。
native StatsReaderの個人／CUSTOM順位、ゲーム内GUI／独自APIの共通経路、新しい別プロセス用
Web `/stats`と認証`/api/v1/stats?key=minecraft:fish_caught`へ接続します。
JST日／月／全期間・Player選択・順位・ページは既存の読取機能を使用します。
未記録はnull／未計測で、真のゼロや全機能完成とは扱いません。
起動例`http://127.0.0.1:18768/stats`は[既存の安全な手順](STATS_LIBRARY.md)を参照。
この作業で常駐viewerを起動したり、18767を更新しません。

除外／限界：副手、釣り竿のloot、creative等の増加なし、未知ID、別容器・カーソル・副手の増加、
複数／再入れ子・主所持品交換・Player失効・次元変更・取消／失敗、全経路や長期的なworld整合性。
型別ネット増加なので、他Modが同期中に同型アイテムを増減させる場合の完全な因果分離も未証明です。

## 検証と未受入

```powershell
python -W error::ResourceWarning -m unittest discover -s tests -p 'test_fish_bucket.py' -v
.\scripts\verify_travel_v2.ps1 -WorldTick -FishBucket
.\scripts\build_server_candidate.ps1
.\scripts\verify_stats_library.ps1
```

合成テストは全型対応・数量／フラグ／再入れ子の除外・origin1回と例外保持、同じ本番builder→
StorageWriter→SQLite→native個人／順位／integrityを検査します。その停止DBから認証HTTP→
実際の日本語stats.js DOMまでの照合も行います。fixtureはBDS／実プレイ／実ブラウザの証明ではありません。
厳格なnative SQLとPythonの不正JSON・数量・型・source判定も照合します。

稼働BDS／旧world／DB／DLL、18767、認証・securityは変更しません。実機は通常ホスト担当者が
未使用world／ポートで実捕獲・正常停止・保存・認証Web・ゲーム内GUI／別Mod APIを確認する必要があります。
CodexSandboxOfflineのonline BDS再試行や、承認拒否の別経路迂回は行いません。
MSPTは既受入のため再試験不要。全9分類／77キーの完成は依然IN_PROGRESSです。
