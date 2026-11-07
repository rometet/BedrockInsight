# 移動距離の実装境界（2026-11-06 / 2026-11-07更新）

> **2026-11-07 更新:** 以下の「未計測」は解消しました。`$checkMovementStats` は pinned
> prelink runtime (`bedrock_runtime_api.lib`) に**エクスポートされておらずリンク不可**と
> 実ビルドで確認したため、フックではなく `ServerLevelTickEvent` 上の座標差分ポーリング
> (`ll26.60.poll_position_delta.v1`、5秒集約) として実装しました。使用APIは
> `getPosition`(inline) / `isSprinting` / `isSneaking` / `isSwimming` / `isGliding` /
> `isFlying` / `isInWater` / `isOnGround` / `getVehicle` / `getDimensionId` のみ。
> 実プレイヤーでの距離検証は未実施で、teleport・補正・外力・欠落tickは欠測し得ます。
> 詳細は `docs/LK_FULL_PARITY_MATRIX.md`。

移動距離の旧記録（2026-11-06 時点）は未計測でした。今回、距離を記録するフック・イベント・
DB変更は追加していませんでした（上記の更新で実装済み）。公式宣言が存在することと、検証済み移動の完了を
観測できることは別です。

確認対象は公式LeviLamina26.60.0の
`072b9ffe0e9d9e0ec846f11d294880e892fe7770`。既存の検証済みヘッダーを読みました。

|参照|確認した事実|まだ証明できないこと|
|---|---|---|
|[Actor.h](https://github.com/LiteLDev/LeviLamina/blob/072b9ffe0e9d9e0ec846f11d294880e892fe7770/src/mc/world/actor/Actor.h)|getPosition/getPosDeltaはStateVectorComponentの参照|任意のtick時点が入力検証・補正完了後であること、座標差の原因|
|[Player.h](https://github.com/LiteLDev/LeviLamina/blob/072b9ffe0e9d9e0ec846f11d294880e892fe7770/src/mc/world/actor/player/Player.h)|$tickWorld、$teleportTo、$respawnはMCAPI|ServerPlayer側の全転送経路・補正・リスポーンを捕捉すること|
|[ServerPlayer.h](https://github.com/LiteLDev/LeviLamina/blob/072b9ffe0e9d9e0ec846f11d294880e892fe7770/src/mc/server/ServerPlayer.h)|$normalTickはMCAPI。$respawn/$checkMovementStatsはMCNAPI|normalTick終了時が移動の正当性を保証する境界であること|
|[PlayerRespawnEvent publisher](https://github.com/LiteLDev/LeviLamina/blob/072b9ffe0e9d9e0ec846f11d294880e892fe7770/src/ll/api/event/player/PlayerRespawnEvent.cpp)|条件付きでoriginより前に通知|転送完了時点・すべてのリスポーンの検出|
|[HasTeleportedFlagComponent](https://github.com/LiteLDev/LeviLamina/blob/072b9ffe0e9d9e0ec846f11d294880e892fe7770/src/mc/entity/components/HasTeleportedFlagComponent.h)、[IsBeingTeleportedFlagComponent](https://github.com/LiteLDev/LeviLamina/blob/072b9ffe0e9d9e0ec846f11d294880e892fe7770/src/mc/entity/components/IsBeingTeleportedFlagComponent.h)、[ServerPlayerTeleportingFlagComponent](https://github.com/LiteLDev/LeviLamina/blob/072b9ffe0e9d9e0ec846f11d294880e892fe7770/src/mc/entity/components/ServerPlayerTeleportingFlagComponent.h)|型の宣言と一部フィールドは存在する|付与・解除の順序と全経路の網羅性。名前だけからフラグの寿命を推測しない|

LeafKnife/Stats固定版の
[入力フック](https://github.com/LeafKnife/Stats/blob/5bb8bfb7e92c28e03bdd02d93ba7cc8b6c15bb40/src/mod/Hook/Player/PlayerAuthInputHook.cpp)と
[移動ハンドラー](https://github.com/LeafKnife/Stats/blob/5bb8bfb7e92c28e03bdd02d93ba7cc8b6c15bb40/src/mod/Stats/Handlers/PlayerMovementStatsHandler.cpp)
は技術的事実だけ参照しました。入力処理後のコールバックから、複数の移動分類に
パケットの差分を使用しています。その差分を当プロジェクトで正当な移動距離として
採用しません。コード・コメント・翻訳をコピーしていません（AGPL-3.0）。

速度上限だけでは小さいテレポート・補正・外力を除外できず、間引いた座標間の
直線距離は完全な経路長でもありません。次の実装には、サーバー側での検証完了境界、
テレポート/次元/リスポーン/乗車のリセット条件と、実際の通常ホスト試験が必要です。
この調査は「26.60にAPIがない」という結論でも、実装完了でもありません。
