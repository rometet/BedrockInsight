# 修正版MSPTのホスト側受け入れ検証

2026-11-06。これは通常ホスト経路で操作するコーディネーター向けです。
実行経路・認証・proxy・firewallを変える手順ではありません。
既存BDS、既存world、18767番のコンパニオンは停止・変更しません。

[ネットワーク監査](../CODEX_SANDBOX_NETWORK_AUDIT_2026-11-06.md)では、outbound Blockの
対象はCodexSandboxOffline（SID末尾1003）で、通常ホスト（末尾1001）とは別でした。
過去の失敗したBDS子プロセスのtoken自体は未採取なので、因果関係の完全証明ではありません。
ただし、オフラインCodex環境からオンラインBDSを繰り返し起動する検証は行いません。
以下は既存の承認済み通常ホスト経路でのみ実行し、firewall・ユーザー・sandbox policy・
online-modeを変更しないでください。Codex側のホスト接続は現在承認ポリシーで拒否されており、
迂回せずコーディネーターが通常ホスト側で実行する必要があります。
enable前のサービス接続失敗は、DLLの実測処理の成功／失敗の証拠には使いません。

最新候補は `.build/server-candidate/run-0388efb2490c4235b9cae627af0f7f60`。
DLL SHA256: `218a8bc7d6a3646b317cb05d1cf8f2a2c9f62d1739fa1aa903a5adb99970cfbf`。
旧C8F24...候補は使わないでください。SymbolProviderのリンク修正はビルド／単独対照試験済みですが、
実BDSでの修正完了・実測MSPT成功はまだ未確認です。

## 1. 通常ホスト端末A：新規隔離・起動・正常停止

作業ディレクトリはこのプロジェクトです。59670/59671が使用中なら別の未使用high portを選びます。
新規stageだけを使い、過去の使用済みrootは再利用・消去しません。

```powershell
$env:BEDROCKINSIGHT_CLANGCL_ROOT='C:\Dev\LL2660Toolchain-20261029\clang+llvm-23.1.4-x86_64-pc-windows-msvc\bin'
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$stage = scripts/stage_isolated_capture.ps1 -CandidateRoot .build/server-candidate/run-0388efb2490c4235b9cae627af0f7f60 -ServerPort 59670 -ServerPortV6 59671 -PassThru
scripts/prepare_capture_executable.ps1 -SandboxRoot $stage.sandbox_root
scripts/check_capture_isolation.ps1 -SandboxRoot $stage.sandbox_root -RequirePatched
$stage.sandbox_root
# 全検査PASSを確認。端末Bへこの新rootのパスを伝える。
scripts/run_isolated_capture.ps1 -SandboxRoot $stage.sandbox_root -StartupTimeoutSeconds 90 -SmokeHoldSeconds 30
```

runnerは自分が起動した子だけにstopを送り、通常停止か強制終了かも記録します。
online-mode=true・空allowlist・no cheats・HTTP/rollback無効は維持します。
load／enableだけでは受け入れ成功ではありません。

## 2. 並行する通常ホスト端末B：生きた実測値を保存

端末Aの起動と並行して実行します。`<NEW_ROOT>`には直前の新rootだけを指定します。
Nodeも既存のインストールを使います。待機は最大180秒、サンプル取得後はすぐ終了します。

```powershell
$proof = Join-Path (Get-Location).Path ('.build/mspt-evidence/run-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $proof | Out-Null
python -m companion.verify_ticks --database <NEW_ROOT>/mods/BedrockInsight/data/bedrock_insight.sqlite3 --wait-seconds 180 --require-busy --ui-dom | Set-Content -LiteralPath (Join-Path $proof 'live.json') -Encoding UTF8
if ($LASTEXITCODE -ne 0) { throw 'Live MSPT/API/DOM verification failed; do not claim PASS' }
Join-Path $proof 'live.json'
```

実SQLiteを読み取り専用で開き、独自の一時127.0.0.1ポートで未認証401／認証200を検査します。
実際の日本語HTML/JSでそのAPI結果をDOMへ反映し、数値と計測理由を照合します。
トークンは出力・保存せず、検証器自身のHTTP listenerだけを閉じた後にJSONを保存します。
これはDOM検証であり、ブラウザーのスクリーンショットではありません。
人数が安全に取得できずnullでも、実測MSPT/TPSと「未計測」の人数表示を検証できます。

## 3. 端末Aのrunner終了後：耐久性・2窓・正常停止の照合

```powershell
scripts/verify_mspt_capture.ps1 -SandboxRoot <NEW_ROOT> -LiveReport <端末Bが出したlive.jsonの絶対パス>
```

この検証器はBDSの起動・停止・再起動をしません。次の条件を満たす場合だけ整合性PASSを出します。

- 実行中ではない新規隔離root、正常なstop・exit0・ロード／enable／ready、正しい候補DLLハッシュ。
- WAL/SHMのない停止済みDB。元DBを読み取り専用ロックで保持し、新しい証拠フォルダへコピー。
- コピーだけをimmutable/read-onlyで検査し、schema4・integrity=ok・外部キー問題なし。
- 同一起動のstart→2窓以上の有効なbusy実行→end。中断・未知版・別起動・0実tickは拒否。
- 同じ元DB・実行時間内のlive認証API結果が、保存されたサンプルとMSPT/TPS等で一致。
- 日本語DOMの数値も一致。停止後のコピーを現在稼働中の値として公開しません。

元DBのハッシュ不変も記録します。WAL/SHMが残ったら削除やcheckpointで無理に通さないでください。
証拠JSONの整合性検査であり、編集された証拠に対する独立認証ではありません。
フィクスチャ試験のPASSを実BDSのPASSに置き換えてはいけません。

実サンプルを人が見る別コンパニオンは、承認済み新規BDSが稼働中の場合にのみ
`http://127.0.0.1:18768/`等の別listenerを使い、その起動時のランダム閲覧トークンで認証します。
既存18767番は変更しません。操作・私設SSHポート転送の説明は[分析画面の説明](ANALYTICS_DASHBOARD.md)を参照してください。

実機→SQLite→認証API／日本語UI→正常停止が揃った後にのみMSPT受け入れを更新し、
その後にLK-Stats参照の行動統計段階へ進みます。
