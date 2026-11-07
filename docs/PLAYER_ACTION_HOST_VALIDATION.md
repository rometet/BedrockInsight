# 行動候補F9D1の通常ホスト検証：まだ実行していない手順

最終監査追補：並行編集でstats_reader/runtime.cpp/runtime.hppと重要3スクリプトが
下の15E4ビルド後に変化しました。15E4もコーディネーターFEB82Dも現ツリーの検証証拠には
できません。STATUS最上段のFINAL AUDITを読み、担当・ソース固定を調整してから
現ツリーの候補を再検証してください。古いDLLへ後続変更の受入を帰属させません。

2026-11-07追補：旧7EDC等も履歴です。最新STATUS最上段の新候補ハッシュ／
source_hashes／build_script_hashesを確認してください。
新しい[killed_byと子飛び道具](KILLED_BY_ATTRIBUTION.md)は、環境原因・自己・所有者不明を
別扱いし、直接攻撃／飛び道具の経路、種類、被害者・攻撃者の統計、DB閉鎖整合性、
認証Web、個人GUIの後続ページ、別Modから4関数呼出しを実操作で照合する必要があります。
射撃・PvP・繁殖・取引等の参加者／操作範囲は勝手に拡張しません。空smokeやfixtureを
全9分類／77キーの受入にはしません。今回新BDSを起動したことはありません。

> 以下のF9D1固定ハッシュとステージング例は**旧手順**です。現在は全9分類・CUSTOM77・
> GUI3コマンド・4APIが必須です。最新STATUS.mdと候補のsource/hash証拠を確認し、
> コーディネーターが新規world/未使用portの試験計画へ更新してから使用してください。
> 旧候補や使用済みrootで新しい変更を受け入れたことにはできません。
> 空smokeは実操作証拠にならず、1人用allowlistを勝手にPvP試験へ拡張しません。
> 新しいGUIページング・時間値・CUSTOM欠測・他Modからの4APIも実際に照合が必要です。

## 2026-11-07 固定スナップショットの候補（ビルドPASS、現ツリーとは不一致）

`.build/server-candidate/run-a2b17a0840b446e3b8a77d1df9cacdbe/`、DLL
`15E4F5F03AB5D4F97E32621830ECBB304E29AACE73C7143677225DA49ACB6A05`、ZIP
`F9800510DE8FA92B2C7BCCD014B37752A3C7C23BBC4BD6F80C079F550D1B486C`。
19ネイティブ単位・prelink/link/PE検査・packageはPASS、81ソース/ヘッダーと
重要3ビルドスクリプトのハッシュ一致。候補固有のcompileディレクトリを使用。
取引／繁殖の並行追加コードも含みますが、今回の帰属・ページングfixtureをその
2機能の実操作証明には流用しません。
このバイナリでのBDS起動・GUI操作・実プレイヤーの記録は未実行です。
旧09C等の空smokeや旧MSPT受け入れを新候補の証拠にしません。

通常ホストのコーディネーターだけが、現在のstage/isolation手順で**新規root・world・
未使用port**を作成し、上の候補パスとハッシュに置き換えて検査してください。
以下の旧F9D1コマンドをそのまま実行しないでください。参加者・XUIDは本人の
承諾を再確認し、元の認証・allowlist制約は維持。既存BDS/18767は変更しません。

新しく確認すべき点：個人明細と順位の前後ページ、CUSTOM77キー選択と未対応表示、
表示名、CUSTOM合計がnull/対象外であること、完結時間だけのランキング、クラッシュ
開放セッション・重複・オフライン時間が増えないこと。新しい4C関数は独自ABIなので、
別Modからの実呼出しとLegacyRemoteCall互換を別に検証します。
既存 `companion.action_acceptance` は対応する一部ソースの照合器であり、
全9分類/77キー/GUI/互換性の受け入れ器ではありません。

既存BDS・18767番・全ての使用済みworldは変更しません。この手順はコーディネーターが
既存の承認済み通常ホスト経路から実施するものです。オフラインCodex sandboxでは起動しません。
MCP承認拒否やfirewall/認証設定を迂回しないでください。MSPTの再受け入れや空smokeの繰返しではなく、
**実プレイヤーの行動を初めて観測する**試験です。参加者の明示的な名前/XUIDと参加が必要です。

候補 `.build/server-candidate/run-d0b293d34d2f42c4bb7e918f9bab131e`、DLL SHA256
`F9D1DA65216DB0F6FC7AAC8148FB6B3E2981C9A33759C6783A260754EAD4959E`。
ZIP SHA256 `958E0368348F034C7387DE9DAB8E731A93B256352751671060DCD7A6829CAC23`。
まだBDSで有効化しておらず、以下はPASSの証拠ではありません。

18:48追記：実プレイヤー検証器の古い300秒上限を、既存runnerと同じ30〜3600秒に
合わせました。下記600秒の試験はrunnerでは以前から許可されていましたが、停止後の
検証器が不一致で拒否していました。runnerの時間・allowlist・認証・停止条件は変更していません。
Python85テストと宣言probeはPASSですが、実行済み行動試験の証拠ではありません。

## 新しい隔離サーバーだけを準備

通常ホスト端末、作業ディレクトリはBedrockInsight。59720/59721が使用中なら別の未使用high portを選択。
プレイヤーはこの新規使い捨てworldへの試験参加を承諾した本人だけ。既存allowlistを流用しません。

```powershell
$env:BEDROCKINSIGHT_CLANGCL_ROOT='C:\Dev\LL2660Toolchain-20261029\clang+llvm-23.1.4-x86_64-pc-windows-msvc\bin'
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$testName=Read-Host '試験参加を承諾したプレイヤー名'
$testXuid=Read-Host 'そのプレイヤーの確認済みXUID'
$stage=scripts/stage_isolated_capture.ps1 -CandidateRoot .build/server-candidate/run-d0b293d34d2f42c4bb7e918f9bab131e -RealPlayer -PlayerName $testName -PlayerXuid $testXuid -ServerPort 59720 -ServerPortV6 59721 -PassThru
scripts/prepare_capture_executable.ps1 -SandboxRoot $stage.sandbox_root
scripts/check_capture_isolation.ps1 -SandboxRoot $stage.sandbox_root -RequirePatched -RealPlayer -PlayerName $testName -PlayerXuid $testXuid
$stage.sandbox_root
# 全検査PASS・新規rootだけ。既存BDSとポートが重なるなら手順を中断し、既存側は変更しない。
scripts/run_isolated_capture.ps1 -SandboxRoot $stage.sandbox_root -RealPlayer -PlayerName $testName -PlayerXuid $testXuid -StartupTimeoutSeconds 105 -PlayerWindowSeconds 600
```

online-mode=true、allowlist本人のみ、no cheats、ModのHTTP/rollback無効を保ちます。
runnerは自分の子だけを停止します。プレイヤーは全行動の完了後に退出してください。

## 実際に行って記録する項目

- この新規サーバーへ本物の認証クライアントで参加。既存サーバー/ゲームプロセスを自動操作しません。
- 入手した種類と個数を控え、単品と複数個を投棄。成功戻り値と入口個数の記録を見比べます。
- 同じアイテムを拾い直し、主インベントリの型別合計を前後で控え、増加差分と拾得観測のamountを照合。
  可能な範囲で一部だけ入る状況／空きなし／全量が入る状況を比較します。危険な自動操作や
  実在しない試験条件を捏造しません。試せなかった条件は未試験と記録します。
  入口個数ではなく実所持数の差分であること、装備・カーソル・別容器を含めないことを確認。
  再入れ子／容器交換／消滅は通常プレイで誘発しないなら未試験とし、稼働BDSへ計測用Modを追加しません。
- 本人が承諾した使い捨てworldだけで、例えば自然な落下による死亡とrespawnを1回観察。
  発生時刻/原因をメモし、通知数と成立数を別々に確認します。cheats/killコマンドは使いません。
- 既に死亡している間の複数通知が成立数を増やしていないか確認。
  死亡時のrandom dropが通常投棄に混ざらないか確認。再度の死亡試験は本人の承諾範囲内だけ。
- スプリント／スニークの開始と終了をそれぞれ行い、各状態を1秒以上維持し、時刻を記録。
  最後の操作から5秒以上待つか正常退出し、確認済みカウンターのflushを待ちます。
  250ms未満の短い切替は合流／欠測し得るので全キー操作数との一致を要求しません。
  取消済み要求だけから行が増えず、同一の遅延フラグ観測を再flushで重複計上しないことを確認。
  日替わりを実際に跨がない場合、その実機境界は未試験と記録し、時計を変更しません。
- 地上で通常のジャンプを数回行い、時刻を控えます。可能なら空中の再入力・連続ジャンプと比較。
  入力回数と一致する保証はなく、このBDS経路の実発火と正の状態変化を確認します。
  乗車／次元変更／極端なjump boost等を無理に作らず、試さなかった条件は未試験と記録。
  処理直後の接地フラグがtrueでも、空中移動完了や距離を測定した証明にはしません。
- 通常プレイで入手できたアイテムを使用し、種類・使用した手・主所持品の種類別合計の前後と
  時刻を控えます（例：投げられるアイテムと、個数の減らない使用）。アイテムを得られない場合や
  本経路を通らない場合は未試験／欠測とし、cheats・アイテム追加や偽SQLで埋めません。
  処理前の使用試行とGameModeのtrue応答を足さず、trueでも効果完了・消費成功とは断定しません。
  副手の個数を主所持品減少へ混ぜず、取消/失敗の行が成功へ変わっていないか確認します。

この新しい使い捨てworldで参加者が通常の直接攻撃によりMobを倒した場合は、入口の種類・時刻を
控え、直接攻撃sourceの死後フラグ行と比較します。飛び道具や落下・火などはこの指標に混ぜません。
対プレイヤー試験は第二参加者と二人用隔離設定の明示承認がある場合だけです。
上の単独参加用helper／allowlistを手変更して二人にせず、未対応なら対プレイヤー実機欄を未試験に保ちます。
既存の二人用BDS・world・allowlistを試験先として流用しません。全行動を終えてから正常退出します。
正常退出後にrunnerのstop/exit0、非強制終了を確認。enableだけでは行動成功にしません。

途中でクラッシュ/弱参照解決失敗/未保存ならログを保全し、試験rootを再利用しません。
行がない場合、原因未特定のままfake値やSQLite INSERTで埋めないでください。

## 停止後のDBと閲覧

```powershell
scripts/verify_real_player_capture.ps1 -SandboxRoot $stage.sandbox_root -PlayerName $testName -PlayerXuid $testXuid
```

これはjoin/leave・正常停止・integrity・元DB不変を確認し、**私有の停止DBコピー**を作ります。
このPASSは使用true応答/直接攻撃帰属/ジャンプ加速/拾得/死亡/投棄/モード観測の受け入れそのものではありません。出力のdatabase_snapshotを指定して、
別の空いているloopbackポート（例18768）で新コンパニオンを起動します。

```powershell
python -m companion.server --database <出力されたdatabase_snapshotのパス> --port 18768
```

`http://127.0.0.1:18768/actions` へ新しい起動時トークンで認証します。18767番は再起動しません。
死亡フラグ成立の原因/個数、投棄種類/入口個数/回数、JST日・月・全期間/プレイヤーの各表示を
実行記録と照合します。拾得は `item_pickup_delta` / `ll26.60.take_inventory_delta.v1` の
amountがdetails.after_units-before_unitsと一致し、入口個数以下・同じ枠数範囲であることを確認。
APIのpickup_inventory_items/pickup_delta_actions・種類別内訳・日本語画面を照合し、
全拾得や永続転送の証明にはしません。モードは `player_mode_observation` / `ll26.60.mode_flag_observed.v1` の
amount合計、sprint/sneak、start/stop、detailsの最初／最後の観測とJST bucketを確認。
APIの4指標・モード内訳・日本語画面を照合し、欠測を0・継続時間・距離・現在状態にしません。
通知と成立を足し合わせないでください。停止DBのTPSは現在値になりません。
ジャンプは `player_jump_impulse` / `ll26.60.jump_pos_delta.v1` / amount=1、
detailsが接地・非乗車、正のafter_y_micro>before_y_micro・範囲内であることを確認し、
dimensionを実行記録と照合します（前後の同一次元はnative側の除外条件です）。
APIのjump_impulses/観測期間/定義と日本語画面を照合。行がなければ入力数で補完せず未検証／原因調査です。
直接攻撃は `direct_kill_observation` / `ll26.60.direct_attack_dead_transition.v1` / amount=1、
actorが攻撃者、primary_typeが入口被害者種類、secondary_type=mob|player、detailsの原因2／
source一致／Player解決／自己除外／死亡前false・後trueを確認します。生の攻撃者ID等は公開しません。
APIのdirect_kills/direct_mob_kills/direct_player_kills・種類別内訳・JST期間・日本語表示を照合し、
被害者側の死亡数とは別の指標にします。通知／不明帰属／未試験のPvPを成功扱いしません。
使用は `item_use_result` / `ll26.60.gamemode_use_return.v1` / amount=1、
primary_typeが入口種類、secondary_type=main_hand|off_handであることを確認。
detailsのhook/phase/scope、entry_units/slots/before_units/after_units/loss_units、
nested=false・effect_completed_verified=false・consumption_verified=falseを確認します。
loss_units=max(0,before_units-after_units)で入口個数以下だけ。APIのuse_return_actionsと
use_inventory_loss、種類／手の内訳、JST期間・選択Playerと日本語画面を照合。
減少の正の記録なしは不明であり、処理前通知・consumeItem引数から推定しません。
別端末からは既存の私設SSH転送のみ。画面認証のトークンをログ/証拠JSON/会話へ保存しません。

残す証拠はcandidate/runtime/stagingの各ハッシュ、実行時刻と行動メモ、正常停止、
private-copy integrity、バージョン付きcause/amount/type/detailsの実行範囲内行、認証API401/200、
日本語UIとの一致です。プレイヤーIDを公開しません。新featureごとにSTATUSの実観測/SQLite/UI欄を
更新し、fixtureテストや旧95FEのsmokeを実行証拠に代用しないでください。

## 生イベント・認証API・日本語JS表示の追加照合（実機実行はまだ）

`verify_real_player_capture.ps1`が出力した**私有のreal-player-evidence.json**と
同じフォルダーの`closed_snapshot.sqlite3`を使います。元のBDS DBはこの追加チェックでは
開きません。既存18767は使わず、自分だけのephemeral loopback listenerを起動・終了します。
これは新しい常設viewerの起動ではありません。

試験中に別途控えた実際の行動メモから、リポジトリ内の私有JSONを作成してください。
SQLite/APIの結果から期待値を逆算して埋めたり、不一致をPASSさせるため書き換えたりしません。
次の空テンプレートは**そのままでは必ず失敗**します。

```json
{
  "kind": "operator-action-observation-v1",
  "observed_in_game": true,
  "expected": {}
}
```

expectedには実際に観察して定義を照合できた指標と正の整数だけを記入します。

- `drop_actions` / `dropped_items`：非random投棄のtrue応答回数／入口個数。
- `pickup_delta_actions` / `pickup_inventory_items`：主所持品増加の観測回数／増加個数。
- `confirmed_deaths`：処理後の死亡フラグ成立。処理前通知と足さない。
- `sprint_starts` / `sprint_stops` / `sneak_starts` / `sneak_stops`：持続した状態切替の観測。短いキー入力数ではない。
- `jump_impulses`：本経路の上向き加速。空中移動完了ではない。
- `direct_kills` / `direct_mob_kills` / `direct_player_kills`：直接sourceと死亡フラグの照合。既存単独参加用設定でPvPを作らない。
- `use_return_actions` / `use_inventory_loss`：使用true応答／主所持品の正の減少。消費成功や効果完了ではない。

未観測・欠測・失敗は無理に0や別の成功値へ変えません。これらは今回の合格対象に含めず、
STATUSの未検証欄へ残します。クラフト／距離／破損はこのチェッカーでも合格できません。

```powershell
$captureReport=Read-Host '通常ホスト検証器が出力した私有real-player-evidence.jsonのパス'
$actionNotes=Read-Host '実行中に別途控えた行動メモJSONのパス'
python -m companion.action_acceptance --capture $captureReport --observations $actionNotes
```

この処理は次を照合し、どれかが違えば非0で終了します。

- 正常exit0・非強制終了・native load/enable・同じ一人用アクセス設定・実join/leave・閉じたsession。
- runtime/staging/stdout/DLL/私有snapshotのハッシュ、v4・integrity・外部キー、検証中の不変性。
- 実セッション範囲内・同じ起動世代・開始/終了マーカーに挟まれた正規source/kind/amount/details。
- 独立した生イベント演算と行動メモの期待値。取消・通知・旧形式から成功を作らない。
- 認証なし401／認証あり200、選択Player・正のSQLランキング。全期間と、観測したJST日／月をそれぞれ生イベントから計算。
- HTTPで取得した実actions.html/jsと評価対象のハッシュ一致、日本語の個数・種類・手・原因・モード表示と未計測表示。
- 自分のlistenerだけを終了し、トークン・生UUID/XUID・名前を結果JSONへ出さない。

出力の`matched_metrics`だけが照合対象です。`not_accepted_metrics`は合格ではありません。
`independent_gameplay_attestation=false`と`browser_verified=false`は常に残します。
編集された証拠やfixture行が本物のゲーム操作だったことまで独立証明できるツールではありません。
Nodeの実JS/DOM harnessによる照合であり、ブラウザーの描画やスクリーンショットの確認でもありません。
実際の参加・操作記録と正常ホスト実行を伴って初めて、対応する実機欄の根拠にできます。
