# Command Code parity pass — 2026-11-07

Concurrent-agent note: a Codex session is editing/building this same tree at the same time.
To avoid corrupting its work this pass limits itself to small, verified source fixes plus a
new doc, and does **not** claim full parity or player-test readiness. `STATUS.md`,
`docs/LK_FULL_PARITY_MATRIX.md` and `docs/PLAYER_ACTION_HOST_VALIDATION.md` are being
rewritten by that other pass.

Live BDS, companion on 18767, auth/firewall and other projects were not touched. No git
push/release.

## Source changes (all compile with pinned LL26.60 headers)

1. `src/bedrock_insight/levilamina/runtime.cpp` — **shutdown ordering fix**.
   Pending travel distance is now flushed via `finish_travel()` *before*
   `capture_->storage` is moved out in `stop_storage()`. Previously the emit callback went
   through `submit()`, which dereferences `capture.storage`, after the writer had been moved
   to a local (`capture.storage == nullptr`) — pending travel could be lost or crash on
   disable within the 5 s coalescing window.
2. `src/bedrock_insight/levilamina/player_kills.cpp` + `core/direct_kill.hpp` —
   projectile/indirect kill attribution. Accepts `EntityAttack(2)` and `Projectile(3)`;
   for a child projectile the owner is `source.getEntityUniqueID()` and the projectile is
   `getDamagingEntityUniqueID()`, so unequal ids are now valid. Non-child sources still
   require equal ids, an entity source, a resolved online player and `self == false`.
3. `src/bedrock_insight/levilamina/player_death.cpp` — `killed_by` now stores the attacking
   **entity type** (projectile owner resolved via `getEntityUniqueID()`), falling back to
   `cause:N` for environmental deaths, instead of an opaque cause code only.
4. `src/bedrock_insight/levilamina/player_hurt.cpp` — `broken` is now gated on real
   durability exhaustion: pre-origin `getDamageValue() + deltaDamage >= max_damage`, in
   addition to damageable item, `mCount == 1`, same inventory/slot and 1→0 after origin.
5. `src/bedrock_insight/core/travel_observation.hpp` +
   `levilamina/player_travel.cpp` — `travel_step_is_continuous()` rejects
   64-block-per-tick discontinuities (teleport/pearl/respawn) without fabricating
   walked distance, and a dimension change resets the baseline without counting.
6. `src/bedrock_insight/core/stats_reader.cpp` — CUSTOM now surfaces the **real measured**
   `minecraft:damage_taken` (`actor_hurt_event.v1`) and `minecraft:damage_dealt`
   (`actor_hurt_real_damage.v1`), and both are rankable only via an explicit
   `custom:<key>`. Attack-action events still never count as damage. Interaction
   pre-notifications remain refused because they do not prove a completed action.

## Tests updated/added and actually run

- `tests/parity_observation_tests.cpp`: travel continuity (normal vs teleport/NaN).
- `tests/direct_kill_tests.cpp`: projectile/child-owner acceptance and mismatch rejection.
- `tests/stats_reader_tests.cpp`: real damage rows + per-key damage ranking; attack actions
  excluded; interaction keys still refused.

Commands and results:

```powershell
# isolated syntax check of all 6 changed sources against pinned 26.60 headers
.build\syntax-check\check.ps1        # exit 0 for all files
# build+run the four affected core tests (throwaway SQLite)
.build\syntax-check\run-tests.ps1    # parity/direct_kill/stats_reader/remote_api all PASS
# fake-SDK lifecycle safety (no BDS)
scripts\verify_native_runtime.ps1    # PASS
```

## Full native candidate (built by the concurrent pass, current source)

- `.build/server-candidate/run-f13f2046ad01465d9426d6582de8c656/`
- DLL `7EDC44BCA007464ACD1762D360525DBA1C58039C90417EF27EFD72CA4384E8D1`
- ZIP `5E6028245410E7FB2EACBF8890126F6BE73DBFE47EF8C411CB384711E69AF223`
- prelink completed, official server import resolution verified, packaged.

## Independent fresh isolated BDS load (this pass)

- Sandbox: `.build/isolated-bds/run-5c85105ad7514f549bf8b5428b45ec27`
  (new root, ports 59950/59951, empty allowlist).
- Candidate DLL `7EDC44BC…8E8D1` (same as above).
- Evidence `capture-runtime-evidence.json`:
  `native_loaded=true`, `native_enabled=true`, `server_started=true`,
  `database_exists=true`, `exit_code=0`, `forced_termination=false`,
  `startup_outcome.stop_reason=ready_hold_completed`.
- This proves the DLL loads, enables and the server starts/stops cleanly on genuine
  BDS 1.26.60.6. It does **not** prove any real player action reaches SQLite/GUI
  (`event_to_db_verified=false`) and is not a full-parity acceptance result.


## Remaining truth gaps (unchanged by this pass, still IN_PROGRESS)

- Interaction keys (chests, workstations, cauldron, noteblock, bell, cake, flower pot,
  jukebox, shulker) come from a **pre-outcome** `PlayerInteractBlockEvent`; they are
  recorded as observations but deliberately not surfaced as successes.
- `animals_bred`, `fish_caught`, `talked_to_villager`, `traded_with_villager`,
  `enchant_item`, `raid_trigger`, `raid_win`, `target_hit`, absorption/resistance and
  shield-block keys remain BLOCKED on symbols/events not exported by LL26.60/prelink
  (`ServerPlayer::$openTrading`, `BreedableComponent::mate`, `BucketItem::$_useOn`,
  `Player::$_hurt`, Trade-container `_handleTransfer`, `SharedAttributes::ABSORPTION`).
- Damage values are the pre-origin hurt amount (tenths); absorption/resistance splitting is
  not measurable with the available API.
- No real player action → SQLite → GUI/API proof exists for this candidate.
