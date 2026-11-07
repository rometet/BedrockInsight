# 他Mod向け読取API（独立C ABI・候補実装）

公開ヘッダは `src/bedrock_insight/core/stats_api.h`。C11 / C++20 で利用でき、
JSON以外のC++オブジェクト、SQLiteハンドル、所有メモリを境界に渡しません。
これは **LegacyRemoteCallの互換実装ではありません**。稼働中BDSに導入・更新する
手順ではなく、候補実装の契約です。既存BDS・18767は変更しないでください。

## 4関数

`broken`の個人・順位応答にも限定`scope`、v2/v1の`source_contracts`、
`final_world_commit_verified:false`を追加。主所持品・副手・防具4枠の同stack消失と
入力耐久閾値の観測であり、全破損や保存の証明ではありません。
[v2・履歴・欠測条件](BROKEN_CAPTURE_V2.md)。4関数のABIやページ範囲は変更しません。

Windows x64、明示的な `__cdecl`、UTF-8 NUL終端入力、呼出側所有の出力バッファです。
ヘッダの関数ポインタ型は、順に `bi_get_player_stats_fn`、
`bi_get_rank_stats_fn`、`bi_get_player_stats_page_fn`、`bi_get_rank_stats_page_fn`。

| シンボル | 入力（最後にすべて `char* out, int capacity`） | 応答 |
| --- | --- | --- |
| `getPlayerStats` | `const char* player, const char* category` | 個人統計の先頭100項目 |
| `getRankStats` | `const char* category, int page, int limit` | 指定ページのランキング |
| `getPlayerStatsPage` | `const char* player, const char* category, int page, int limit` | 個人統計の指定ページ |
| `getRankStatsPage` | `const char* category, int page, int limit` | 指定ページのランキング |

- `page` は **0始まり、0..1000**。`limit` は1..100。自動補正はしません。
- `player` は保存済みの正確な表示名または内部ID。返却するのは表示名のみで、生のUUID/XUIDは出しません。
- カテゴリは9種類の短縮名、`minecraft:` ID、日本語カテゴリラベルを受理します。
- CUSTOMランキングは `custom:minecraft:walk_one_cm` のようにキーを明示します。
  単位混在の `custom` 総合ランキング、共有世界時間の個人ランキング、盾など未対応キーは拒否します。
- 各呼出は別のSQLite読取専用接続を使います。ゲームスレッドから高頻度に呼び出さず、
  呼出側で低頻度にキャッシュしてください。全期間の集計であり、このC ABIにJST期間引数はありません。
  日/月フィルタは既存の認証付きWeb APIを利用します。

戻り値が正なら、NULを除いたJSONのバイト数です。**正の戻り値でも必ず `ok` を確認**してください。
DB未設定・読取失敗・未対応キー・プレイヤー未検出などは `{"ok":false,"error":"…"}`。
負の戻り値の場合、有効なバッファなら `out[0]` をNULにします。

| 戻り値 | 意味 |
| --- | --- |
| `-1` | NULLの入力文字列 |
| `-2` | ページ/件数の範囲外、NULL出力または容量<=0 |
| `-3` | NULを含めた容量不足。必要容量を返すプローブ方式ではありません |
| `-4` | 不明カテゴリ |
| `-5` | 境界で捕捉したC++例外 |

メモリの有効性は信頼された呼出側の責任です。任意ポインタの安全性、アクセス違反、
別Modの破壊をこのAPIで隔離できるという保証はありません。

## 応答とページの意味

個人応答は `player_name/category/category_label/total/page/limit/total_entries/has_more/entries`、
各項目は `key/label/value`。ランキング応答は `category/category_label/page/limit/total_entries/has_more/entries`、
各項目は `rank/name/value` です。どちらも `collection_complete:false`。
型が列挙されていても全履歴・全用途の収集済みを意味しません。

ランキングの `total_entries` は正の記録があるプレイヤー数で、ゼロや欠測は除外します。
値降順、名前のバイナリ順、内部IDの順で決定します。名前が同じでも別プレイヤーを合算しません。
内部IDはタイブレークにだけ使用し、出力しません。表示名は一意の公開識別子ではありません。

行と総件数は **同じ1本のSQLite statementの読取スナップショット**から取得します。
範囲外ページも `total_entries` を維持し、`entries:[]/has_more:false`。
別リクエスト間はスナップショットを固定しないため、更新によりページが動く可能性があります。

未記録カテゴリの個人 `total`、異なる単位を含むCUSTOM合計は `null`。
ランキングの空配列は「正の保存済み記録なし」であり「全プレイヤーの実測0」ではありません。
値は整数JSONです。64bit整数を正確に扱えるJSONライブラリを使ってください。
CUSTOMの距離はcm、ダメージは0.1HP単位、時間キーは定義されたtick相当などで、単位を混ぜません。
`time_basis:closed_session_lower_bound_50ms_ticks` は個人応答の時間キーに関する注記であり、
採掘など他の値を時間に換算するものではありません。

## 呼出側のロード方針・信頼境界

1. BedrockInsightのenable後に `GetModuleHandleW(L"BedrockInsight.dll")` で**既にロード済みのMod**を取得。
2. 4つの正確なシンボル名を `GetProcAddress` で解決し、公開ヘッダの関数ポインタ型を使用。
   NULLなら機能を無効化し、未対応を表示。旧DLLに新しいページ情報があるとは仮定しません。
3. 出力バッファを渡し、戻り値・JSON `ok`・期待フィールドを確認。
4. providerのenable期間外は呼び出さない。`GetModuleHandle` の結果を `FreeLibrary` しない。
   **照会のためにModを `LoadLibrary`、再ロード、差し替えしてはいけません。**

HTTPの認証トークンをこのAPIに渡す必要はありません。同じプロセスにロードされたネイティブModは
既に全面的な実行権限を持ち、このAPIは認証境界を追加しません。ネットワーク公開・IPC・
コマンド登録・書込関数は増やしていません。Webは従来どおりloopback+認証+読取専用です。

`BI_STATS_API_CONTRACT` はヘッダの契約名であって、稼働DLLのバージョン検出関数ではありません。
DB設定関数は公開C ABIではありません。実Modは既存のload処理でDBを設定します。

## LK-Stats / LegacyRemoteCallとの差分（未完了）

固定参照は [LK-Stats v0.13.0のRemoteCall契約](https://github.com/LeafKnife/Stats/blob/5bb8bfb7e92c28e03bdd02d93ba7cc8b6c15bb40/src/mod/Stats/RemoteCall.cpp)。
API名・引数/値型・ページ基準などの事実だけを調査し、上流の実装・コメント・翻訳はコピーしていません。

| 項目 | 上流参照の契約 | 現在の独立C ABI |
| --- | --- | --- |
| 登録先 | RemoteCall namespace `LK-Stats` | DLLの4つのCシンボル。namespace登録なし |
| 個人指定 / カテゴリ | UUID文字列 / 整数enum | 保存済み名前または内部ID / 名前空間文字列 |
| 非ページAPIの返却 | 文字列→unsigned64値のmap | 呼出側バッファ内のJSON（signed64値） |
| ページ基準 | 1以下を先頭にする1始まり契約 | 厳格な0始まり |
| 依存 | LegacyRemoteCall0.23系 | 依存追加なし、互換性なし |

つまり **上流の既存RemoteCall利用Modは、そのままでは接続できません**。
確認したローカルtoolchainの2つのxmake package cacheにはLegacyRemoteCall0.23 SDKがありません。
2026-11-07に公式GitHubのv0.23.0ヘッダ/ライセンス/ビルド定義をWebで確認しようとしましたが
cache missで取得できず、正確な依存物とライセンスの確認は未了です。
不明ABIを手書きで偽装したり、既存BDSへ依存Modを勝手に導入したりはしていません。
正式なSDK/ライセンス確認と実際の別Mod連携が必要であり、full parityのAPI受入は未達です。

今回、[公式リポジトリ概要](https://github.com/LiteLDev/LegacyRemoteCall)のmainにはLGPL-3.0表示が
あることをWebで確認しました。ただし固定v0.23.0のLICENSE／APIヘッダーは再びcache missで、
このmain表示を固定版ライセンス／ABIの確認済み証拠にはしていません。依存物の導入・コピーなし。

## 採掘v2の読取契約追補

minedは[状態変化証拠の厳格な検証](BLOCK_RECORDS_V2.md)を個人・種類・順位へ共通適用します。
amount=1、既知v1/v2の型付きJSON、actor/type/時刻等を検査。不完全な旧行は保存したまま集計対象外です。
応答にscope/source_contracts/final_world_commit_verified:falseを追記しますが、4関数のABI・
正負の戻り値・ページ方式は変えません。欠測null・collection_complete:falseを維持します。

## 再現テスト（BDSを起動しない）

キャッシュ済みclang-cl/SQLiteを設定して `./scripts/verify_native_stats_api.ps1`。
新規 `.build/c-api-tests/run-<GUID>` だけにfixtureを作り、次を実行します。

- 本番の `remote_api.cpp/stats_reader.cpp/sqlite_api.cpp` を**テスト専用core DLL**へリンク。
  DB作成/設定のshimはテスト専用で、本番Modのビルドには含めません。
- C11の独立EXE（C++/SQLite/provider import libraryに依存しない）がそのcore DLLだけをロードし、
  4関数を動的解決して実際に呼出。テストshimのDB作成は既存ファイルを拒否。
- ページ境界、正の記録だけのランキング、同点の名前順、引用符/改行/バックスラッシュ、
  NULL・容量・範囲の拒否、空カテゴリnull、盾の未対応を検査。
- C側でAPI読取前後のDB全バイト一致、独立Python読取で実応答JSONとclosed DB integrityを検査。
- before/afterソースハッシュを一致確認し、`api-evidence.json` に記録。

これは **本物のDLL境界/C呼出のfixtureテスト**ですが、LLやBDSにロードするテストではありません。
既存/新規BDSは起動せず、18767へのアクセスもありません。通常ホストの新規隔離BDSで、
実プレイヤー記録・ゲーム内GUI・別Mod呼出の検証が残ります。
