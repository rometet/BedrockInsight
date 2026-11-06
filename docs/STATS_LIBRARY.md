# 9分類・CUSTOM77項目の日本語統計ライブラリ

この追補は読み取り専用Webの実装です。**全機能同等性の完成宣言ではありません。**
キーを選べること、SQLite fixtureで値が出ること、DLLがロードされることと、
実プレイヤーの行動を正しく収集できることは別の受入条件です。
`PROJECT_STATUS: IN_PROGRESS` を維持します。

## 起動済みの18767は更新しない

新ルートは `/stats`、APIは `/api/v1/stats` です。既存18767のプロセス、旧ページの
`index.html` / `app.js` / `styles.css`、BDS、world、SQLiteには配備・変更しません。
この実装作業で追加の常駐viewerは起動していません。

利用者が別の空きポートを確認し、承認済みDBを明示して新プロセスを起動する手順例:

```powershell
python -m companion.server --database <承認済みのBedrockInsight SQLiteの絶対パス> --port 18768
```

`http://127.0.0.1:18768/stats` を開き、その新しい起動端末にのみ表示されるランダム
トークンを入力します。これは**手順例であり、18768で稼働中という意味ではありません**。
既存18767のトークンは流用しません。トークンは8時間で期限切れ、ブラウザーではメモリ内のみ、
URL・Cookie・localStorageには保存しません。ログアウト／401時は取得した名前と値を消去します。
認証前に配信されるHTML/JSには、プレイヤーデータやトークンを埋め込みません。

別PCからは既存の私設SSH経路のloopbackポート転送のみを利用してください。
LAN/Tailscale/public待受、認証無効化、firewall/proxy/ユーザー設定の変更は不要です。
ポート使用中なら既存プロセスを止めず別ポートを指定します。Ctrl+Cは自分で起動した
新コンパニオンだけに使用し、BDSや既存18767を停止しないでください。

DBは既存の読み取り専用接続からメモリへbackupし、同じsnapshotで応答を生成します。
SQLite v1〜v4の既存読取条件を再利用し、migration・書込・checkpointはしません。
保護対象の稼働DBでWAL/SHMが揃わない場合、ファイルを作らず503にします。
原本を手動コピーせず、既に承認済みの停止後コピーまたは既存読取条件に合うDBを指定します。

## 操作

1. 分類を選びます。CUSTOMなら77キーから1つ選択します。
2. 全期間／日／月を選択します。日付境界と画面表示はJST、保存値はUTC epoch msのままです。
3. 全プレイヤーまたは個人を選び、種類別明細と同じ期間・種類の全体順位を確認します。
4. 種類ID／日本語名の部分検索、正確な名前空間付き種類IDで絞れます。
   明細の「この種類で順位を比較」でも指定できます。
5. 種類、順位、プレイヤー一覧は独立した前後ページです。新しい要求が失敗したり、
   古い要求が遅れて完了しても、以前の値を最新値のように残しません。

明細・順位は通常20件／ページ、APIのlimitは1〜100、page/rank_page/players_pageは0〜1000。
プレイヤーは100人／ページです。200種類で集計を打ち切らず、合計と種類数は全該当行から計算します。
順位は正の値だけを降順、同点は名前のbinary順、内部ID順で固定します。
応答のプレイヤー参照はプロセスごとのHMACで、生UUID・source actor IDは返しません。
JSの安全な整数範囲を超えた値は丸めて表示せず「表示精度の範囲外」とします。

## 9分類の読取定義

すべて実プレイ受入は別途必要で、`collection_complete:false` です。

| 分類 | この画面で数える保存値 | 数えたと主張しないもの |
| --- | --- | --- |
| mined | GameMode true応答＋同座標の種類変化、回 | 破壊試行、全破壊経路、保存完了 |
| broken | 主所持品同一slotの耐久閾値と1→0消失、回 | 防具／副手など全破損の網羅 |
| crafted | 成功要求の出力種類・数量の申告、個 | 最終inventory commit、NBT、精錬、取引 |
| used | useItem true応答＋主所持品照合、回 | 効果完了、消費成立、全用途の使用数 |
| picked_up | take true応答中の主所持品同種類の増加、個 | 入口stack個数、全拾得、永続転送 |
| dropped | 非randomlyのdrop true応答時の入口個数、個 | 在庫差分、生成物の保存完了 |
| killed | 直接攻撃／子飛び道具所有者の解決＋死亡フラグ、回 | 全キル、環境・不明・自己帰属 |
| killed_by | 新ソースの解決済み死亡元エンティティ種類、回 | 原因コードをエンティティに見立てること |
| custom | 選んだ1キーの限定観測、キー固有単位 | 個数・距離・時間を足した総合得点 |

新しく補ったcraft/broken/移動/sleep/sneak/繁殖/村人の読取では、hook・phase・bool/intの型、
数量や関連フィールドの一致を確認し、不正JSON・重複キー・NaN/Infinityを拒否します。
未対応source、処理前通知、旧ヒューリスティックを確定結果へ置き換えません。
その他の分類も既存の専用source validatorと種類・時刻・actor条件を通した行だけです。

## CUSTOM: 27の限定読取と50の未対応

77項目すべてに独自の日本語表示ラベルがあります。分類やキーの登録は収集証明ではありません。
この画面の `projection_supported` は**読取SQLがある**という意味であり、
ネイティブフック／GUI／RemoteCallの受入完了や全経路対応を意味しません。

限定読取を接続した27キー（並行作業の`target_hit`読取追加も保持）:

- `animals_bred`, `talked_to_villager`: 新sourceのoffspring非空／Player帰属の申告、
  openTrading正常終了後のtarget解決。子の生存、画面の実表示、取引commitを確認した値ではありません。
- `drop`, `jump`, `deaths`, `mob_kills`, `player_kills`: 分類別の限定観測。
  `killed_by`は死亡件数とは独立し、二重加算しません。
- `sleep_in_bed`, `sneak_time`, `leave_game`: 睡眠フラグ遷移、通知間実時間の50ms換算、
  `levilamina_player_event` の切断保存イベント。真の継続tick、未記録退出は補いません。
  スニークの各正区間は最小1換算tickで、全量を保存時刻へ配賦し、期間境界で分割しません。
- `play_time`, `time_since_death`: 完結セッションを統合した下限の50ms換算tick。
- `target_hit`: 新しいTargetBlock post-origin sourceのPlayer所有者解決・飛び道具種類一致の申告。
  処理前相互作用から推測せず、ネイティブ所有者寿命・帰属・実命中の実機受入とは分離します。
- `walk/sprint/crouch/swim/fly/aviate/fall/boat/minecart/pig/strider/horse/nautilus/happy_ghast_one_cm`:
  各々独立した14キー。5秒の位置差分バッチを保存時刻で期間分けします。
  境界を跨いだ距離の復元、全移動モード、全テレポート除外は未検証です。

残り50キーはこの読取経路では `status:unsupported`、値null、順位なしです。
たとえばdamage_*、大釜・容器・設備の処理前相互作用、襲撃、釣り、エンチャント、
登攀／水面歩行／水中歩行、time_since_rest/total_world_timeなどを推測しません。
別のネイティブ候補があっても、この画面の限定読取を自動的に実機受入済みへ
昇格しません。未対応表示も公式API不存在の宣言ではありません。

時間集計はJST半開区間と完結セッションの交差をとり、重複・入れ子・連続重複を統合します。
未終了、未来終了、異常duration、オフライン時間を加算しません。死亡後時間は当期間末までの
最後の正当な死亡以降の完結時間だけです。履歴のない部分を補完しない下限です。
実時間を50msへ換算するため、サーバーの実tickカウンターと同等ではありません。

`no_record`は読取経路はあるが正の記録なし、`unsupported`は当該読取未対応です。
どちらも総計nullです。実機で連続収集を証明していないため、欠測を「0回」としません。
最初／最後の記録時刻も、その間の収集連続性を保証しません。

## APIと検証範囲

`GET /api/v1/stats`。Authorization Bearer必須、同一Origin/loopback Hostのみ。
パラメータはcategory, key, period, date, player, search, type, page, rank_page, limit, players_page。
分類は短縮名または`minecraft:`付き、CUSTOMキー・種類は名前空間付きです。
値はbindし、検索は`instr`による文字通りの部分検索（%と_もwildcardにしない）。
不明・重複・範囲外指定400、不明なopaque player404、期限切れ401、DB待機／非対応503です。
書込HTTPは405。これは独自のWeb読取APIで、要求された4つのRemoteCall APIや
LegacyRemoteCall wire互換の代わりではありません。

テスト: `python -W error::ResourceWarning -m unittest discover -s tests -p 'test_*.py' -v`。
`test_stats_view.py`は9分類、77キーの選択、27限定読取、期間・型・欠測・253種類ページング・
auth/read-only/historyをfixtureで確認し、実HTTPのJSONを`stats_library_ui_tests.cjs`へ渡して
実際のJSのDOM反映、単位、日本語／未知ID、名前のliteral表示、検索、独立ページ、
古い応答の除外、503、logout/401の消去を検査します。実ブラウザー画像や実プレイの証明ではありません。

元の4区分（プレイ時間・行動・ランキング・TPS/MSPT）は `/` と `/actions` も引き続き利用できます。
真のMSPTの既受入は変更せず、停止後・古いsampleを現在値にしません。
全9/77、ゲーム内3コマンド、4呼出しAPI、全翻訳、実操作→SQLite→GUI/APIの残り受入は
[全機能マトリクス](LK_FULL_PARITY_MATRIX.md)で追跡し、未完了のままです。
