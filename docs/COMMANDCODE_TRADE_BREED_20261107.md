# Command Code: villager trade + breeding capture (2026-11-07)

New post-origin MCAPI hooks in this pass. Reference only: LeafKnife/Stats v0.13.0
(AGPL-3.0). No upstream code or translations were copied; semantics were
re-derived from the pinned 26.60 headers.

## Implemented

| CUSTOM key | Hook | Success semantics |
| --- | --- | --- |
| `minecraft:talked_to_villager` | `ServerPlayer::$openTrading(ActorUniqueID const&, bool)` | After the original call, the target id is resolved through the level. Only `villager`, `villager_v2`, or `wandering_trader` count. Opening the trade screen is the completed LK action; the pre-origin right click is not counted. |
| `minecraft:animals_bred` | `BreedableComponent::mate(Actor&, Actor&) -> vector<WeakEntityRef>` | `mLoveCause` is copied **before** origin (a successful mating may clear it). After origin, a non-empty offspring list is required, then the love cause is resolved to an online `Player`. Feeding without offspring never counts. |
| `minecraft:target_hit` | `TargetBlock::$onProjectileHit(BlockSource&, BlockPos const&, Actor const&)` | After origin, the projectile owner is resolved to an online `Player`. An unowned/environment projectile is never attributed. |

All three adapters follow the existing lifecycle contract:

- the hook stays installed and passes through when the token is inactive;
- callbacks are rejected after the activation generation changes;
- only owned primitive/string values cross into the bounded SQLite writer;
- no engine pointer is dereferenced after `origin`;
- a nested/reentrant call is never counted.

## SQLite / reader

Events are written as `EventKind::PlayerInteractionResult` with
`player_interaction_result`:

- cause `ll26.60.open_trading_villager.v1`, primary `minecraft:talked_to_villager`;
- cause `ll26.60.breed_offspring.v1`, primary `minecraft:animals_bred`;
- cause `ll26.60.target_block_projectile_hit.v1`, primary `minecraft:target_hit`;
- `amount = 1`.

`StatsReader` surfaces all three keys in `minecraft:custom` and allows ranking
only with the exact `custom:minecraft:<key>`. Unit mixing remains refused.

## Evidence

- `player_trade.cpp` and `player_breed.cpp` compile with `/W4 /WX` against the
  pinned LL 26.60 headers.
- Full server-function candidate build passes prelink/link/import resolution:
  DLL SHA256 `89DC8A3FD1E45DCE21B9E93CAABC09C1AE74DAFB2B7DB37213C55BD9674391FB`,
  ZIP SHA256 `707ADB46B6933C3A23BEA28176AFB8F06A2DBC82466FFE9B7ED8F5E558C7E0E0`
  (`.build/server-candidate/run-71022aa4b5e149af9b01030339422df9`).
- `stats_reader_tests` insert fixture rows for both causes, assert the CUSTOM
  rows and per-key rankings, and still assert that failed/interaction-pre
  notifications are excluded.
- The delay-import allowlist was extended by exactly the reviewed MCAPI symbols
  used here (`?$openTrading@ServerPlayer@@...`, `?mate@BreedableComponent@@...`,
  `?$onProjectileHit@TargetBlock@@...`, `?getOwner@Actor@@...`).
- Fresh isolated BDS `.build/isolated-bds/run-368608bfa2c342c482e9f34ee0713fb3`
  loaded this exact DLL: `native_loaded=true`, `native_enabled=true`,
  `server_started=true`, `exit_code=0`, `forced_termination=false`,
  `database_exists=true`.

## Still not proven

No real player has opened a villager trade, bred an animal, or hit a target
block with this DLL. Event-to-SQLite for these three keys is fixture-verified
only. The isolated BDS run has an empty allowlist and does not exercise gameplay.
