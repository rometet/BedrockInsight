# Overnight player-test handoff (2026-11-07)

Status: **NOT_READY** for the single real-player acceptance test.

This document reflects the current working tree after the Command Code session that
added the confirmed `GameMode::$useItemOn` interaction path, wired real-damage
reading, and added the committed-trade transfer hook. Full LK-Stats v0.13.0 parity
is still not met, and no real player action has been proven to reach SQLite.

## Why NOT_READY

1. Full 9-category / 77-key parity is not complete. Per-row truth is in
   `docs/LK_FULL_PARITY_MATRIX.md`: CODE_ONLY 58 / UNIT_ONLY 5 / BLOCKED 10 /
   MISSING 4. BLOCKED keys have no legitimate official 26.60 API
   (absorption/resistance/shield splits, `BucketItem::$_useOn`, anvil-vs-enchant
   separation, raid effect grant, rest origin).
2. No real-player action has been proven to reach SQLite and then the in-game
   `/stats` GUI or the four remote APIs for this source. An empty-allowlist BDS
   load/enable is not gameplay acceptance.
3. The four C exports are a documented in-process C ABI. They are **not** proven
   wire-compatible with LK-Stats' LegacyRemoteCall, and no other Mod has called
   them inside a real BDS yet.
4. In-game GUI paging/selectors compile and are linked, but have not been driven
   by a real client.

## Verified this session (current working tree)

Host toolchain: clang-cl 23.1.4 + MSVC 2022 + pinned LL26.60 headers + official
server-derived import library + prelink. Disposable fixtures only; no live BDS,
companion 18767, auth/firewall, or unrelated project was touched.

| Suite | Result |
| --- | --- |
| pinned 26.60 header compile of changed TUs | PASS |
| `scripts/build_server_candidate.ps1` (prelink/link/PE import validation) | PASS |
| candidate DLL SHA256 | `F11BF8510590CA89513F67FB9389DC53DB5353C03D51727EA222270FD8D05322` |
| candidate ZIP SHA256 | `0183CF1ECBF87B06E4802A6921F6AF9F3F6407B98EEA8CEB549D081AC960A119` |
| candidate run | `.build/server-candidate/run-67bd8fd9cd944fc2b61996b9c001bfeb` |
| isolated BDS load `.build/isolated-bds/run-782536caf3f24f52bfe79f70c340f364` | native_loaded/enabled=true, server_started=true, exit_code=0, forced_termination=false, database_exists=true |
| `tests/parity_observation_tests.cpp` | PASS |
| `tests/stats_reader_tests.cpp` / `remote_api_tests.cpp` / `killed_by_storage_tests.cpp` | PASS |
| other core/observation/player-mode suites | PASS |
| Python `unittest discover tests` | 126 PASS |
| `scripts/audit_lk_parity.py` | 96 rows, zero accepted |

## Changes made this session

- `src/bedrock_insight/levilamina/player_interact.cpp`: new MCAPI hook
  `GameMode::$useItemOn`. Emits `PlayerInteractionResult` with cause
  `ll26.60.gamemode_use_item_on_success.v1` only when the returned
  `InteractionResult::mSuccess` is true on the authoritative first event. The
  pre-outcome `PlayerInteractBlockEvent` stays under its own cause and is never
  counted. 33 block-interaction CUSTOM keys (chests, barrels, workstations,
  noteblock, cauldron, cake, jukebox, bell, flower pot, etc.) now have a real
  post-outcome path.
- `src/bedrock_insight/core/interaction_observation.hpp`: `kInteractionKeys`
  (33) is the single source of truth for `interaction_key_valid()` and the
  reader's fixed SQL list; `interaction_success_confirmed()` is the tested gate.
- `src/bedrock_insight/core/stats_reader.cpp`: CUSTOM aggregation and per-key
  ranking accept the confirmed interaction cause. The native real-damage
  producers are now read as `damage_taken`/`damage_dealt` (tenths).
- `src/bedrock_insight/levilamina/player_trade.cpp`: added MCAPI hook
  `ItemStackRequestActionHandler::_handleTransfer`. Counts
  `minecraft:traded_with_villager` only for `ItemStackNetResult::Success` when a
  trade-result-preview container (33/49) moves into the player's carried
  inventory/cursor (12/28/29/59). Ingredient transfers and failures excluded.
- `src/bedrock_insight/core/trade_observation.hpp`: testable container
  classification with pinned ordinals.
- `src/bedrock_insight/levilamina/player_travel.cpp` +
  `src/bedrock_insight/core/travel_observation.hpp`: `walk_on_water_one_cm` /
  `walk_under_water_one_cm` split via `Actor::isUnderLiquid(Water)`.
- `companion/stats_view.py`: projections + strict detail contracts for the
  confirmed interaction cause, both real-damage causes, and the trade cause.
- `scripts/validate_server_release.ps1`: allowlisted only the two reviewed MCAPI
  symbols and raised the bounded delay-import shape from 30 to 48 entries.
- `docs/LK_FULL_PARITY_MATRIX.md`: recomputed every row against the code.

## Required before flipping to READY_FOR_PLAYER_TEST

1. Finish the remaining BLOCKED/MISSING keys where a legitimate official hook
   exists, or keep them explicitly BLOCKED with the grounding already recorded.
2. Perform the real-player smoke below on a brand-new isolated world and confirm
   each row reaches SQLite and the Japanese `/stats` GUI + companion view with
   truthful, correctly attributed values.
3. Re-audit `docs/LK_FULL_PARITY_MATRIX.md` per row; never promote CODE_ONLY to
   accepted without real player action.
4. Prove or explicitly re-document the remote API compatibility status against
   LK-Stats LegacyRemoteCall.

## 10-minute real-player smoke checklist (only after READY)

Run on a fresh isolated BDS with a new world; one operator player. For each item,
verify the in-game `/stats` form and the companion `/stats` page show the expected
type and a positive value; then confirm the row exists in SQLite.

- [ ] `mined`: break a few `minecraft:stone` blocks → `mined` shows stone count.
- [ ] `crafted`: craft sticks/planks → `crafted` shows the item + quantity.
- [ ] `used`: throw a snowball / eat → `used` shows the item.
- [ ] `picked_up`: pick up dropped items → `picked_up` shows the item count.
- [ ] `dropped`: drop a stack → `dropped` shows the count.
- [ ] `broken`: use a low-durability tool until it breaks → `broken` shows it.
- [ ] `killed`: kill a zombie directly and with an arrow → `killed` shows `minecraft:zombie`.
- [ ] `killed_by`: die to a mob (and once to a projectile) → `killed_by` shows the entity type.
- [ ] `custom` walk/sprint/sneak/swim/fly/boat distance → the matching `*_one_cm` keys.
- [ ] `custom` jump, `sleep_in_bed`, `sneak_time`, `leave_game`.
- [ ] `custom` interactions: open a chest/barrel, use a crafting table/furnace,
      ring a bell, eat a cake slice, tune a noteblock, fill a cauldron, wash a
      banner → the matching confirmed interaction keys.
- [ ] `custom` `talked_to_villager` (open a villager) and `traded_with_villager`
      (complete one actual trade) show separate positive counts.
- [ ] `custom` `animals_bred` (breed two cows), `target_hit` (shoot a target block).
- [ ] `custom` `damage_taken` (get hit) and `damage_dealt` (hit a mob) show
      tenths-based values, not attack counts.
- [ ] `/stats rank mined`, `/stats rank custom:minecraft:walk_one_cm`, `/stats player <name> minecraft:custom` open and paginate.
- [ ] Remote API: from the same BDS process, `GetProcAddress` `getPlayerStats`/`getRankStats`/`getPlayerStatsPage`/`getRankStatsPage` return `ok:true` JSON with paging and refusal of bare `custom`.

## Rollback

- Stop the isolated BDS. Remove the candidate DLL/`manifest.json` from the test
  mod directory. Restore the previously deployed DLL if any. No schema downgrade
  is needed: schema v4 is append-only and old databases remain readable.
- Never deploy this candidate to the live BDS or overwrite an existing candidate.

## Known limitations to state honestly during the smoke

- Interactions count when the engine's own `useItemOn` reports success on the
  first event. Screen rendering is not observed, and the echo (non-first) event
  is intentionally ignored, so a rare second-event-only success could undercount.
- `traded_with_villager` counts a committed result transfer; the specific villager
  entity is not identified, and it cannot be distinguished from a wandering
  trader (both use the Trade containers).
- `damage_taken`/`damage_dealt` are the `ActorHurtEvent` amounts in tenths,
  measured before absorption/resistance breakdowns; those splits are BLOCKED.
- Distances are 5-second polling sums and can miss sub-second movement and count
  short teleports as movement; they are not vanilla-tick exact.
- `broken` covers main-hand inventory same-slot durability exhaustion only; armor
  and offhand are out of scope.
- `crafted` reflects a successful craft request's reported output, not proven
  final inventory commit; smelting/trading are counted separately or not at all.
- `climb_one_cm` and `total_world_time` remain MISSING.
- `walk_on_water`/`walk_under_water` are additive to the primary mode, so a
  swimming player also accrues an underwater distance key (like vanilla).
