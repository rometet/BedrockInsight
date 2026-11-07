# クラフトAPI調査：まだクラフト成立数を有効化しない理由

対象は公式LL26.60.0のcommit `072b9ffe0e9d9e0ec846f11d294880e892fe7770`、
参照はLeafKnife/Stats v0.13.0の `5bb8bfb7e92c28e03bdd02d93ba7cc8b6c15bb40` のみ。
AGPL-3.0のコード・コメント・翻訳は転載せず、宣言と処理順の着眼点を独立調査しています。

|経路|固定ヘッダの状態|証明できる範囲／不足|
|---|---|---|
|ItemStackRequestActionHandler::_handleTransfer|MCAPI、ItemStackNetResultを返す|個別転送の結果。要求全体のcommit完了ではない|
|ItemStackRequestActionHandler::getScreenContext|MCAPI、const参照|画面文脈。完成個数や要求成功の証明ではない|
|CraftHandlerBase::postRequest(bool wasSuccess)|MCAPI|成功通知を受ける経路は存在。ただし呼出し元の順序と同一要求の転送への対応付けは未検証|
|ItemStackRequestActionCraftHandler::endRequest|MCNAPI|完了経路を未対応シンボルで呼ぶ／hookすることはしない|
|ContainerScreenValidation::tryCommitActionResults、_commit|MCNAPI|実際のcommit成立をこの宣言だけから推測できない|
|_getOrInitSparseContainer|MCAPIだが初期化を含む|観測のために呼んで容器を作る設計は採用しない|

`ItemStackNetResult`には個別転送の成功0とは別にcommit失敗5や終了処理失敗4があります。
したがってCreatedOutputContainerからの転送が成功しただけで、Craft成功数や製作品個数へ
加算するのは不十分です。画面種別によるcraft／精錬／交易等の範囲も別途確認が必要です。
上流の引数名からABIを推測せず、公式の順序（source hint、destination hint、swap）を使用します。

`tests/craft_api_compile_probe.cpp`と`scripts/verify_craft_api_probe.ps1`は、既存の検証済み
ヘッダcompile optionsを使い、3本のMCAPIの型とコンパイラ生成の実importだけを調べます。
このprobeはModにリンクせず、関数を実行せず、新DLLやBDSを作りません。コンパイル成功を
要求commit・エンジンABI・実機クラフト・SQLite保存の成功にはしません。

次に必要なのは、同一要求の識別と寿命を壊さないread-onlyな出力snapshot、完了通知の
呼出し順／取消・後続失敗・入れ子の処理、実インベントリ／数量の照合です。これが未確定の間は
コンパニオンのクラフトを未計測のまま保ちます。既存の使用true応答や投棄・拾得は別統計です。

一次資料：

- [公式転送handler](https://github.com/LiteLDev/LeviLamina/blob/072b9ffe0e9d9e0ec846f11d294880e892fe7770/src/mc/world/inventory/network/ItemStackRequestActionHandler.h)
- [公式CraftHandlerBase](https://github.com/LiteLDev/LeviLamina/blob/072b9ffe0e9d9e0ec846f11d294880e892fe7770/src/mc/world/inventory/network/crafting/CraftHandlerBase.h)
- [公式commit関連宣言](https://github.com/LiteLDev/LeviLamina/blob/072b9ffe0e9d9e0ec846f11d294880e892fe7770/src/mc/world/inventory/simulation/ContainerScreenValidation.h)
- [AGPL参考箇所](https://github.com/LeafKnife/Stats/blob/5bb8bfb7e92c28e03bdd02d93ba7cc8b6c15bb40/src/mod/Hook/ItemStackRequestActionHandler.cpp)
