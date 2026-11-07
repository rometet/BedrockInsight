# 盾の未実装境界（26.60固定ヘッダー調査）

`minecraft:damage_blocked_by_shield` は引き続き必須・未計測です。
推測値を表示して77項目完成とはしません。

## 新しい証拠採取候補（既定無効・統計へ不算入）

実ガード／無敵／キャンセル等を比較するため、独立した型付きraw観測を追加しました。
**盾の成功producerではありません。MISSING/未計測を維持します。**
通常起動ではフックを登録せず、既存動作への追加観測もありません。
明示的に `BEDROCKINSIGHT_SHIELD_EVIDENCE=1` を指定した新規テストプロセスだけで有効化。
"true"、"01"、"1 "などは有効化しません。この環境変数はアプリの一時検証設定であり、
認証・ネットワーク・OS設定ではありません。既存BDSの環境やDLLを変更してはいけません。

公開 `Player::$_hurt` の前後で以下をコピーします。

| 観測値 | 条件 / 解釈 |
| --- | --- |
| isDamageBlocked / isInvulnerableTo | sourceが生存する入口だけで評価。予測／判定の観測であり今回の盾成功ではない |
| isBlocking / BlockedUsingShield flag74 | 公開virtual / LLNDAPI getStatusFlagの前後値。flag名から成功を断定しない |
| shield / damaged-shield timestamps、previous flags | 固定Player.hの型付き宣言から読む。sentinel/時刻単位/更新条件は実機比較待ち |
| invulnerable flag/ticks、dead flag | 型付き値の前後コピー。これだけで無傷終了の原因を断定しない |
| hurt入力、戻りenum・optional量 | 有限0..100000HPを最寄りmilliへ丸める。範囲外/非有限はnull、optional存在とは別に保持 |

originは一度だけ呼び、戻り値・例外を変更しません。完了後は弱参照を再解決し、同一Player・
Level/次元と未removedを確認。古いthisやsourceを完了後に読みません。再入れ子は曖昧と明示。
1プレイヤーあたり5秒以上の間隔（初回/時計逆行時を除く）、最大256プレイヤー、
新規IDの枠が埋まった場合は30秒超の古い採取枠のみ整理。それでも満杯なら採取しません。
入口で採取枠を予約するため、後段の観測失敗やqueue dropでも次の採取機会まで欠測し得ます。
これは全被弾ログやイベント数の証明ではありません。比較操作は**6秒以上**あけてください。
他Mod等がこのhookに到達する前に処理を返した場合も採取されません。採取行がないことは、
盾成功・キャンセル成功・被弾なしの証明ではありません。位置座標は測定していません。

新規event_kind `player_shield_evidence` をenum末尾へ追加し、既存ordinal/意味を保持。
cause `ll26.60.player_hurt_shield_evidence.v1`、primary_type `minecraft:shield_evidence`、amount=0。
既存schema4/events/有界writerを使用し、SQLite migration・旧データ改変・game-thread DB処理なし。
daily type集計、CUSTOM、ランキングへ接続しません。JSONは常に以下を明示します。

```json
{"shield_success_verified":false,"shield_blocked_amount_measured":false,"eligible_for_statistics":false}
```

## 通常ホスト担当者向け手順

既存の承認済み通常ホスト経路で、未使用root/world/portを新規作成した場合だけ実施します。
候補DLLはSTATUSの最新ハッシュと照合し、既存BDS/18767へ上書き・配備しないでください。
新規BDSを起動する**専用PowerShellの一時環境**で上の環境変数を設定して子へ継承させます。
管理者権限変更、online-mode変更、セキュリティ回避は不要・禁止です。
開始ログの `Shield raw evidence opt-in active; NOT a shield statistic or success proof` を確認。
未設定ならraw採取も行わず、このログは出しません。実起動の確認はまだ未実施です。

各ケースを6秒以上離し、実操作時刻・対象Player・操作内容を別に記録します。

| 実操作ケース | 比較目的（自動合格条件ではない） |
| --- | --- |
| 盾なしの通常被弾 / 盾を持つだけ | 予測フラグと通常DamageProcessedの関係を比較 |
| 盾で前面を実際に防ぐ / 背面被弾 | flag/timestamp/戻り値が正例と負例を独立に区別できるか |
| 同条件で無敵・拒否/他Modキャンセル | 可ブロック＋NotHurtの偽陽性を比較 |
| 所有飛び道具 / 近接 / 連続攻撃 | source/入口入力と各raw変化の対応、sampling欠測を比較 |
| 盾破損・効果併用 | damaged-shield timestampや他の軽減段階を実盾量と誤認しないか |

正常stop後、閉じた私有DBまたは明示的なread-only対象をローカルで検査します。
次のCLIは読み取り専用で、サーバーを起動せず、トークン不要の**ローカルファイル検査**です。
このrawデータを返すHTTPエンドポイントは追加していません。

```powershell
python -m companion.shield_evidence --database '<私有検証DBの絶対パス>' --limit 100
```

UUID/activation IDはopaque keyへ置換、表示名・event_id・観測時刻とrawフィールドを出力。
最新最大1000行の走査部分、無効行数、残り有無を明示し、全期間総数や成功数を推測しません。
重複キー・型違い・偽のsuccess=true・非有限JSON・別source・未来行は除外。
空データでも `shield_metric_available=false` / `full_parity_accepted=false`。CLI終了0は
ファイル読取ができた意味で、統計・engine・実プレイのPASSではありません。
Webはraw行があっても盾をunsupported/null／未計測のまま表示します。

## 検証境界

固定26.60単体コンパイル、origin一回、設定predicate、256枠/5秒制限、builder→writer→
SQLite→私有CLI、既知raw変化でもnative統計/認証API/実stats.jsの合成DOMが未計測のまま、を
合成テストで確認します。**実BDS・実盾操作・browserの証明ではありません。**
必要な証跡は実操作記録、実際のDLLハッシュ/開始ログ、対応event_id/SQLite、正常stop0と
閉DB integrity。正例・負例を区別する実経路と量を確認するまで統計producerは追加しません。

## 既存調査の事実（以下の制限は継続）

固定26.60ヘッダーとLK-Stats固定版の参照で判明した事項:

- Playerの公開virtual `isDamageBlocked(source)` は利用可能ですが、同名private `$isDamageBlocked` thunkはMCNAPI。
  前者を使えることと、今回の実際の盾成功・軽減量を証明できることは別です。
- 固定ヘッダー全体には `_blockUsingShield` の公開フック宣言が見つかりません。
- Player.hに型付き `mBlockedUsingShieldTimeStamp` / `mPrevBlockedUsingShield` が存在。
  ServerPlayer.hに `mPrevShieldBlockingTick`、ActorFlagsに `BlockedUsingShield=74`、
  LevelSoundEventに `ShieldBlock=255` が存在します。しかし宣言・名称だけでは更新条件・
  通知時点・ダメージ値との対応・他Mod/コマンド発火を独立証明しません。
- [固定上流のshield hook](https://github.com/LeafKnife/Stats/blob/5bb8bfb7e92c28e03bdd02d93ba7cc8b6c15bb40/src/mod/Hook/Player/PlayerBlockUsingShield.cpp)
  は、入口の可ブロック判定とhurt後の無ダメージ戻りを組み合わせる推論です。
  同版に書かれた試験結果は、BedrockInsight候補の実機証明ではありません。
  無敵・キャンセル・別の無傷終了を除外できるローカル証拠はまだありません。

従って、旗・timestampの名前だけを「成功した盾軽減」として出力するadapterや、
予測可能フラグ＋NotHurtだけで全入力ダメージを加算するproducerは追加していません。
旧試行・通知データもshield実績へ昇格しません。Web未計測・native読取未対応を維持します。

次の検証は既存BDSではなく、通常ホストの新しい隔離worldで行う必要があります。
実際に盾で防いだ正例と、盾なし・構えるだけ・背面攻撃・無敵・キャンセル・投射物・
連続同tick攻撃などの負例を比較し、公開API/イベントで実成功を区別できるか確かめます。
今回追加した対象projectだけの型付き・有界raw観測候補で比較します。アドレス/offset推測、
packet傍受、security変更、sandboxオンラインBDS再試行は行いません。
真のblocked量が検証できるまではこのキーを受入にしません。
