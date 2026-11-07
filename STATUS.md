# BedrockInsight

PROJECT_STATUS: IN_PROGRESS

## 2026-11-08 Real-player jump retest — event capture PASS, distinct-jump accuracy NOT YET accepted

Rome6882 joined and left a fresh 1.26.60.6 / LL26.60.0 isolated test BDS
(`.build/isolated-bds/run-b82a0cb06e2d482e8a6204787a3bb5d7`).
The official candidate DLL SHA256 was
`4A98698DF60D21001EA4961D6CF61266E57637DFDCBF7AD57A45187E26D2AA28`.
Closed SQLite integrity check `ok`; one session start/end pair with 9,205ms
duration. Exactly **27** actual player-generated `player_jump_impulse` rows,
amount 1 each, cause `ll26.60.player_jump_event.v1`, belong to Rome6882.
This proves runtime native event -> SQLite persistence, eliminating the prior
0-event regression. It DOES NOT validate 27 completed jumps. Of 26 gaps,
11 are below 100ms (some 43-49ms). The pinned
`C:/Dev/LeviLamina-26.60-Assistant-20261029/src/ll/api/event/player/PlayerJumpEvent.cpp`
publishes from per-input-packet handling when `Jumping` is present and the
server-side grounded / positive-delta checks pass. Thus one physical jump may
lead to multiple event observations. A real-player count-controlled test and
sound de-duplication/transition policy are required before calling
`minecraft:jump` a truthful distinct-jump counter. Do not retroactively rewrite
raw observations. User requested approximately ten jumps, not a precise audited
number. Grouping by time thresholds is only a diagnostic heuristic.
Post-exit read-only evidence: `.build/real-player-jump-evidence-20261108.json`.
Only the owned test BDS PID6160 was gracefully stopped; exit code 0.
Unrelated BDS PID748 and dashboard were not modified. GUI/API live acceptance
for the jump value is still unproven. Overall LK-Stats parity remains IN_PROGRESS.

## 2026-11-07 Jump capture correction (real-player regression)

The 2026-11-07 Rome6882 gameplay run recorded zero `player_jump_impulse`
events despite the operator jumping. The old `Mob::jumpFromGround` direct
getPosDelta pre/post predicate was not effective in that 26.60 run. Its
legacy method and `ll26.60.jump_pos_delta.v1` cause remain historical only.

The current `levilamina/player_jump.cpp` now subscribes to LeviLamina
`PlayerJumpEvent`, matching the upstream LK-Stats v0.13.0 event strategy.
New cause: `ll26.60.player_jump_event.v1`. Native StatsReader personal
CUSTOM + ranking, Python analytics and `/stats` projections accept the
new cause while retaining historical read compatibility. Native/C++
reader and targeted Python fixture tests passed. This is an observed
post-auth-input event, not independent proof of a completed jump or that
later event listeners did not cancel it. Fresh real-player re-test is
REQUIRED before upgrading the `minecraft:jump` acceptance status.


## Analytics Dashboard User Request 2026-11-06

### 2026-11-07 — broken v2 owned offhand/armor capture + strict GUI/API/Web contract (not gameplay accepted)

Extended the existing ItemStackBase::hurtAndBreak hook independently, using
pinned26.60 public LL Actor::getOffhandSlot/getArmor and ArmorSlot0..3, without
new hooks, guessed offsets, AGPL implementation reuse, migrations or deployment.
The old candidate only matched main Inventory stacks; v2 also matches offhand
and four humanoid armor slots, requiring exactly one entry pointer match.

| Area | Candidate implemented | Not proven / pending |
| --- | --- | --- |
| Capture | Entry count1, damageable, max/entry damage and positive input threshold; origin once/true/no reentry; weak samePlayer, mRemoved, sameLevel/dimension before slot reread; same stack1→0 | Actual tool/offhand/armor actions, all break paths, exclusive break causation, final persistence |
| Evidence | v2 cause + location/slot/type/threshold/frame fields; final_break_verified/world_save_verified:false; no XYZ read | Copies, replacement/reload, unknown ownership, invalid Inventory sizes, removed/dimension transition, other paths can be missed |
| Native/Web | Shared strict contract per implementation for personal/type/ranking/page-count + actions; duplicate/unknown/type/empty/future/wrong-location rejection; invalid-only null | GUI/other-Mod/HTTP actual-player acceptance; native all-time only, Web retains JST periods |
| History | v1 rows retained under old main-inventory contract, not upgraded to v2; no data rewrite | Legacy incomplete rows remain stored but no longer accepted by looser native/actions projection |
| Scope | Japanese GUI/C API/Web limitations; C API additive source_contracts/scope metadata, ABI unchanged | Independent C ABI remains NOT LegacyRemoteCall-compatible; shield-damage key stays MISSING/null |

Non-armor used semantics remain unchanged; its post-origin Player reread also
now rejects removed/Level/dimension changes. No per-tick database work.
Private fixture tests cover all four modeled armor slots, offhand/main paths,
bad threshold/frame/location/types, v1 history, null vs zero, ranking/paging,
actual authenticated HTTP and production Japanese JS on a synthetic DOM.
docs/BROKEN_CAPTURE_V2.md, analytics instructions, API docs and matrix updated.

Initial targeted Python run28 had one test error: month date was incorrectly
YYYY-MM-DD; corrected to YYYY-MM, and fixed selected assertion to totals.value.
New targeted4 Python tests then PASS1.355s. No production period behavior changed.
Initial native run-4ed51560d3894549a03b9752dc83ed87 failed compiling the new test:
getRankStats requires page/limit as well as category/output; fixed test call,
not the ABI. Next run-d6a9401f5bce4dddae51757b3320a0e3 passed the preceding16
binaries but new fixture writer.flush failed after an out-of-SQLite-date-range
future timestamp. Changed test future to year2100 and added writer last_error
diagnostic. Failed receipts are retained, not called PASS or BDS failures.

Final guarded verification (synthetic/build only, NOT real gameplay):

- Cached toolchain env: BEDROCKINSIGHT_CLANGCL_ROOT=
  C:/Dev/LL2660Toolchain-20261029/clang+llvm-23.1.4-x86_64-pc-windows-msvc/bin;
  BEDROCKINSIGHT_SQLITE_ROOT=C:/Users/chihirosub/AppData/Local/.xmake/packages/s/sqlite3/3.54.0+100/b897e7007f3d49769c227e337e14fdef;
  BEDROCKINSIGHT_XMAKE_PACKAGES=C:/Dev/LL2660BuildCache/.xmake/packages.
- `./scripts/verify_travel_v2.ps1 -WorldTick -FishBucket -RaidEffect -Absorption -Rest -Resistance -ShieldEvidence -HealthLoss -TypeLabels -BlockRecords -BrokenRecords`:
  **17 C++ binaries +11 native/SQLite/C API/private-inspector/authenticated
  HTTP/Japanese synthetic JS readbacks PASS**, at
  `.build/travel-v2-tests/run-5f14fe31b3cf47db895dc775661c229a/fixture-evidence.json`.
  Modeled main/offhand/armor + v1 fixture: total5, Alice3/Bob1/Carol1;
  native personal/rank/pages and Web stats/actions agree; invalid-only null,
  read-only bytes/integrity, no private IDs. All239 inputs match before/after/current.
- `./scripts/verify_native_stats_api.ps1`: four real GetProcAddress calls from
  independent C11 consumer into owned core-only test DLL PASS, at
  `.build/c-api-tests/run-a9734433d43147b2920cebd90c706008/api-evidence.json`.
  All73 inputs match before/after/current. No LL/BDS mod load or LRC acceptance.
- `./scripts/verify_stats_library.ps1`: **183 Python PASS85.012s**, optimized
  inventory2 PASS0.004s, at
  `.build/stats-view-tests/run-91c4b533f651429990acc8710946f704/regression.json`.
  All90 inputs match before/after/current. Playtime/open-crash/JST/rank/auth/
  no-sample telemetry and prior source-specific tests retained.
- `./scripts/build_server_candidate.ps1`: **26 native units +prelink +DLL
  link +strict PE/package PASS**, at
  `.build/server-candidate/run-8addcc0c4a6a4557b5eaf81544a62dde/candidate-evidence.json`.
  DLL SHA256 `E5E2DC858D4AA5AF0F41386153131E6E1A3D567A79E9A79B5C2C9BAABEBE2313`;
  ZIP SHA256 `61F5488043349DA3458C00BEC56700F6A04E7117B16D50CCD785C153D7D929B6`.
  All125 source/script inputs match current; DLL/ZIP hashes match receipt and
  actual ZIP language entry matches shared asset50BCF694... . Old-client lib
  not used. matched_server_sdk_verified/runtime_dependencies_verified/
  normal_release_evidence/launch_authorized/bds_load_verified all remain false.
  Candidate is NOT staged or loaded into BDS.
- `python scripts/audit_lk_parity.py`:96 mandatory rows/zero accepted, at
  `.build/parity-audit/run-7bb29ac623f34bf28d16734a6dba9e49/inventory.json`.
  CUSTOM CODE_ONLY72/UNIT_ONLY4/MISSING1 unchanged. `git --no-optional-locks diff --check` PASS.
  Separate in-memory SQLite check confirmed date(9000000000000000ms)=NULL
  versus4102444800000ms=2100-01-01. One initial diagnostic command had shell
  quoting SyntaxError; corrected read-only command succeeded, no file writes.

Read-only final protection audit22:26:28JST: bedrock_server_mod748 start
2026-11-05T01:38:04+09:00, viewer python28268 start2026-11-06T06:30:51+09:00,
listener127.0.0.1:18767 unchanged. Protected run-2bbb... DLL SHA256 remains
`B286F054EF1CCF9D90CABBBF3943543FEA2B6FF78805D2EB7F044FF89D6D8A00`.
No BDS stop/restart/commands/launch/stage/load, old SQLite edits, persistent
viewer operations, security/auth/network changes, push or release. MSPT was
already accepted and NOT repeated. PROJECT_STATUS remains IN_PROGRESS.

Real online-BDS actual-player/SQLite/GUI/other-Mod acceptance still requires
the authorized normal-host coordinator path. Its last capability check was
denied by this session's approval=never policy, not by host connectivity or
the DLL; no repeat/bypass/offline-sandbox online-BDS retry was attempted.
Shield producer, compatible LRC, complete source coverage/semantics and all96
real-operation gates are still incomplete, not replaced by fixture success.

### 2026-11-07 — block v2 native frame/position capture + strict mined GUI/API read (not gameplay accepted)

Found a genuine mismatch: native StatsReader accepted mined cause + positive
amount alone, while Web required same-position state-change JSON and amount1.
Incomplete/attempt-like rows could therefore inflate native GUI/C API only.
Added independent bounded block_record.hpp validation shared by native personal,
type, ranking and page-count queries. No source/license copy from AGPL Stats.

| Area | Candidate enabled | Pending / boundary |
| --- | --- | --- |
| Native capture | Existing GameMode hooks, origin once; weak re-resolution, same Player/Level/dimension, mRemoved checked before BlockSource; new v2 causes | Actual 26.60 player operation / cancellation / dimension controls not accepted |
| Position | Copy observed BlockPos XYZ + dimension into events, no guessed world_id | Existing int8 Position.dimension limits v2 to -1..127; do not wrap128+. v1 default0 is NOT proven position |
| Evidence | Exactly1 typed row per transition, before/after/hook/phase/scope + new same_player/same_level; bounded JSON/duplicate/unknown/type rejection | Stored fields are a contract, not cryptographic proof against DB editing or other-Mod exclusive causation |
| History / view | v1 preserved under v1 contract, not upgraded to v2; native GUI/C API/Web expose limited scope/provenance; incomplete-only => null, excluded from rank | Same-position only, adjacent/multi-block placement can be missed; no final-world/whole-category guarantee |
| Read safety | Read-only SQLite function + fixed SQL, integer1/player/type/time/frame checks; future/negative timestamps excluded; hand column/JSON coherent | Native all-time only; JST day/month/all-time remains Web; existing time-budget/disk limits apply |

New native production builder writes ll26.60.gamemode_destroy_block.v2 and
ll26.60.gamemode_build_block.v2. No new hook, engine address, migration, per-tick
DB write or active-server modification. Native minedscope/source_contracts/
final_world_commit_verified:false metadata is additive, not a C ABI change.
Web v1/v2 predicates and source metadata updated, with explicit legacy limits.
Corrected artificial old API fixtures (amount3/emptyJSON) to three amount1
modeled transitions, without weakening source validation. Tests use production
builder/writer/reader and private fixture files only.

Initial new Python readback failed because /analytics defaults to day while
the fixture is historical; explicitly selected all, no production default
change. Initial C++ fixture run-f8ee1a765e254a8f907f4b096399fd49 aborted, and
diagnostic run-f65230422a554123a8d1839fead33c80 identified missing actor_name
in the test-only raw invalid-row INSERT. Fixed the fixture to copy all required
columns from its owned row and changed the new test's checks to log failures
without a CRT assert dialog. Neither failure is a BDS/DLL runtime result.
Targeted6 C++ binaries +2 readbacks then PASS at
.build/travel-v2-tests/run-9963a9bd0dd141a68c035f0dde630bbe/.

New test coverage includes false frame/reentrancy/origin once, coordinates,
legacy v1, wrong amount/actor/secondary/cause/primary, duplicate/malformed JSON,
future rows, invalid-only null/rank omission, stable pages, read-only DB bytes
and integrity, plus actual stats.js synthetic DOM (not browser proof).
Native fixture hash guard now captures Python/JS readback inputs too.
docs/BLOCK_RECORDS_V2.md and ANALYTICS_DASHBOARD/matrix/API docs updated.

Final verification (synthetic/build evidence, NOT gameplay acceptance):

- `./scripts/verify_travel_v2.ps1 -WorldTick -FishBucket -RaidEffect -Absorption -Rest -Resistance -ShieldEvidence -HealthLoss -TypeLabels -BlockRecords`:
  **15 C++ binaries +10 native/SQLite/C API/private inspector/authenticated
  HTTP/Japanese synthetic JS readbacks PASS**, at
  `.build/travel-v2-tests/run-2e55825b16dc43e2a9473eb067d20df0/`.
  v2 and legacy source fixture: mined total4, native Alice2/Bob1/Carol1,
  explicit bad-only player null/rank omission; copiedXYZ12/64/-9 checked.
  All232 captured source/test/readback inputs match before/after/current.
- `./scripts/verify_native_stats_api.ps1`: four actual GetProcAddress C calls
  into owned core-only test DLL, pages/JSON/escaping/DB bytes/integrity PASS at
  `.build/c-api-tests/run-5dc22ce7577344fb8cd81f3e8842aa28/api-evidence.json`.
  All72 inputs match before/after/current. No LL/BDS mod load or LRC registry.
- `./scripts/verify_stats_library.ps1`: **179 Python PASS77.732s**, plus
  optimized inventory2 PASS0.005s, at
  `.build/stats-view-tests/run-0088bf1e08714516bb95317fd5ffe80c/regression.json`.
  All87 inputs match before/after/current. Existing playtime/crash/JST/rank/
  auth/no-sample telemetry and source semantics tests retained.
- `./scripts/build_server_candidate.ps1`: **26 native units +prelink +DLL
  link +strict PE/package PASS** at
  `.build/server-candidate/run-99699d9d039a4071aca2ef4e72a7529a/candidate-evidence.json`.
  DLL SHA256 `B3D3DEEEF8380061089CA3CB0DE9285940CE5EFFEC98263724C2A5CD727CDE80`;
  ZIP SHA256 `CC6920EA932C7BBB7BD4825B3305F52BAA9519DA244C15B855D01157C22255F2`.
  All124 source/script inputs match current; DLL/ZIP match receipt; archived
  shared language asset matches source. No old-client import lib; matched
  fullSDK/runtime deps/normal-release/launch-authorized/BDS-load flags remain
  false. This candidate was NOT staged or loaded into BDS.
- Category matrix now explicitly calls mined CODE_ONLY rather than the old
  misleading COMPLETED_CAPTURE label. CUSTOM72/4/1 remains unchanged.
  `python -O -m unittest discover -s tests -p test_parity_inventory.py -v`:
 2 PASS0.005s after matrix edit. `python scripts/audit_lk_parity.py`:
  `.build/parity-audit/run-b158fa53bc034a968430118cf7f55020/inventory.json`,
  96 mandatory rows / zero accepted. `git --no-optional-locks diff --check` PASS.

Read-only final audit2026-11-07T22:00:41+09:00: bedrock_server_mod PID748
start2026-11-05T01:38:04+09:00 and viewer python28268
start2026-11-06T06:30:51+09:00 still present; listener127.0.0.1:18767.
Protected run-2bbb... DLL SHA256 unchanged:
`B286F054EF1CCF9D90CABBBF3943543FEA2B6FF78805D2EB7F044FF89D6D8A00`.
No BDS stop/restart/commands/launch/stage/load, old SQLite modification,
persistent viewer operation, security/auth/network change, GitHub push/release.
MSPT was already accepted and NOT repeated. PROJECT_STATUS stays IN_PROGRESS.

Host-route capability check: Praxis Sub PC ping returned
"MCP tool call requires approval, but approval policy is never". No host
command/BDS was started. This is this session's tool-approval boundary, NOT
host connectivity failure, DLL failure or renewed MSPT blocker. No alternate
identity/tool route, security change or offline-sandbox online-BDS retry.
Coordinator normal-host fresh-world actual play / GUI / API acceptance remains
required. Shield/LRC/full parity remain IN_PROGRESS, not a fixture-derived PASS.

### 2026-11-07 — shared Japanese type labels / bounded local override (not gameplay acceptance)

Implemented requirement6's language-file/configuration path independently,
without copying upstream AGPL source/translations. Shared138 common Japanese
block/item/entity labels in companion/lang/ja_JP.json, bounded C++/Python
loaders, optional per-process override, raw-ID fallback. Native summary/rank
GUI labels and the four C APIs use the same asset as Web /stats/search.
CUSTOM77 truth/observation-stage labels take precedence over cosmetic overrides.
No canonical ID/value/unit/rank, capture hook, schema or old history changed.

| Area | Candidate implementation | Verification boundary |
| --- | --- | --- |
| Shared asset |138 independently authored common names; package carries lang/ja_JP.json with hash receipt | Not a complete game registry or proof that any type is captured |
| Configuration | schema1/ja_JP;128KiB/4096 merged entries/256-byte labels; strict IDs/UTF-8/duplicate/control rejection | Native invalid file logs and preserves prior valid catalog; companion invalid override rejected before bind |
| Isolation | Native load once; Python immutable per-server snapshot; no HTTP file parameter/hot reload/writes | Fresh processes only, existing BDS/viewer18767 not updated |
| View/API | Cosmetic label + unchanged key; Japanese search; CUSTOM labels cannot be renamed by file | C API != LegacyRemoteCall; actual native GUI/other-Mod gameplay validation still pending |

New core/type_labels.hpp/cpp, companion/type_labels.py and shared asset;
runtime/stats_command/remote_api/stats_view wiring. Build recipes add the
JSON loader and asset; PowerShell packaging rechecks inputs after archive
creation. xmake local modpacker copy definition added but xmake route NOT run.
docs/TYPE_LABELS.md gives setup, exact bounds, token/loopback safety and limits;
ANALYTICS_DASHBOARD and per-key matrix updated without acceptance promotion.

Initial targeted Python run caught a malformed synthetic block fixture
(amount3 instead of three amount1 transitions), then an incorrect test unit
expectation (existing mined unit is 回). Corrected fixtures/assertion, not
production source validation. Final targeted4 Python tests PASS0.985s,
including actual stats.js synthetic DOM, auth401/200, label search, HTML-like
text escaping, server isolation, read-only DB bytes, schema/UTF-8/merge bounds.
Earlier targeted5 C++ binaries +1 readback passed at
.build/travel-v2-tests/run-85aeabc3231f4364b214e61485973f5c/;
this precedes final extra loader-bound tests, not latest-tree final evidence.

Initial full13 C++ binaries +9 readbacks PASS at
.build/travel-v2-tests/run-1f8af47f1c9347698685a102ec87344f/.
Initial candidate run-c26a17a79d8a4879ac5e90de851f2222 compiled26 native
units but DLL link FAILED: newly formatted Logger::warn required an unlinked
fmt::v11::vformat. Not a DLL/build/runtime PASS. Replaced ONLY these two new
format warnings with existing fixed-message pattern; no new fmt dependency.
One concurrent read-only shell creation was OS-denied5; no retry loop or
security/identity change, waited for existing verification to finish.

Found stale jump display after the coordinator's PlayerJumpEvent correction.
Native/Web now say ジャンプ通知（旧加速観測を含む）. Analytics metadata identifies
current event cause + explicit legacy_sources; definition distinguishes
post_auth_input notification/cancellation from historical PosDelta observation.
No producer/filter/quantity change or new jump runtime acceptance. Targeted
jump6 tests PASS2.069s and labels4 PASS1.075s. Header/display updates mean
the preceding fixture run is not final current-tree evidence.

Final corrected verification (all synthetic/build scope, NOT actual-play):

- `./scripts/verify_travel_v2.ps1 -WorldTick -FishBucket -RaidEffect -Absorption -Rest -Resistance -ShieldEvidence -HealthLoss -TypeLabels`:
  **13 C++ binaries +9 native/SQLite/C API/private inspector/authenticated HTTP/
  Japanese synthetic JS readbacks PASS** at
  `.build/travel-v2-tests/run-98e3dc29964e4e158409c1d40ace5930/`.
  Includes labels4096 boundary/merged overflow/invalid UTF-8/nesting/failed-load
  rollback/CUSTOM priority and exact canonical IDs/values. All144 captured
  inputs match before/after/current tree.
- `./scripts/verify_native_stats_api.ps1`: separate C consumer resolves and
  calls all4 exports from owned core-only fixture DLL; JSON/pages/escaping/
  absence/read-only DB/integrity PASS, at
  `.build/c-api-tests/run-4ad7de327a9b460a9a70fa9ad96ef616/api-evidence.json`.
  All70 inputs match before/after/current. NOT a BDS mod load or LRC proof.
- `./scripts/verify_stats_library.ps1`: **175 Python PASS75.627s**, optimized
  inventory2 PASS0.005s, at
  `.build/stats-view-tests/run-c4a249393d9d41c1aa7813c883e62d3e/regression.json`.
  All85 inputs match before/after/current, including shared JSON asset.
  Existing playtime/JST/crash/open-session/rank/auth/escaping/no-sample telemetry
  tests retained, plus label config/HTTP/JS and corrected jump display checks.
- `./scripts/build_server_candidate.ps1`: **26 native units +prelink +DLL
  link +strict PE/package PASS**, at
  `.build/server-candidate/run-d53ad537377a46f0b354ac50fef389f4/candidate-evidence.json`.
  DLL SHA256 `643DF2FDCF132CAEFE539F6B1BD52A32C071A6C07376C17E6D000F6030A84123`;
  ZIP SHA256 `1927A7B3C1CC08E20879ADF081743BAFCD25D4D605EB8C3BD36C05EF2A0D64B9`.
  All123 source/script inputs match current tree; DLL/ZIP match receipt.
  Shared source, packaged file, ZIP entry and receipt label SHA256 all match
  `50BCF69462A65FF99C0C898AE97C20311BF074C191A30491A3CF8F9B419B656C`.
  No old-client import lib; matched fullSDK/runtime deps/normal-release/
  launch-authorized/BDS-load flags remain false. NOT staged/loaded into BDS.
- `python scripts/audit_lk_parity.py`:
  `.build/parity-audit/run-b3d4de1186ee4b08af9a9964ab12ca7a/inventory.json`,
  96 mandatory rows / zero accepted. CODE_ONLY72/UNIT_ONLY4/MISSING1 unchanged.
  `git --no-optional-locks diff --check` PASS.

Read-only final audit at2026-11-07T21:31:59+09:00: BDS **bedrock_server_mod**
PID748 still start2026-11-05T01:38:04+09:00; viewer python28268 still
start2026-11-06T06:30:51+09:00, listener127.0.0.1:18767. An exact-name
bedrock_server query initially omitted the _mod process; PID/wildcard audit
confirmed it, not a server exit. Protected run-2bbb... DLL SHA256 remains
`B286F054EF1CCF9D90CABBBF3943543FEA2B6FF78805D2EB7F044FF89D6D8A00`.
No BDS stop/restart/commands/launch/stage/load or persistent companion
operation, no security/auth/network change, no GitHub push/release.
MSPT already accepted, not repeated. PROJECT_STATUS remains IN_PROGRESS:
shield statistics, compatible RemoteCall and full actual-player/SQLite/GUI/API
acceptance remain unfinished; language fixtures do not promote those gates.

### 2026-11-07 — same-hurt HEALTH delta source, not notification damage or final-world proof

Implemented independent `ll26.60.hurt_health_delta.v1` for CUSTOM damage_taken/
damage_dealt. Reuses existing Player/Mob hurt hooks and bounded writer path in
player_absorption.cpp, with separately copied HEALTH and ABSORPTION attributes.
No extra hurt hook, per-tick DB write, schema migration or protected deployment.
Native StatsReader personal/rank/C API and companion/real stats.js now use the
strict HEALTH source only. Old notification rows stay in SQLite and old action
history, but do NOT contribute to these CUSTOM keys. Legacy-only DB => null.
76 limited projections remain; this is not a full77/9 or actual-play acceptance.

| Area | Candidate implementation | Limits / pending |
| --- | --- | --- |
| Capture | Public SharedAttributes::HEALTH/getAttribute typed float before/after, same weak Actor/Level/dimension/attribute; result DamageProcessed2 + sane optional returned amount | Returned/input amounts are NOT HP loss; only actual positive attribute delta is used |
| Reentrancy | Existing one Player->sameMob parent allowed; recursive/foreign/repeated parent poisons outer observation; origin result/exception preserved | Hook execution and lifetime need real engine controls |
| Quantity | Nearest milli then nearest0.1HP; nonfinite/negative/gain/unchanged/oversized delta rejected; delta<=input+1milli | Net HEALTH change within call, not all applied damage; healing/totem/otherMod can reduce/hide loss; no exclusive causation/world commit |
| Attribution | Victim Player and independently re-resolved direct/child-projectile source Player; unknown/self/environment/thorns/explosion not guessed | Partial attribution/coverage; separate queue rows may be lost/non-atomic |
| View | Distinct cause/attribute/typed details; HP属性減少観測 label and explicit not-final/not-gameplay status; old notifications not summed | New source actual BDS/player/SQLite/in-game GUI/API still unaccepted |

Added core/health_loss_observation.hpp, health_loss_sql.hpp,
companion/health_loss_source.py and native/reader/catalog/JS wiring.
Shared validation/fixed-point helpers are existing independent BedrockInsight
code, not AGPL reference code. docs/HEALTH_LOSS_CAPTURE.md covers controls,
data definitions, old history, trust/rounding and normal-host validation.
Sites skill used LOCAL-ONLY for existing UI/JS checks; no registration/source
sync/hosting/publication, no change to the persistent viewer process.

Pinned-header adapter compile PASS:
`.build/health-probe/run-945be9b2eb0c42549a6127683ef5ceb2/`.
One new compiler-emitted MCAPI name, `?HEALTH@SharedAttributes@@SAAEBVAttribute@@XZ`,
was reviewed against public SharedAttributes.h (HEALTH MCAPI). Exact PE
allow-list adds only this name; ceiling52->53. Parser tests13 valid/41 rejected
PASS, including incorrect HEALTH return signature rejection. No guessed offset.
Target Python4 tests PASS2.523s; actual JS syntax check PASS.

Initial full171-Python run failed1 obsolete test expecting pre-notification
values in CUSTOM, `.build/stats-view-tests/run-390ed05447c1417d883d36b12c86fc8d/`.
Not counted as PASS. Updated that assertion to require null and HEALTH-stage
metadata while retaining the old action-history pre-notification checks.
Changed-contract3 tests then PASS0.665s.

Final native fixture command:
`./scripts/verify_travel_v2.ps1 -WorldTick -FishBucket -RaidEffect -Absorption -Rest -Resistance -ShieldEvidence -HealthLoss`
**12 C++ binaries +9 SQLite/native C API/private inspector/authenticated HTTP/
Japanese synthetic JS readbacks PASS** at
`.build/travel-v2-tests/run-58513a9f8a73425893afa41b60a81ed6/`.
HEALTH fixture gives Alice50/Bob30/total80 independently for taken/dealt,
checks page metadata and rejects old999 notification amounts, absorption
rows, malformed/duplicate JSON, wrong amounts/types. New source stays distinct.
This is NOT actual engine/player/browser/GUI proof. All140 captured native
source/test/script hashes match before/after/current.

Final build/regression evidence:

- `./scripts/build_server_candidate.ps1`: **26 native units +prelink +DLL
  link +strict PE/package PASS**, at
  `.build/server-candidate/run-1d2ccb568b044a5dbd922d27b4d3e9c7/`.
  DLL SHA256 `FF21329CA540F83914323C0A412A3E58E1F6996AE6067CE29126F58440B80152`;
  ZIP SHA256 `281655D3D5A336A99523AF4D69DA1D08D05D999FFD6CB091710F55FE23022ADA`.
  All119 source/script inputs match current tree; DLL and ZIP match receipts.
  old_client_import_library_used=false. Matched full SDK/runtime deps/normal
  release/launch authorized/BDS load remain false. NOT staged into a BDS or loaded.
- `./scripts/verify_stats_library.ps1`: **171 Python PASS74.978s**, plus
  optimized inventory2 PASS0.004s at
  `.build/stats-view-tests/run-11ecb73a03b546fa94a3fe6adde3fd00/`.
  All81 source/test/script inputs match before/after/current. Includes prior
  playtime/crash/JST/rank/auth/escaping/telemetry coverage and new HP cases.
- Final `python scripts/audit_lk_parity.py`:
  `.build/parity-audit/run-214ee3fdf3894eb1a7f6839cc8dccdd7/inventory.json`.
  96 rows zero accepted; declarations remain CODE_ONLY72/UNIT_ONLY4/MISSING1.
  No compile/fixture evidence promoted actual-player/GUI/API acceptance.

Read-only end audit: PID748 still has start2026-11-05T01:38:04+09:00;
viewer28268 still start2026-11-06T06:30:51+09:00, .NET TCP listener127.0.0.1:18767.
Protected run-2bbb... DLL SHA256 remains
`B286F054EF1CCF9D90CABBBF3943543FEA2B6FF78805D2EB7F044FF89D6D8A00`.
Previously observed externally started PID34584 is no longer present; a name
audit currently finds only BDS748. This turn sent NO stop/terminate/restart or
server command and did not stage/load a BDS. Its exit time/cause is unknown;
do not infer candidate runtime failure or silently restart it.

This turn did not stop/restart/stage/load/patch any existing BDS/18767, no security/auth/network change,
no GitHub push/release. MSPT is already accepted and was not repeated.
PROJECT_STATUS remains IN_PROGRESS; all96 actual-play acceptance rows remain
unaccepted, including damage keys. Shield/RemoteCall/full gameplay gaps remain.

### 2026-11-07 — native C API consumer / coherent ranking pages (NOT RemoteCall compatibility)

Implemented a C11/C++20 public header, `core/stats_api.h`, with explicit calling
convention and function-pointer types for the existing four independent APIs.
The old C++ `remote_api.hpp` now reuses those declarations. No allocation,
C++ object, SQLite handle, HTTP authentication token or writer crosses the ABI.
Added `StatsReader::ranking_page`: positive ranking rows, total_entries and
has_more come from ONE SQLite statement/read snapshot, including empty and
beyond-last pages. Existing ranking callers delegate to this read-only path.
Both rank APIs now include total_entries/has_more/collection_complete:false.
No source event semantics, migrations or existing databases were changed.

| Area | Enabled in candidate source / test result | Pending |
| --- | --- | --- |
| Public ABI | C-compatible declarations; same4 names/signatures/negative returns; explicit __cdecl | Provider lifetime and real other-Mod integration on normal-host isolated BDS |
| Pagination | Deterministic positive-only rows, coherent count, empty/out-of-range pages | Requests are not a pinned cross-request snapshot; writer may update between pages |
| Actual dynamic boundary | Separate C EXE -> GetProcAddress -> core-only test DLL -> read-only SQLite;4 functions called | NOT an LL/BDS mod load or LegacyRemoteCall registry/export |
| Honesty | NoData totalnull, shield unsupported/error, collection_completefalse, raw IDs absent | Full9/77/GUI/API gameplay acceptance remains incomplete |
| Compatibility audit | Pinned local upstream contract inspected factually; namespace/UUID/enum/map/1-based pages differ | Official LegacyRemoteCall0.23 SDK absent in checked caches; pinned Web inputs cache-miss, license/ABI still unverified |

New test-only fixture DLL setup and C consumer are excluded from the actual
BDS mod source list. The test creates only an unused project-local DB and
refuses an existing file. C verifies every DB byte unchanged after API reads;
independent Python parses the actual emitted JSON and checks closed DB
integrityok. No upstream AGPL implementation/comments/translations copied.
`docs/NATIVE_STATS_API.md` documents usage, bounds, units/nulls, trust/lifecycle,
native vs HTTP auth, current incompatibilities and remaining real-player gate.

Final verification (synthetic/API-boundary/build evidence, NOT gameplay):

- `./scripts/verify_native_stats_api.ps1` PASS:
  `.build/c-api-tests/run-65ca73b9bcd0456ba28b0994ae0f0a7b/`.
  Actual C11 dynamic4 calls, buffer/error bounds, valid escaped JSON, page
  totals/last/out-of-range, absence of fabricated zeros, full-file read-only
  check and independent integrityok. All64 captured input hashes match
  before/after/current. Core fixture only; C caller does not statically link
  the provider, C++ or SQLite. Test DLL SHA256
  `EAB90F87F377B40309E7053A3239DE6157115706017783E17EB67274B31928D3`.
- `./scripts/verify_travel_v2.ps1 -WorldTick -FishBucket -RaidEffect -Absorption -Rest -Resistance -ShieldEvidence`
  **11 C++ binaries +8 DB/HTTP/private-inspection/Japanese synthetic JS
  readbacks PASS**, `.build/travel-v2-tests/run-985fef718e704397a78210ab18436fc5/`.
  Includes new native reader and four-API pagination assertions. No gameplay,
  browser or BDS acceptance inferred. All137 captured input hashes match
  before/after/current.
- `./scripts/build_server_candidate.ps1`: **26 native units +prelink +DLL
  link +strict PE/package PASS** at
  `.build/server-candidate/run-a92aea3c79f5407eb1fe8fcdb14fc296/`.
  DLL SHA256 `B0DA643AC10AD54A15241C813992911392EBFD5EDF2794015BA7F19FD4416E89`;
  ZIP SHA256 `9E99DBDD2E26A0ED2998254B68B7D9287E1BBC6E158C36E7ECAACC49881356E7`.
  All117 source/script hashes match current tree. Static PE export inspection
  confirms all4 public names in the actual candidate. Old client import=false;
  matched full SDK/runtime deps/normal release/launch authorized/BDS load=false.
  Candidate NOT staged/loaded. Cached clang/SQLite/package env as documented
  in preceding build entries; no network SDK installation.
- Optimized `python -O -m unittest discover -s tests -p test_parity_inventory.py -v`:
  2 PASS; `python scripts/audit_lk_parity.py` keeps96 rows unaccepted and the
  same CODE_ONLY72/UNIT_ONLY4/MISSING1 declarations. Inventory is not an
  implementation audit or runtime acceptance. No repeat of the unchanged
  prior167-Python baseline was needed; this turn ran8 readbacks plus2 inventory
  tests, native11 and the new C dynamic boundary.
  Final inventory after matrix corrections:
  `.build/parity-audit/run-8920661604b141f1a40c363fd52197a3/inventory.json`.
  Final optimized2 tests PASS0.006s; final candidate117 inputs, DLL and ZIP
  still match their receipt. No metric/API/GUI acceptance was promoted.

During the candidate build, an extra read-only shell spawn was denied with
known CreateProcessAsUserW5. No alternate shell/identity/security bypass or
fast retry; the original build session completed normally. Post-build
read-only audit found BDS748 (same11-05 start), BDS34584 (same11-07 16:55:05
start) and viewer28268 (same11-06 06:30:51 start), and .NET TCP listener
127.0.0.1:18767. The externally started34584 is protected, NOT proof of this
candidate. Protected run-2bbb... DLL remains SHA256
`B286F054EF1CCF9D90CABBBF3943543FEA2B6FF78805D2EB7F044FF89D6D8A00`.

No existing BDS/viewer18767 was stopped, restarted, staged, loaded or patched.
MSPT acceptance remains done; no MSPT/offline-BDS retry. PROJECT_STATUS remains
IN_PROGRESS; four C exports do NOT satisfy LegacyRemoteCall compatibility or
actual other-Mod acceptance merely because the dynamic core fixture passed.

### 2026-11-07 — opt-in raw shield evidence, NEVER a successful shield statistic

Shield mitigation remains MISSING/unmeasured; no prediction/no-hurt shortcut.
Implemented a narrow prerequisite to genuine acceptance: typed Player::$_hurt
before/after evidence, disabled by default. Only exact process-local
BEDROCKINSIGHT_SHIELD_EVIDENCE=1 registers the evidence hook; unset/0/true/01/
trailing-space do not. Normal startup gets a dormant token with no new hook.
No BDS or persistent viewer was started; no process environment/security or
protected world/DB/DLL was changed here.

| Area | Candidate implementation | Still pending / limits |
| --- | --- | --- |
| Raw observations | Entry public isDamageBlocked/isInvulnerableTo; before/after public blocking/statusflag74, typed shield timestamps/previous flags, invulnerable/dead state; copied input/result type/optional amount | Names and raw changes are NOT validated shield-success semantics or measured blocked quantity |
| Safety | Origin exactly once/result/exception preserved, weak re-resolution after origin, same Player/Level/dimension; no old this/source dereference after origin | Real engine dispatch, lifetime and all control cases require normal-host tests |
| Bound | Max256 identities and>=5s between samples/player (except initial/clock regression), old admission slots pruned only when full; admission reserves before origin | Samples can miss cases; queue/telemetry failures lose rows; not a complete damage log |
| Persistence | Append-only EventKind::PlayerShieldEvidence text player_shield_evidence, cause ll26.60.player_hurt_shield_evidence.v1, amount0, existing schema4/bounded writer | NOT in daily type/CUSTOM/ranking projections; no migration or old-data rewrite |
| Private inspection | companion.shield_evidence CLI, read-only bounded subset, duplicate/type/shape/false-success checks, opaque identity, no HTTP evidence endpoint | CLI exit0/schema-valid/sample-present are NOT actual-play/engine/metric acceptance |
| Web / API | Existing Japanese shield stats must remain unsupported/null even with raw evidence; other76 limited projections preserved | Shield metric and all96 full-parity actual player/SQLite/GUI/API gates remain unaccepted |

New core/shield_evidence.hpp, native player_shield_evidence.cpp/.hpp, lifecycle/
build registration, read-only companion/shield_evidence.py, native and Python/
actual stats.js synthetic tests. No AGPL source/comments/translations copied.
Docs/SHIELD_CAPTURE_FINDINGS.md contains opt-in, raw field definitions and
normal-host cases separated by>=6s; docs/ANALYTICS_DASHBOARD.md links it.

Single-adapter pinned-header compile PASS at
`.build/shield-probe/run-f486c77dcf644f32b1f8b0814c594b83/` (before the final
opt-in helper refactor; final full candidate below is authoritative).
No new BDS MCAPI allow-list entry is needed: Player::$_hurt already reviewed,
getStatusFlag is a public LLNDAPI. No guessed offset/address or private thunk.
Targeted Python5 PASS1.243s: raw rows cannot become shield stats/zero/rank,
strict JSON/types/duplicates, future/foreign exclusion, bounded subset,
private CLI/read-only/empty is not PASS, authenticated HTTP/actual stats.js
stays unavailable. Initial DOM fixture incorrectly expected zero children
instead of the existing unavailable-placeholder row; test expectation fixed,
production UI unchanged. No failed run was counted as acceptance.

Final verification:

- **11 native C++ binaries +8 SQLite/private-inspection/authenticated HTTP/
  Japanese JS readbacks PASS**:
  `.build/travel-v2-tests/run-0cc6c961b94d4b12bdeea082d42f3b7f/`.
  New4-row production builder→StorageWriter→closed SQLite fixture preserves
  hypothetical raw states, amounts0, integrityok. No daily-type aggregate rows,
  native CUSTOM empty, native shield ranking unsupported; private CLI sees4
  raw rows while anon401/auth200 Japanese stats.js keeps shield unsupported/
  null. Synthetic data, NOT actual shield/engine/player/browser proof.
 136 source/test/script hashes match before/after/current.
- **26 native units +prelink +DLL link +strict PE/package PASS**:
  `.build/server-candidate/run-e3d0fa4739134cf9b65917d34b290ce8/`.
  DLL SHA256 `50D4E4EDD57095DCFF40EE1767DD9B66D7404DAD988338734668BBA08F3F1105`;
  ZIP SHA256 `4424A8C4EAC8E8E2470137ADC8AC447B9F8618DD029ABEC06EDFB064C1071CA0`.
 116 source/script hashes match current tree; DLL/ZIP match receipt. No old
  client import library used. Matched full SDK/runtime dependencies/normal
  release/BDS load/launch authorization remain FALSE. Not staged/loaded.
- **167 Python PASS77.455s +optimized inventory2 PASS0.004s**:
  `.build/stats-view-tests/run-c9a2fcc279cb44bfb2296e2833cc06b8/`.
 78 inputs match before/after/current. Real stats.js ran in a synthetic DOM
  on authenticated HTTP JSON; no actual browser/gameplay acceptance.
- Strict import parser regression **12 valid /40 rejected PASS**, and current
  matrix optimized2 PASS0.005s. Per-key classifications stay72 CODE_ONLY,
 4 UNIT_ONLY,1 MISSING (77); all96 actual-play/SQLite/GUI/API gates unaccepted.
  Final post-document inventory:
  `.build/parity-audit/run-85cdecc47bce429ab75be5cb1af63c73/inventory.json`,
  current matrix optimized2 PASS0.004s.76 limited read projections unchanged;
  no shield metric projection. Raw diagnostic code is not a completed metric.

Exact commands (same cached clang-cl/SQLite/package env variables as the
preceding verified entries, no downloads or BDS start):

```powershell
$env:PYTHONPATH='tests;.'
$env:PYTHONUTF8='1'
python -W error::ResourceWarning -m unittest discover -s tests -p test_shield_evidence.py -v
./scripts/verify_travel_v2.ps1 -WorldTick -FishBucket -RaidEffect -Absorption -Rest -Resistance -ShieldEvidence
./scripts/build_server_candidate.ps1
./scripts/verify_stats_library.ps1
./scripts/verify_interval_runtime_import.ps1
python -O -m unittest discover -s tests -p test_parity_inventory.py -v
python scripts/audit_lk_parity.py
```

Final read-only protection audit: PID748 bedrock_server_mod still started
2026-11-05T01:38:04.0009206+09:00; viewer PID28268 python still started
2026-11-06T06:30:51.2176499+09:00.127.0.0.1:18767 remains listening;
owner PID not newly CIM-attested. Protected run-2bbb... DLL SHA256 stays
`B286F054EF1CCF9D90CABBBF3943543FEA2B6FF78805D2EB7F044FF89D6D8A00`.
An additional BDS PID34584 started2026-11-07T16:55:05.6596720+09:00 was
observed. We did not launch/stop/restart/modify it or inspect its world/DB;
it is also protected. Its existence is NOT candidate load/runtime evidence.

PROJECT_STATUS remains IN_PROGRESS. Normal-host actual-play collaboration was
requested without blocking local implementation. No MSPT rerun, no online BDS
retry in the offline sandbox, no GitHub push/release or unrelated project work.

### 2026-11-07 — correlated resistance-stage formulas, NOT measured applied mitigation

Implemented damage_resisted and damage_dealt_resisted as independently derived
quantities on an observed/correlated public26.60 post-enchantment stage. Cause
ll26.60.processed_hurt_resistance_formula.v1. No AGPL code/comments/translations
copied; referenced pinned upstream behavior factually, not as local proof.

| Area | Implemented / verified so far | Pending / limits |
| --- | --- | --- |
| Native hooks | Public Player/Mob::$_hurt + const Mob::$getDamageAfterEnchantReduction, WeakEntityRef, getEffect and source virtual API;25-unit candidate DLL/link/strictPE PASS below | No real BDS execution |
| Correlation | One stage inside same victim/source hurt; one Player→Mob parent, no duplicate event; repeated/foreign/recursive stage or hurt-in-stage poisons frame | Actual26.60 dispatch/call hierarchy, compatibility with other Mods |
| Admission | Positive finite/bounded inputs and basis, resistance-eligible source, effectID11, stable amplifier/duration and Player/Level/dimension; outer DamageProcessed + finite optional return | NotHurt/EffectsOnly, duration<=0/unbounded effect and changing effect omitted; full-resistance returns may be missing |
| Quantity | Nearestmilli basis × capped level/5, then nearest0.1HP; no HP/absorption/returned-value substitution | A reference FORMULA, NOT engine-applied resistance or final HP loss; flags explicitly false |
| Attribution | Player victim; separately resolved same-Level direct/child-projectile owner Player, re-resolved after origin | Self/environment/thorns/explosion/unknown excluded from dealt; pair of writer submissions not atomic |
| View / read | Strict native SQL + Python quantity/source/type/JSON validation, per-player/period/rank and Japanese computed-value status, 1/10HP, null if absent | Common GUI/API reader is wired, but actual in-game GUI/compatAPI/browser proof still missing |
| Safety | Origin once/result/exception preserved, copied primitives to bounded writer, no game-thread DB I/O or protected deployment | Preserve accepted MSPT, travel/rest/absorption/jump/history and live18767 |

New core/resistance_observation.hpp, resistance_sql.hpp, native
player_resistance.cpp/.hpp, companion/resistance_source.py, lifecycle/build
registration and dedicated synthetic native/Python/HTTP/JS tests.
Docs: RESISTANCE_CAPTURE.md and SHIELD_CAPTURE_FINDINGS.md. Sites skill was used
in LOCAL-ONLY existing-checkout mode; no Site registration/source sync/hosting/
publication or extra persistent viewer. Root dashboard and authentication stay.

Single-adapter pinned-header probe PASS at
`.build/resistance-probe/run-04925ee8ab494665a51f841e93fee1bc/` (before the final
hurt-in-stage reentry guard; final complete candidate below is authoritative).
Exact const compiler import ?$getDamageAfterEnchantReduction@Mob@@QEBAMAEBVActorDamageSource@@M@Z
is individually allow-listed. Runtime delay shape ceiling51→52; exact
module/name/ordinal checks retained. No guessed offsets/private thunk calls.
Targeted Python5 PASS3.107s: roles, stages, levelsI..V/255 cap, rounding/bounds,
malformed/duplicate/typed/tampered rows, nativeSQL/Python equivalence,
JST/player/rank/pages/read-only401/200 and actual stats.js in synthetic DOM.
10 native C++ fixtures +7 native writer→SQLite→HTTP/JS paths PASS so far in
`.build/travel-v2-tests/run-1fdd9a6d7599411e8e3b436201940ca5/`.
Synthetic resistance total50 per key, Alice30/Bob20, invalid rows excluded,
closed DB integrityok. None of these is player/engine/browser acceptance.

Shield remains mandatory MISSING/unmeasured. Header inventory found public
predictive virtual isDamageBlocked, typed shield timestamps/previous flags,
ActorFlags::BlockedUsingShield74 and LevelSoundEvent::ShieldBlock255, but no
public _blockUsingShield declaration or independently verified successful
blocked-amount publisher. Names/flags/sounds and upstream prediction+no-hurt
heuristic are not silently promoted to actual shield mitigation.
Current76 limited read projections do NOT mean76 fully captured/accepted
metrics or all-feature parity.
All96 actual-play/SQLite/GUI/API acceptance entries remain unaccepted;
PROJECT_STATUS=IN_PROGRESS. Existing BDS/oldDB/18767/security/auth/network,
other projects and GitHub are untouched. No MSPT rerun or sandbox online BDS.

Final verification:

- **10 native C++ binaries +7 native DB→authenticated HTTP→Japanese JS DOM
  readbacks PASS**, root run-1fdd9a6d7599411e8e3b436201940ca5 above.132
  source/test/script hashes match before/after/current. New14-row resistance
  DB contains6 valid and8 excluded records; personal30, ranks30/20, total50
  per key, integrityok. Stage-gate duplicate/foreign/nested exclusion and
  origin/exception preservation are SYNTHETIC models, not engine proof.
- **25 native units +prelink +DLL link +strict PE/package PASS**:
  `.build/server-candidate/run-9bb957e117ed4ae9941f9a4f98cf9eb7/`.
  DLL SHA256 `669B49A4A17D7211800955CF2581C0531BC0957DCADD522036D1756714102730`;
  ZIP SHA256 `D3BBDB5D95F1EF1E89715D7A8574898B299DEF165290627ADD5466E98C594355`.
 113 source/script hashes match current tree; DLL/ZIP match receipt. Old
  client import library used=false. Matched full SDK/runtime dependencies/
  normal release/BDS load/launch authorization remain FALSE; NOT staged/loaded.
- **162 Python PASS76.292s +optimized inventory2 PASS0.006s**:
  `.build/stats-view-tests/run-fdb06598af07424ba317b9f3e4a8bef3/`.
 75 before/after/current source hashes match. Includes real stats.js executing
  on authenticated HTTP JSON in a synthetic DOM; not an actual browser.
- Strict runtime import parser regression **12 valid /40 rejected PASS**,
  `./scripts/verify_interval_runtime_import.ps1`; no load/runtime proof.
- Final matrix inventory
  `.build/parity-audit/run-6f0cf729b93b4e4980257d2f2d1adff4/inventory.json`:
 96 entries, zero accepted; CUSTOM77 declarations CODE_ONLY72/UNIT_ONLY4/
  MISSING1. Matrix input hashes match;76 limited Python read projections and
  no shield projection. Post-document optimized inventory2 PASS0.005s.
  UNIT_ONLY/limited definitions and every actual-play gate still require work;
  these counts do not assert76 complete native metrics.

Exact commands used, with the same three cached toolchain/SQLite/package env
variables documented in the preceding rest entry:

```powershell
$env:PYTHONPATH='tests;.'
python -W error::ResourceWarning -m unittest discover -s tests -p test_resistance.py -v
./scripts/verify_travel_v2.ps1 -WorldTick -FishBucket -RaidEffect -Absorption -Rest -Resistance
./scripts/build_server_candidate.ps1
./scripts/verify_stats_library.ps1
./scripts/verify_interval_runtime_import.ps1
python -O -m unittest discover -s tests -p test_parity_inventory.py -v
python scripts/audit_lk_parity.py
```

Protection recheck: visible BDS PID748 bedrock_server_mod started
2026-11-05T01:38:04.0009206+09:00 and viewer PID28268 python started
2026-11-06T06:30:51.2176499+09:00, unchanged from the preceding checkpoint.
Same-token .NET enumeration confirms127.0.0.1:18767 is listening (owner PID
not newly CIM-attested). Protected run-2bbb... DLL SHA256 still
`B286F054EF1CCF9D90CABBBF3943543FEA2B6FF78805D2EB7F044FF89D6D8A00`.
No process restart, protected file write, permanent preview, security change,
remote push or release. Future online player tests require the authorized
normal-host coordinator, not the offline execution sandbox; no such test was
attempted here. All96 mandatory actual-player/SQLite/GUI/API gates remain open.

### 2026-11-07 — time_since_rest: completed Player tick snapshots after observed sleep

Implemented an independent public26.60 ServerPlayer::$normalTick producer,
core/rest_counter.hpp and strict rest_sql.hpp / companion/rest_source.py.
Cause ll26.60.player_rest_ticks.v1. It counts completed PLAYER tick calls
after actual post-origin isSleeping=true, never Level callbacks, elapsed
wall time or a presumed50ms/tick. No upstream AGPL code/comments/translations.

| Area | Enabled in candidate source | Pending / limits |
| --- | --- | --- |
| Baseline / reset | Unknown until sleep observed; sleeping resets to known0; awake completed calls increment | Not full-night/phantom-reset proof or actual engine dispatch-frequency validation |
| Continuity | Same weak Player/Level, join/disconnect memory reset, >30s gap/clock regression/nesting reset, overflow invalidation | Past saved rows are history, not a current/online value; no restart/offline extrapolation |
| Safety | Origin once/exception preserved, post-origin weak re-resolution, bounded256 records, cross-thread fail-closed, >=5s queue snapshots | No per-tick DB I/O; queue drops/final <5s tail not filled; all actual gameplay pending |
| Readers | Native StatsReader personal/rank (shared GUI/API path), strict JSON/types/duplicates/time; Japanese Web selected-player/JST period/paging/rank | Latest per player, NOT SUM/MAX history. Latest unknown suppresses older known. Known0 displayed, zero/unknown not ranked |
| Existing four categories | Playtime, activity/ranking and genuinely accepted TPS/MSPT retained | Current source is not deployed into18767 or protected BDS |

Period filters select snapshot timestamps, not a clipped duration since the
period boundary. All-player total sums latest known per-player values only.
Unknown marker amount0 is never a measured zero; no record/unknown returns null.
Web explicitly says last SAVED observation, not current value. Exact definition,
exclusions and normal-host player acceptance checklist: docs/REST_CAPTURE.md.

Verification below is NOT real BDS/player acceptance. Do not infer that from
compile, synthetic DB/HTTP/DOM, or enum registration. Single-adapter
probe compiled successfully against pinned headers at
`.build/rest-probe/run-f39f20b046df48388e93193072bfb899/`.
Its exact compiler import ?$normalTick@ServerPlayer@@QEAAXXZ is individually
allow-listed; bounded runtime PE shape50→51, module/ordinal checks retained.
Targeted Python5 PASS1.690s. Initial tests had two incorrect expectations
(lexicographic tied player IDs; snapshot cadence at exactly5s); corrected
test expectations without changing counter/reader behavior. Initial fixture
run6035516cc5a0425fbc1db0badc852aca FAILED and is not acceptance evidence.

Final verification:

- **9 native C++ binaries +6 native DB→authenticated HTTP→Japanese JS DOM paths
  PASS**: `.build/travel-v2-tests/run-3850f5b182cd46399ac2aea5e73569ce/`.
  Rest production builder→StorageWriter→closed10-row SQLite→native personal/
  ranking→integrity=ok: Alice latest known0, Bob200, Charlie latest unknown
  (not his older300); malformed/mismatched/duplicate rows excluded. API401/200,
  Web total200, rankingBob200 only, known0 and null shown separately. Synthetic
  only, NOT player/engine/browser evidence.127 source/test/script hashes match
  before/after/current tree.
- **24 native units +prelink +DLL link +strict PE/package PASS**:
  `.build/server-candidate/run-a280aac3d1a74788b70d09f18fb7938d/`.
  DLL SHA256 `EFF10803D8171B7477506690080DFCCD86030E7B1C0447C1F82D53323000DF75`;
  ZIP SHA256 `F5C5F24A4E7BF08F115AD6D175503D82EC61F5A9EA2BCE35F7F0E38AE2245A5A`.
 109 source/script hashes match current tree; DLL/ZIP match receipt. Matched
  full SDK/runtime dependencies/normal release/BDS load/launch authorization
  remain FALSE. Candidate was NOT staged/loaded. No old-client library used.
- **157 Python PASS71.315s +optimized inventory2 PASS**:
  `.build/stats-view-tests/run-96be8f2275b64602be016c8a93a6ac98/`.
 72 inputs before/after/current all match. This includes the actual stats.js
  executing on authenticated HTTP JSON in a synthetic DOM, not a browser.
- Strict runtime import parser regression **12 valid /40 rejected PASS**:
  `./scripts/verify_interval_runtime_import.ps1`. Exact names/module/ordinal
  remain enforced; no blanket API allow or load proof.
- Final inventory `.build/parity-audit/run-0cb02e5c7ad1419bbc34956965ab11af/inventory.json`:
 96 mandatory rows, zero accepted.77 CUSTOM declarations: CODE_ONLY70,
  UNIT_ONLY4, MISSING3, BLOCKED0. Declarations are not independently attested
  native/gameplay/SQLite/GUI/API acceptance.
  Matrix evidence-table follow-up also passed optimized inventory2 tests0.005s.

Exact commands (same cached environment as the preceding verified increments):

```powershell
$env:BEDROCKINSIGHT_CLANGCL_ROOT='C:/Dev/LL2660Toolchain-20261029/clang+llvm-23.1.4-x86_64-pc-windows-msvc/bin'
$env:BEDROCKINSIGHT_SQLITE_ROOT='C:/Users/chihirosub/AppData/Local/.xmake/packages/s/sqlite3/3.54.0+100/b897e7007f3d49769c227e337e14fdef'
$env:BEDROCKINSIGHT_XMAKE_PACKAGES='C:/Dev/LL2660BuildCache/.xmake/packages'
$env:PYTHONPATH='tests;.'
python -W error::ResourceWarning -m unittest discover -s tests -p test_rest.py -v
./scripts/verify_travel_v2.ps1 -WorldTick -FishBucket -RaidEffect -Absorption -Rest
./scripts/build_server_candidate.ps1
./scripts/verify_stats_library.ps1
python scripts/audit_lk_parity.py
./scripts/verify_interval_runtime_import.ps1
```

Final read-only protection audit: PID748 bedrock_server_mod still started
2026-11-05T01:38:04.0009206+09:00; PID28268 python still started
2026-11-06T06:30:51.2176499+09:00.127.0.0.1:18767 is listening. Protected
run-2bbb... DLL SHA256 remains
`B286F054EF1CCF9D90CABBBF3943543FEA2B6FF78805D2EB7F044FF89D6D8A00`.
Get-NetTCPConnection/CIM access was denied; same-token .NET listener enumeration
and explicit process checks succeeded, but listener owner PID was not newly
attested. An additional shell read during the active build hit the known
CreateProcessAsUserW denied5; no security/identity changes or fast retries.
No real-host test attempted: the previously documented coordinator tool path
requires unavailable approval in this session. Normal-host fresh-world/player
validation remains required; do NOT substitute offline-sandbox BDS retries.

Remaining mandatory unprojected CUSTOM keys are damage_blocked_by_shield,
damage_dealt_resisted and damage_resisted. Existing shield prediction/no-hurt
and calculated resistance-stage formulas are not silently upgraded to measured
successful mitigation. Current limited Web projections74/77 != full parity;
all96 actual-play/SQLite/GUI/API acceptance rows remain unaccepted.
PROJECT_STATUS stays IN_PROGRESS. No MSPT repeat, no BDS launch/staging, no
18767 restart, no security/auth/network changes or push/release.

### 2026-11-07 — post-hurt ABSORPTION attribute loss, not notified damage or final HP loss

Implemented two more mandatory CUSTOM sources independently: damage_absorbed
and damage_dealt_absorbed. The pinned public Player/Mob::$_hurt return and
SharedAttributes::ABSORPTION / Actor::getAttribute API are used to compare the
same victim/attribute **before vs after**, not upstream's pre-origin
min(notified_damage,absorption) estimate. No AGPL code/comments/translations
copied. Old pre-origin damage_taken/dealt events and semantics remain intact.

| Area | Implemented / verified | Pending / limits |
| --- | --- | --- |
| Victim source | Same weak unremoved Actor, Level/dimension, attribute address/definition, positive finite input, DamageProcessed type, optional finite nonnegative returned amount, bounded actual attribute decrease | Real engine/player execution; fully absorbed/cancelled/lethal return behavior |
| No double count | Root Player→same Mob parent once allowed without a second event; same-hook recursion, second parent, different target/chain mark root ambiguous | Actual26.60 call hierarchy and other-Mod interaction |
| Quantities | Before/after nearest0.001HP, delta nearest0.1HP; decrease <=input (+1milli rounding tolerance), finite/bounded, zero-unit result omitted | Not final HP loss, theoretical resistance/shield or exact sub-unit fractions |
| Credited player | Victim player; separately, resolved same-Level direct or child-projectile source Player, re-resolved after origin | Self/environment/thorns/explosion/unknown omitted; two queue rows are not atomic |
| Storage / UI | Existing kind/schema4/bounded writer, strict native SQL + Python, native personal/rank/common GUI/API reader, JST/player/paging/authenticated Japanese Web | Real player→SQLite→GUI/API/browser and18767 deployment not accepted |
| Four dashboard categories | Working playtime/activity/rank and accepted TPS/MSPT retained | Full9/77 and per-key actual-play acceptance still required |

New `core/absorption_observation.hpp`, `absorption_sql.hpp`, native
`player_absorption.cpp/.hpp`, lifecycle/build registration, read projections and
Japanese labels. Cause `ll26.60.hurt_absorption_delta.v1`. All serialized rows
say final_health_loss_measured=false, exclusive_causation_verified=false and
durable_world_commit_verified=false. Attribute address/definition equality is
an observation, not a generation/world/cross-Mod causal guarantee. Origin and
its result/exception are preserved despite telemetry failure. No old Actor,
source or attribute pointer is dereferenced after origin. No game-thread DB I/O.

Verification so far:

- Actual single-adapter pinned26.60 compile PASS:
  `.build/absorption-probe/run-3dde75715f9b441a84c5dc2d19a0160c/`.
  Exact compiler-emitted Player::$_hurt, Mob::$_hurt, ABSORPTION and getAttribute
  names were added to the reviewed import allow-list. Reviewed prior PE has46
  runtime entries; these4 bring the bounded shape ceiling from48 to50. The
  exact per-symbol/module/ordinal checks remain. No general import bypass.
- Targeted Python **5 PASS2.596s**: both roles, rounded quantities, parent
  restrictions, direct/projectile source contracts, malformed/duplicate/typed/
  out-of-range rows, nativeSQL/Python exclusion equivalence, period/player/rank/
  paging, no-record null, read-only auth and actual Japanese stats.js DOM.
- Final **8 C++ binaries +5 native DB→HTTP/DOM paths PASS** in
  `.build/travel-v2-tests/run-d5e918fee4c547d78b6de0fe3f8992c8/`.
  New absorption_tests exercises parent/ambiguity, quantities, source gates,
  origin once/exception preservation, production builder→StorageWriter→closed
  SQLite→native personal/rank→integrity=ok. Each key Alice30/Bob20; invalid
  rows excluded. The same14-row DB gives anonymous401/auth200 and Japanese
  JS totals50, ranks30/20 with1/10HP units. **Synthetic only**, not engine,
  real-player, browser, in-game GUI or other-Mod acceptance.
- Actual **23 native translation units / prelink / DLL link / strict PE /
  package PASS**, official22202 pinned headers verified (missing0/copied0).
  Root `.build/server-candidate/run-59ff52144ea24565ba8b3162482a31a4/`.
  DLL SHA256 `54D94DC520C7C79744835CB54FED62642A84BB80837AB16D73942C91F0521BF8`;
  ZIP SHA256 `A30F31D8BCCFE3D31A47D2CED80C3153ED0C8B1177AEF3A595950E2FD6028907`.
  Independent **105 source/script hashes match current**, DLL/ZIP match receipt;
  actual PE has **50 reviewed runtime symbols**. old_client_import_library_used,
  matched_server_sdk_verified, runtime_dependencies_verified, bds_load_verified
  and launch_authorized remain false. This is an official function-import
  candidate, **not staged or loaded**, not full SDK/release/runtime acceptance.
- Full Python **152 PASS67.976s**, optimized inventory **2 PASS0.005s**;
  **69 current inputs match before/after/current source**. Evidence
  `.build/stats-view-tests/run-85e2cb67eabc4919adb7452d366d1010/`.
  Native8-binary receipt independently matches **122 current inputs**.
  These are synthetic/read-path receipts, not actual-player acceptance.
- Final inventory `.build/parity-audit/run-6ec7cba06e834c8cb7f5a413842f70ca/inventory.json`:
  **96 mandatory rows, zero accepted**;77 CUSTOM rows actually grouped as
  CODE_ONLY69 / UNIT_ONLY4 / MISSING4. Optimized inventory replay after doc
  changes **2 PASS0.004s**. No global test upgrades individual runtime gates.
- Read-only preservation audit: protected BDS PID748 retains start
  2026-11-05 01:38:04; viewer PID28268 retains 2026-11-06 06:30:51 and listens
  only on **127.0.0.1:18767**. Protected run-2bbb... DLL SHA256 is still
  `B286F054EF1CCF9D90CABBBF3943543FEA2B6FF78805D2EB7F044FF89D6D8A00`.
  No claim that an owner's live DB bytes remain static; this increment made
  no protected-world/DB/DLL writes.

Commands with existing cached compiler/SQLite/package environment:

```powershell
python -W error::ResourceWarning -m unittest discover -s tests -p 'test_absorption.py' -v
.\scripts\verify_travel_v2.ps1 -WorldTick -FishBucket -RaidEffect -Absorption
.\scripts\build_server_candidate.ps1
.\scripts\verify_stats_library.ps1
python scripts/audit_lk_parity.py
```

Docs: `docs/ABSORPTION_CAPTURE.md`, updated dashboard/library guides and per-key
matrix. There are now **73 limited projections**, not73 accepted gameplay
counters. Four CUSTOM keys remain MISSING: damage_blocked_by_shield,
damage_dealt_resisted, damage_resisted, time_since_rest. All complete-parity
coverage and actual-player/SQLite/GUI/API gates remain required; neither enum
presence, a global compile, fixture data nor empty smoke upgrades acceptance.

Follow-on API audit, not an implementation/acceptance claim: ServerPlayer.h:427
has MCAPI $normalTick (Player.h:1177 $normalTick is MCNAPI); a durable rest-reset/
actual Player-tick counter still needs independent design, lifecycle and runtime
proof. The pinned shield reference uses may-block + no returned damage as an
inference; cancellation/invulnerability must not be silently called proven
shield mitigation. Resistance reference uses the post-enchantment basis and
effect amplifier to calculate a reduction, not a measured HP delta. Public
Mob::$getDamageAfterEnchantReduction / ActorDamageSource resistance predicates
exist; a truthful stage/quantity contract and tests are still required. These
four remain mandatory MISSING work, not a proved absence of all official APIs.

No BDS was staged/started/loaded by this increment; no protected world/DB/DLL
or running18767 was written/restarted. The coordinator's jump correction and
other existing work are preserved. No security/auth/network changes, no
GitHub push/release or deployment. MSPT already accepted, not repeated. Runtime
acceptance must use the authorized normal-host coordinator and fresh unused
world/ports. Previous host-tool approval-needed/never denial was not retried
or bypassed. Do not use an offline-sandbox online BDS retry as DLL evidence.

### 2026-11-07 — raid-related effect changes, not raid outcomes; remaining six CUSTOM source gaps

Continued independent action capture after the fish-bucket checkpoint. Reviewed
pinned upstream PlayerAddEffect/PlayerCombatStatsHandler factually, without
copying AGPL source/comments/translations. The old matrix's "effect API private"
claim was wrong: pinned26.60 Actor.h:679/782 declare addEffect/getEffect(uint)
MCAPI. Added `player_raid_effect.cpp` and core builder/strict SQL, existing
runtime lifecycle/writer/StatsReader/API/GUI and Japanese companion projection.

| Area | Implemented / verified | Not accepted |
| --- | --- | --- |
| raid_trigger | Same-call RaidOmen=36/600tick post-origin new/changed effect, named SDK enum | Actual raid trigger/participation, effect cause, real26.60 gameplay |
| raid_win | Same-call VillageHero=29/48000tick post-origin new/changed effect | Actual raid victory/contribution or all effect paths |
| Guards | Same weak live Player/Level/dimension, exact duration/amplifier, reject unchanged/denied/nested; origin once and engine exceptions preserved | Other-Mod/cmd grants are not distinguishable; no durable world proof |
| Native / SQLite / Web | Existing schema4/bounded writer; personal and CUSTOM ranking; JST periods/player/paging, anonymous401/auth200/Japanese actual JS DOM on synthetic DB | No game GUI/other-Mod API/player/browser acceptance, no deployment to18767 |
| Four dashboard categories | Playtime/activity/SQL ranks and genuinely accepted TPS/MSPT preserved | Full9/77 and all96 per-entry runtime acceptance gates remain IN_PROGRESS |

Japanese labels now explicitly say **襲撃前兆効果の付与観測** / **村の英雄効果の付与観測**.
Input-only requests are not counted. Unchanged same-value grants are excluded;
refreshes with changed values may count. This deliberately narrower observation
is not a full-parity claim or a real-raid counter. Cause
`ll26.60.raid_effect_changed.v1` separates it from old notifications.

Verification so far:

- Single-adapter actual pinned26.60 compile PASS in
  `.build/raid-effect-probe/run-71f6b1935ed84b98b4b863b962b360ba/`.
  Only the exact compiler-emitted addEffect/getEffect(uint) MCAPI names were
  added to the import allow-list. Initial probe failed on scalar TypedStorage
  `.get()` misuse; corrected to the header's scalar field (no ABI guess).
- Targeted Python **4 PASS2.240s**: both effect contracts, strict nativeSQL vs
  Python malformed/duplicate/type/value/source equivalence, new/refresh/empty,
  JST/day/month/all, selection/rank/paging, read-only auth/actual Japanese JS.
- Final **7 C++ binaries and4 native-DB HTTP/DOM readbacks PASS** in
  `.build/travel-v2-tests/run-4c670e98300c4f61a91489aec603cc4a/`.
  New raid_effect_tests proves gates/origin once/exceptions, production builder
  through StorageWriter/closed SQLite/native personal and ranking/integrity=ok.
  Each key has Alice2/Bob1, malformed/source/amount rows excluded; same14-row
  DB feeds anonymous401/auth200/actual stats.js. All **synthetic**, not engine
  hook invocation/real raids/browser/player proof.
- Actual **22 native units / prelink / DLL link / strict PE imports / package
  PASS**, official22202 pinned headers verified (missing0/copied0). Root
  `.build/server-candidate/run-03a8a761a7054953a3dee3c2f2012a81/`.
  DLL SHA256 `DA8DCD927232C6BF8D544BB1667B2ECE0576651A2C13DA6CE113AD73392B335A`;
  ZIP SHA256 `DF93FF12F7E77EAEF0EE158D3195EB301AA2B6258BDDC6BE7239EFC681868D0B`.
  Independent **101 source/script hashes match current**, DLL/ZIP match the
  receipt. Official function-import candidate, not full matched-SDK/runtime/
  release acceptance; bds_load_verified=false, launch_authorized=false,
  old_client_import_library_used=false. **Not staged or loaded**.
- Full Python **147 PASS67.153s**, optimized inventory **2 PASS0.004s**;
  **66 inputs match before/after/current source**. Evidence
  `.build/stats-view-tests/run-dfc8fb664cca49d9ad9d6b103d86ddcf/`.
  Native7-binary receipt independently matches **117 current inputs**.
  None of these receipts claims BDS/player/browser/persistent-viewer acceptance.
- A concurrent shell launch returned CreateProcessAsUserW denied5; waited for
  the existing fixture process to finish before one ordinary launch. No
  alternative identity/security bypass. Web refetch of the two fixed SDK
  raw URLs returned cache-miss; no verification was claimed from that fetch.
  The official22202-file local header/hash check remains the source evidence.
- Final matrix check: **77 CUSTOM rows: CODE_ONLY67 / UNIT_ONLY4 / MISSING6**.
  Corrected the old hand-maintained totals to the actual row counts. Final
  requirement inventory: **96 mandatory rows, zero accepted** in
  `.build/parity-audit/run-14cfbe5b6f6747689872312cf75a503c/inventory.json`.
  Replayed optimized inventory tests after documentation corrections:
  **2 PASS0.003s**. No enumerations/build/fixture promote a runtime gate.
- Final read-only preservation audit: protected BDS PID748 retains its
  2026-11-05 01:38:04 start; viewer PID28268 retains 2026-11-06 06:30:51 and
  LISTENING **127.0.0.1:18767**. Protected run-2bbb... DLL SHA256 remains
  `B286F054EF1CCF9D90CABBBF3943543FEA2B6FF78805D2EB7F044FF89D6D8A00`.
  This is process/listener/DLL preservation, not a claim that every live DB
  byte stayed static while its owner runs. No writes to any protected world/DB.

Commands with cached compiler/SQLite/package env:

```powershell
python -W error::ResourceWarning -m unittest discover -s tests -p 'test_raid_effect.py' -v
.\scripts\verify_travel_v2.ps1 -WorldTick -FishBucket -RaidEffect
.\scripts\build_server_candidate.ps1
.\scripts\verify_stats_library.ps1
python scripts/audit_lk_parity.py
```

Docs: `docs/RAID_EFFECT_CAPTURE.md`, per-key matrix and dashboard/library guides.
There are now **71 limited Web projections**, including shared world tick and
pre-origin damage observations, NOT71 accepted gameplay counters. Six mandatory
keys still lack a projection: damage_absorbed, damage_blocked_by_shield,
damage_dealt_absorbed, damage_dealt_resisted, damage_resisted, time_since_rest.
For rest, the pinned upstream mod source search found the key in the catalog,
not a producer. Public Player sleep thunks exist; the old "no wake API" statement
was not justified. A matching rest-origin/player-tick/persistence implementation
and real gameplay remain missing, not optional future scope.

Follow-up read-only API audit also corrected the remaining old availability
claims: current pinned SharedAttributes.h:38 declares ABSORPTION MCAPI;
Player.h:1451 declares $_hurt MCAPI; Mob.h:565 declares
$getDamageAfterEnchantReduction MCAPI. This does **not** establish usable
runtime linkage or correct absorbed/shield/resistance quantities. No unchecked
accessor, damage formula or new damage hook was shipped. The six unimplemented
keys are **MISSING**, not a proved absence of all official API alternatives.
The public WeakStorageEntity/StackResultStorageEntity implementation also
confirms that tryUnwrap<Player> calls Player::tryGetFromEntity, not a bare cast.

No BDS staging/start/load, no persistent viewer or protected-world/DB/DLL writes,
no security/network/auth changes, no push/release. MSPT already accepted, not
repeated. Runtime acceptance still needs the authorized normal-host coordinator
with a fresh isolated world/ports. Prior host-tool approval-needed/never denial
was not retried or circumvented.

### 2026-11-07 — same-call fish-bucket capture, not fishing-rod loot; native SQLite/Web fixtures verified

Implemented an independent `fish_caught` producer and corrected its Japanese
label to **水生Mobのバケツ捕獲観測**. Pinned LK v0.13.0's interaction hook is a
bucket-capture path, not fishing-rod loot. The old "Bucket API unavailable"
declaration was stale: pinned Player.h:979 and BucketItem.h declare both
Player::interact / BucketItem::$_useOn MCAPI. Upstream source was consulted
factually only; no source/comments/translations were copied. This implementation
uses a synchronous thread-local call context, not upstream's timed pending map.

| Area | Implemented / verified | Pending |
| --- | --- | --- |
| Native capture | Same main-hand target/ID in one Player::interact + BucketItem child, interaction success/swing, matched water bucket, one child, no nesting, same weak Player/Level/dimension/inventory, corresponding bucket net+1 | Actual player capture on26.60; all observed content IDs/flags |
| Target state | Weak target resolved after origin: removed_flag vs weak_unavailable recorded separately; no use of stale original Actor/ItemStack reference | Unavailable reference is not proof of final world deletion/commit |
| Storage / native reads | Existing PlayerInteractionResult/schema4/writer; cause fish_bucket_delta.v1, strict source contract; native personal/CUSTOM ranking and common GUI/API reader | Real event→SQLite→in-game GUI/other-Mod API acceptance |
| Japanese Web | /stats, authenticated /api/v1/stats, period/player/filter/ranking; honest null and bucket-only definition | Not deployed to18767; no actual-browser/player proof |
| Four dashboard categories | Existing playtime, other activity, SQL rankings and accepted TPS/MSPT preserved | Full9/77 and all mandatory acceptance gates remain IN_PROGRESS |

Source: new core/fish_bucket.hpp and fish_bucket_sql.hpp,
levilamina/player_fish.cpp/.hpp, runtime lifecycle, native reader/catalog;
new companion/fish_bucket_source.py and existing stats_view/catalog projection.
Registered in xmake and owned candidate/header-compile scripts. Import policy
adds only the two exact compiler-emitted reviewed MCAPI names. No guessed
actor numbers/offsets/virtual slots, NBT editing, per-tick DB writes or item/world
mutation. Main inventory snapshots are bounded to128 slots, only on eligible
interactions. Offhand, rod loot, no gain/creative paths, unknown types, multiple
children/nesting, failed interaction, live target and identity/inventory/Level/
dimension changes are omitted; they are not fabricated zeros or full coverage.

Verification:

- Targeted Python4 tests PASS before final whole-suite replay. Initial malformed
  fixture found native2048/Python4096 detail-length mismatch; fixed Python's
  fish-specific limit, not the SQL exclusion. Final protocol also distinguishes
  target retired state instead of falsely labeling an unresolved weak ref as
  a directly observed removed flag.
- Final **6 C++ binaries PASS** and3 native DB→HTTP/DOM readbacks PASS in
  `.build/travel-v2-tests/run-a9532d0a8c4645a48903cfc278a2cca1/`:
  travel_window, parity_observation, stats_reader, remote_api, world_tick,
  fish_bucket tests. The new test exercises exactly-once origin, preserved
  engine exceptions despite prepare/post/sink failures, success/gain/identity/
  nested bounds,7 exact type mappings, production builder→StorageWriter→closed
  SQLite→native personal/rank→integrity=ok. Alice7/Bob1; malformed/source/
  quantity/duplicate rows excluded. Same12-row native DB→anonymous401/auth200→
  actual Japanese stats.js DOM PASS (`fish-http-dom-readback.log`). **Synthetic
  only**, not a trace of real engine hook invocations or real gameplay.
- Prior protocol's6-binary PASS is retained in run-892e5a1db74048acba98f68b1ab46211;
  it is not substituted for the final target_after serialization above.
- Actual **21 native translation units / prelink / DLL link / PE / package
  PASS**, official22202 pinned headers verified. New owned candidate root
  `.build/server-candidate/run-c8675dc59f75415f89c5e545b92c284f/`.
  DLL SHA256 `57617CD0612499C9AE60FF845FD0857AD4106B0C571356D18890BBE23346F0F7`;
  ZIP SHA256 `52B38D175293D6D27FD1FA2F22F5D33B90AC498679E4AC924399E7D9A3DBB66E`.
  Independent current-source check: **97 source/script hashes match**, both
  artifact hashes match receipt. Official function-import candidate, not full
  matched-SDK/runtime/release acceptance: bds_load_verified=false,
  launch_authorized=false. No old client import library, no staging or load.
- Full Python **142 PASS62.979s**, optimized inventory **2 PASS0.005s**;
  **63 inputs match before/after/current source**. Evidence
  `.build/stats-view-tests/run-1427ed9c1a2d484795b1cc5f23a1ae4f/`
  (`regression.json` / both logs). Independent native fixture check:
  **112 current inputs match** the final6-binary receipt above. These receipts
  explicitly do not attest BDS/gameplay/persistent-viewer/full-parity acceptance.
- Final requirement inventory
  `.build/parity-audit/run-97a6075755604e54b258f9cb44d1e284/inventory.json`:
  **96 mandatory rows, zero accepted**. No shared build or fixture is promoted
  to per-key real-player/SQLite/GUI/API evidence.

Commands with the existing cached toolchain/SQLite environment:

```powershell
python -W error::ResourceWarning -m unittest discover -s tests -p 'test_fish_bucket.py' -v
.\scripts\verify_travel_v2.ps1 -WorldTick -FishBucket
.\scripts\build_server_candidate.ps1
.\scripts\verify_stats_library.ps1
python scripts/audit_lk_parity.py
```

Docs: `docs/FISH_BUCKET_CAPTURE.md`, newest Analytics/Stats library sections
and corrected per-key matrix.69 limited Web projections include the world
snapshot and2 pre-origin damage amounts; not69 proven gameplay counters.
Remaining8 unprojected mandatory keys: damage_absorbed,
damage_blocked_by_shield, damage_dealt_absorbed, damage_dealt_resisted,
damage_resisted, raid_trigger, raid_win, time_since_rest. All real-action/SQLite/
GUI/API acceptance remains pending; no enumeration/fixture/empty smoke promotes
any gate. Full LK parity stays **IN_PROGRESS**.

No BDS was started/staged/loaded, no persistent viewer started, no live SQLite
or world/DLL changed. Preserve18767 and all protected servers. MSPT's genuine
09:18 acceptance is done and was not repeated. The previous normal-host ping
was denied by approval-needed/never policy; no retry via another tool/identity
or offline online-BDS attempt. New runtime acceptance needs the normal-host
coordinator's fresh isolated world/ports and scoped real-player actions.
No security/auth/network changes, GitHub push, release or production deployment.

### 2026-11-07 — engine world-tick snapshot, not telemetry SUM; native/Web readback verified

Implemented `minecraft:total_world_time` as a **shared Level tick ID snapshot**,
not per-player playtime, not wall-clock/20TPS conversion, and not a sum of
persisted busy-tick windows. Pinned upstream Form.cpp displays this Level value;
the earlier matrix's missing *player-specific* API did not prove that the
shared API was unavailable. No upstream code/comments/translations were copied.
The prior native SUM(busy samples) projection was corrected; raw history remains.

| Area | Implemented / verified | Still pending |
| --- | --- | --- |
| Native source | Pinned public virtual `Level::getCurrentServerTick()` / declared Tick.tickID; one system snapshot per supported >=5s interval window, existing bounded writer | Real26.60 execution/SQLite in fresh normal-host BDS |
| Persistence | New cause `ll26.60.world_tick_snapshot.v1`, existing ServerTick kind/schema4; no enum renumbering, migration or user DB edits; no fake MSPT sample | Runtime queue/stop behavior beyond synthetic writer |
| Native personal/API | Last valid world snapshot shared by known players; older larger run not chosen with MAX, no SUM; measured zero retained; world ranking explicitly rejected | Real game GUI and other-Mod interoperability |
| Japanese Web | `/stats` and authenticated read-only `/api/v1/stats`; world scope, period's last observation, no player multiplication/ranking, stale>30s, null vs measured0, opaque run_key | Actual browser visual/player acceptance; not deployed to18767 |
| Four dashboard categories | Existing playtime/activity/rankings and genuinely accepted TPS/MSPT preserved | Full9/77 semantics, missing9 keys, per-key gameplay/GUI/API gates |

Changed: new `core/world_tick.hpp`, `world_tick_sql.hpp`, the native on_tick
capture and StatsReader projection/label; new `companion/world_tick_source.py`,
Stats library projection/catalog and existing `stats.js`. New native/Python/DOM
fixtures and `verify_travel_v2.ps1 -WorldTick` option. No guessed offset/virtual
slot/private `$getCurrentServerTick` call, no packet hook or per-player/tick DB
writes. Public virtual-call compilation is not proof of runtime ABI behavior.

Verification so far:

- Targeted Python **5 PASS1.416s** (world periods/latest/zero/restart/stale,
  native/Python SQL equivalence and malformed rows, read-only HTTP/Japanese JS).
- Extended native fixture **5 binaries PASS**: travel_window_tests,
  parity_observation_tests, stats_reader_tests, remote_api_tests, world_tick_tests.
  Evidence `.build/travel-v2-tests/run-081d647e18c34eeea991bf5755055b73/`.
  Same production snapshot builder→StorageWriter→SQLite→native/API exercised;
  values1000000→40→0→9 choose the latest, both players see9, rank rejects the
  shared metric; invalid rows excluded, closed integrity=ok, zero legacy MSPT
  samples created. `world-http-dom-readback.log` proves same native DB→anonymous401/
  auth200→actual Japanese stats.js DOM. All are **synthetic, not BDS/player proof**.
- Actual candidate **20 native translation units / prelink / DLL link / PE /
  package PASS**, pinned22202 headers verified. New owned root
  `.build/server-candidate/run-732292c357514063a25ae74401d3e4ec/`.
  DLL SHA256 `313E073E2AE42D89996F7BF2CED059EE2AF0082F488B0B68536E6B08A94A68E9`;
  ZIP SHA256 `2AD81259F4BB982304A4F13A799659AB518E8E48E6A2F81D095D2C8F1B1EA6C2`.
  Independent final check: **93 native source/script hashes unchanged**,
  DLL/ZIP match their receipt. No new import-policy allowance was needed for
  this public virtual API. `candidate-evidence.json` retains
  bds_load_verified=false and launch_authorized=false; full matched-SDK,
  runtime-dependency and normal-release attestation remain false. No old client
  import library used. This DLL was **not staged or loaded**.
- Full Python **138 PASS60.577s**, optimized inventory **2 PASS0.005s**,
  **60 captured inputs match before/after and current source**. Evidence
  `.build/stats-view-tests/run-632ee1da2fc4425cad9f3b43a38ea052/`
  (`regression.json`, both test logs). It explicitly does not attest DLL/BDS/
  persistent-viewer/full-parity acceptance. Native fixture receipt independently
  passed with **107 captured inputs still matching**, including the new world test.
- Final requirement inventory:
  `.build/parity-audit/run-8d68640c1b4c4bcdbef6755a1b5db96e/inventory.json`,
  **96 rows, zero accepted**. No shared compile/fixture result promotes any
  per-key real action/SQLite/GUI/API gate.
- Earlier fixture runs retained: run-6d148ffecdc14ee9809c81e29fbe9fb7 found a
  C++ string-concatenation typo, fixed before native link; run-9738e2191e2c45c2b4877de19b1d83b6
  passed the prior4 binaries but the new API test wrongly expected key/value
  adjacency despite the Japanese label field. Corrected the test to inspect
  that object's actual value; no production API/validator weakening.

Commands (same cache environment as the prior section):

```powershell
.\scripts\verify_travel_v2.ps1 -WorldTick
.\scripts\build_server_candidate.ps1
.\scripts\verify_stats_library.ps1
python scripts/audit_lk_parity.py
```

Docs: `docs/WORLD_TICK_METRIC.md`, newest Analytics/Stats library and per-key
matrix sections.68 limited projections now include1 world-shared snapshot and
2 pre-origin damage amounts; **not68 verified gameplay counters**. Remaining9
without a Web projection: damage_absorbed, damage_blocked_by_shield,
damage_dealt_absorbed, damage_dealt_resisted, damage_resisted, fish_caught,
raid_trigger, raid_win, time_since_rest. These remain mandatory. All96 actual
acceptance rows remain unaccepted. `PROJECT_STATUS: IN_PROGRESS`.

Runtime boundary: discovered the existing Praxis Sub PC normal-host tools and
attempted only its harmless ping. It returned **"MCP tool call requires approval,
but approval policy is never"**. No host shell/BDS was started and no alternate
tool/identity/security workaround was attempted. The authorized normal-host
coordinator must perform fresh isolated runtime validation; do not retry online
BDS in CodexSandboxOffline or redo accepted MSPT. A local diagnostic shell spawn
during native compilation separately failed CreateProcessAsUserW denied5; kept
the running build, did not change shell/security or treat it as a DLL crash.
Protected BDS/18767/old worlds/DB/DLL remain outside all writes; no GitHub push,
release, new persistent viewer or production deployment. Sites skill followed
the existing local-only checkout workflow, no registration/sync/publication.
Final read-only preservation check: BDS748 started2026-11-05 01:38:04 and
viewer28268 started2026-11-06 06:30:51 are unchanged; netstat confirms
127.0.0.1:18767 LISTENING28268. Protected original DLL SHA256 still
`B286F054EF1CCF9D90CABBBF3943543FEA2B6FF78805D2EB7F044FF89D6D8A00`.

### 2026-11-07 — native travel v2 linked; fractional/continuity/JST fixes, SQLite/HTTP/Japanese readback verified

Implemented an independent replacement for the existing server-position travel
sampler, not a restart of Phase1 or MSPT. Full9/77 parity remains **IN_PROGRESS**.
Read the full-parity/integration/analytics instructions and pinned26.60 inputs;
kept existing SQLite schema4, bounded queue/single writer, session analytics,
rankings and accepted MSPT. No BDS started, staged or loaded in this increment,
no persistent viewer started, and no change to18767/security/auth/network/GitHub.

| Area | Enabled in source / verified here | Pending boundary |
| --- | --- | --- |
|17 travel modes| New `ll26.60.poll_position_delta.v2` fixed windows; fractional cm summed then floored; exact vanilla vehicle IDs, exclusive water modes,3D swim/fly/glide/riding | Actual game flag/vehicle IDs and all modes in a real authenticated player run |
| Climbing | Official pinned `Actor::onClimbableBlock() const` for ascending samples; exact MCAPI symbol resolved in candidate PE, no block-name heuristic | Actual ladder/vine/scaffold behavior; link is not runtime acceptance |
| Continuity / time | Reset on gap>1s, step>10blocks, dimension/mount/vehicle/death changes, invalid/reversed clocks, JST day; final bucket uses last actual observation | Conservative omissions; short teleport detection and complete LK numerical parity unproven |
| Persisted history | v1 retained unchanged, v2 versioned contract; no live DB edits/migration. Shared native event builder→StorageWriter→closed SQLite→StatsReader/ranking/integrity tested | Real player-event-to-SQLite acceptance |
| Web / rankings | `/actions` and `/stats` accept strict v2 with source/units/limits, day/month/all, selected player/type and stable ranking; actual fixture HTTP JSON→actual Japanese JS DOM | Not a visual browser screenshot, game GUI or other-Mod runtime call |
| Playtime / TPS / MSPT | Existing playtime/session safety, authenticated UI and accepted telemetry preserved; regressions pass | No re-acceptance/rework of genuine09:18 MSPT required |
| Full LK features | Existing9-category/77-key library and3 commands/4 APIs preserved; per-key matrix corrected for v2 |10 keys still lack projections; all mandatory gameplay/GUI/interop gates remain open |

Source: `core/travel_observation.hpp`, new `travel_window.hpp`,
`travel_event.hpp`, `travel_sql.hpp`, `levilamina/player_travel.cpp/.hpp`,
`core/stats_reader.cpp`, new `companion/travel_source.py`, `analytics.py`,
`stats_view.py`. No guessed offsets/packet hooks, no per-tick SQLite writes.
The PE import policy adds only `?onClimbableBlock@Actor@@QEBA_NXZ`, declared
MCAPI in pinned `Actor.h`; no broad allowlist relaxation or resolver rewrite.

Reproducible commands using existing caches:

```powershell
$env:BEDROCKINSIGHT_CLANGCL_ROOT='C:\Dev\LL2660Toolchain-20261029\clang+llvm-23.1.4-x86_64-pc-windows-msvc\bin'
$env:BEDROCKINSIGHT_SQLITE_ROOT='C:\Users\chihirosub\AppData\Local\.xmake\packages\s\sqlite3\3.54.0+100\b897e7007f3d49769c227e337e14fdef'
$env:BEDROCKINSIGHT_XMAKE_PACKAGES='C:\Dev\LL2660BuildCache\.xmake\packages'
.\scripts\build_server_candidate.ps1
.\scripts\verify_travel_v2.ps1
.\scripts\verify_stats_library.ps1
python scripts/audit_lk_parity.py
```

- Actual native candidate **compile/prelink/link/PE/package PASS**,20 native
  translation units, separate build root
  `.build/server-candidate/run-38e460add06742628c8ff265945eaebc/`.
  DLL SHA256 `993F3858F64F164F90E2D819DEF70493516ECF0CE7BF88BE30011B6792ACF1F4`;
  ZIP SHA256 `27093B98FCC1BDAD43C0F3EF7FC6A55AFEDE7A6D720DF79DC0A3AE19C908118E`.
  Independent final check: **91 captured native source/script hashes match**,
  DLL/ZIP hashes match. `mod-pe.txt:577` contains the exact climb import.
  `candidate-evidence.json` correctly retains launch_authorized=false,
  bds_load_verified=false, matched_server_sdk_verified=false and
  runtime_dependencies_verified=false. This is not a full SDK/runtime or
  normal release attestation. Old client import library was not used.
- **4 C++ binaries PASS** (`travel_window_tests`, `parity_observation_tests`,
  `stats_reader_tests`, `remote_api_tests`) in
  `.build/travel-v2-tests/run-1845c7962d3e421a9dcfe076abc39025/`.
  `fixture-evidence.json` passed=true; captured native inputs still match.
  Production builder/writer emitted a closed7-row synthetic DB: v2walk19 +
  v1walk5 gives Alice24; Bob10; malformed/duplicate/mismatched rows excluded;
  integrity_check=ok. Same DB→anonymous401/auth200→both Japanese JS views PASS
  (`http-dom-readback.log`). This is synthetic, **not real gameplay**.
- Full Python **133 tests PASS69.872s**, optimized inventory **2 PASS**,
  **57 captured inputs unchanged**, final independent hash check also matches.
  Evidence `.build/stats-view-tests/run-bb4f6a25dad74ca8a9644bdd5cd5e796/`
  (`regression.json`, `python-tests.log`, `optimized-inventory.log`). Receipt
  explicitly says no DLL/BDS/persistent viewer/full-parity acceptance.
- Updated the17 per-key movement acquisition rows and corrected climb's old
  MISSING declaration; final requirement inventory
  `.build/parity-audit/run-b22dc10235974d56a3630cdee91fa323/inventory.json`
  still has **96 mandatory rows, zero accepted**. Inventory tests rerun after
  the matrix edit:2 PASS. The inventory intentionally does not convert a
  shared compile/fixture receipt into per-key actual-player proof.
- Earlier failed native harness runs are retained: run-1275de3b46464016921ffbf4621ed485
  lacked two core dependencies, run-6dea1b6ecc29430bb160b34866064882 used an
  external custom: selector at the internal bare-key API, and
  run-7d25937f250d4e33b4a363d3de5fbb29 had invalid legacy fixture JSON.
  Fixed the harness/fixture, not the strict reader. Earlier Python run-a2c009f44c654f7bba8baa7ca820d112
  failed2 expected definition-wording checks; restored explicit period and
  non-completeness wording, then reran all133. No security/runtime workaround.
- Sites skill followed local-only guidance for the existing checkout; no
  registration/sync/publication, new site/project or external hosting. Existing
  upstream AGPL source was read for factual mode/unit comparison only; new
  window/event/SQL/Python code and documentation were independently authored.

Read-only preservation audit: BDS748 still started2026-11-05 01:38:04;
viewer28268 still started2026-11-06 06:30:51, listening only at127.0.0.1:18767.
Protected original test DLL remains
`B286F054EF1CCF9D90CABBBF3943543FEA2B6FF78805D2EB7F044FF89D6D8A00`.
No file in either protected server/world was changed. No player UI operated.

Docs: `docs/TRAVEL_CAPTURE_V2.md`, updated `docs/ANALYTICS_DASHBOARD.md`,
`docs/STATS_LIBRARY.md`, per-key `docs/LK_FULL_PARITY_MATRIX.md`.
Remaining10 unprojected keys: damage_absorbed, damage_blocked_by_shield,
damage_dealt_absorbed, damage_dealt_resisted, damage_resisted, fish_caught,
raid_trigger, raid_win, time_since_rest, total_world_time. The67 limited
projections include2 pre-origin damage observations, not final HP outcomes.
No mandatory key has new actual-player acceptance. Full in-game commands,
other-Mod4API interoperability, capture completeness, translations and history
acceptance remain mandatory, not optional follow-ups.

Next real online-BDS/player validation requires the existing authorized
**normal-host coordinator**, fresh unused world/ports and the participants'
explicitly scoped actions. Do not run online BDS inside CodexSandboxOffline,
reuse old scratch or change protected BDS/viewer/security. Use the latest
candidate/hash/source receipt and the v2 mode checklist, not an old smoke or
MSPT proof. Current native DLL was **not staged or loaded**.

### 2026-11-07 — authenticated nine-category/77-key Web library; local regression verified

Added the independently authored Japanese `/stats` interface and authenticated
read-only `/api/v1/stats` to the existing Python companion. This increment is
**not deployment to18767, not a native DLL build, and not full LK parity**.
Sites skill used in local-only mode: retained this checkout/implementation,
skipped external registration, synchronization and publication. No new project,
hosting credentials, network/security change or Git push/release.

| Area | Implemented / verified here | Still pending |
| --- | --- | --- |
| Playtime | Existing root dashboard retained; new CUSTOM closed-session lower bounds, union/clipping/JST/no abandoned-open growth fixtures | True continuous play-tick parity and new actual-play acceptance |
| Activity |9 categories /77 selectable keys, independent Japanese labels, type details, source definitions, unknown/null states; later concurrent source projections preserved | Remaining capture/semantic gaps and every mandatory actual-player gate |
| Rankings | SQL-backed day/month/all, selected-player details vs global same-metric ranking, literal Japanese/ID search, exact type filter, stable ties, independent bounded pages;253-type fixture | Actual recorded game actions across all keys and in-game GUI/API acceptance |
| TPS / MSPT | Accepted09:18 baseline preserved; existing live/stale/no-sample tests pass | No new MSPT work needed; not redeployed to protected viewer/BDS |
| Authentication | Loopback-only / Bearer / same-origin / read-only reused; request races,401 logout,503 clearing and literal names exercised with actual fixture HTTP JSON and real JS DOM | No actual-browser visual or real-player screenshot claim |
| In-game GUI /4 calling APIs | Existing independent implementation preserved, no C++ edits/build/staging in this increment | Full77 gameplay, three real GUI commands, other-Mod/API compatibility and per-key acceptance remain required |

Implementation: `companion/stats_catalog.py`, `stats_view.py`, the new
`web/stats.html/.js/.css`, companion server routing and `/actions` navigation.
Old root `index.html`, `app.js`, `styles.css` were not edited. All quantities
remain source-scoped observations, not a guarantee of complete capture. Current
catalog has67 limited projections (including2 **pre-origin amount** observations)
and10 without a projection; those counts are not native/runtime acceptance.
Original26/27 projection counts in earlier progress are superseded.

Follow-up inspection of the concurrent merged producers found misleading
“real damage” wording for `phase:pre_origin_amount`. Corrected both `/stats`
and `/actions` labels/definitions and added `observation_stage` plus
`final_world_outcome_verified:false`. Later cancellation/modification and final
HP/absorption/resistance are NOT measured by these rows. Preserved raw history;
no counter was fabricated or promoted to a final-world result. New strict
checks pair use-method ordinal/name, CreatedOutput screen/branch, integer
result0 (not bool false), cause-code fields and source entity flags. New usage
sources without their serialized contract are excluded. PvP pre-origin amount
is not incorrectly dropped solely because victim type is minecraft:player.

Tests / replay:

```powershell
.\scripts\verify_stats_library.ps1
python scripts/audit_lk_parity.py
```

The verifier invokes `python -W error::ResourceWarning -m unittest discover -s
tests -p 'test_*.py' -v`, then optimized inventory tests, captures before/after
SHA256 for all companion/test source inputs plus the verifier/log helper, and
fails closed on source-list/hash drift. It starts only fixture-owned ephemeral
loopback HTTP servers, shuts them down, and never starts BDS or a persistent viewer.

- Final **129 tests PASS55.564s**, optimized inventory **2 PASS**, all **54
  captured inputs match before/after**. Durable evidence:
  `.build/stats-view-tests/run-c1268e0a44df496fa9af2ce05408511d/`
  (`python-tests.log`, `optimized-inventory.log`, `regression.json`). Receipt
  explicitly has native_dll_built/bds_started/persistent_viewer_started/
  full_parity_accepted=false.
- Earlier129 PASS55.805s /52-input guard is retained in run-4f0c8a82251c489c87674921cab900f6.
  Earlier123 PASS and the16-test target-description mismatch during concurrent
  edits are retained in run-1117ab4cbc704bae9d063be2d5a9d388; neither is claimed
  as verification of the later source. Concurrent additions were preserved.
- Latest inventory at this point: `.build/parity-audit/run-2b2c2f8a9730476b84c84faf690263a3/inventory.json`,
  **96 mandatory rows, zero accepted**. No enum/label/fixture/empty BDS smoke
  promotes a gameplay gate. License provenance and native safety review of
  concurrent additions remain separate from this local Web regression.

Usage/semantics: `docs/STATS_LIBRARY.md` and the newest section of
`docs/ANALYTICS_DASHBOARD.md`. Example separate-process URL
`http://127.0.0.1:18768/stats` is **not started**; use only an explicit approved
DB and a separately available port, fresh random viewer token and private SSH
forward if needed. Preserve18767. Protected BDS748 / viewer28268 and original
DLL SHA256 B286F054EF1CCF9D90CABBBF3943543FEA2B6FF78805D2EB7F044FF89D6D8A00
were read-only checked unchanged. No real game actions were exercised here.

Full9/77 + all commands/APIs/history/translation acceptance remains mandatory
and **IN_PROGRESS**. Future real online-BDS acceptance must use the authorized
normal-host coordinator with fresh isolated world/ports, never an offline
sandbox retry or protected-server modification. Existing MSPT acceptance is done.

## 2026-11-07 source audit correction: actual 26.60 interaction and trade producers

This entry supersedes the previous leading report, which described hooks that are not present in this source tree. Full LK-Stats 9-category/77-CUSTOM parity remains IN_PROGRESS; real-player capture acceptance has not passed.

- `src/bedrock_insight/levilamina/player_block_interaction.cpp` is the existing native producer: `Block::use` post-origin `true` (`ll26.60.block_use_success.v1`) plus dedicated CakeBlock, CauldronBlock, FlowerPotBlock and NoteBlock success paths. No separate GameMode-useItemOn hook exists. The `PlayerInteractBlockEvent` notification is not counted as a confirmed CUSTOM interaction.
- `src/bedrock_insight/core/interaction_observation.hpp` declares 32 `kInteractionKeys` (24 Block::use keys and 8 dedicated-key variants). `interaction_key_cause()` maps each key to its real producer. The native `StatsReader` accepts only those six post-outcome block-interaction causes for these keys.
- `src/bedrock_insight/levilamina/player_trade.cpp` only hooks `ServerPlayer::$openTrading` for `talked_to_villager`. The committed CreatedOutputContainer transaction hook resides in `player_item_transfer.cpp` and emits `ll26.60.trade_created_output.v1` for `traded_with_villager` after success. The reader also accepts a historical `trade_result_transfer_success.v1` cause, but the current production tree has no 33/49 -> 12/28/29/59 trade hook implementing that older contract; `core/trade_observation.hpp` is a classifier, not proof of a runtime hook.
- `src/bedrock_insight/core/stats_reader.cpp` handles real `ActorHurtEvent` damage amounts, currently produced created-output craft, item-use and enchant cases, committed trade, and per-key CUSTOM ranking. Unit-test fixtures do not prove player activity.
- `scripts/validate_server_release.ps1` contains a bounded reviewed 26.60 import allowlist. Allowlisting an imported method does NOT prove that a particular hook is registered.

Current checked build: `.build/server-candidate/run-6a952bab73fd43afbbcb56fbf1ab4530/BedrockInsight.dll` SHA256 `EAB1A3026A89B15E3A90DFE47EB86871C8FC46E97B40CD5786E2B79756B2F63D`. C++ native header compile, link, PE validation, fake SDK native safety tests, stats-reader/RemoteAPI fixture tests, and Python 126 tests passed. A fresh isolated empty-allowlist BDS run at `.build/isolated-bds/run-09550349139d407caa56c11e7d03510a` loaded/enabled/stopped cleanly and opened SQLite. These results do not imply any real-player event-to-database/UI/API acceptance; accepted gates remain zero.

Release/actual-player test must be reviewed against this actual source inventory and the separate HIGH/CRITICAL review policy. Claims in older dated historical notes below are not evidence of missing hooks being implemented.

## 2026-11-07 Command Code: target_hit web projection + current-tree native re-verification

- Added the missing `minecraft:target_hit` CUSTOM projection in
  `companion/stats_view.py` with a strict post-origin contract
  (`TargetBlock::$onProjectileHit`, `post_origin_hit`, resolved owner, matching
  projectile). The native producer and C++ reader already existed; the Japanese
  web dashboard could not display/rank the key before this. Added a positive +
  negative fixture test in `tests/test_stats_view.py`; moved the attempt-only
  expectation to `no_record`.
- Made `tests/parity_observation_tests.cpp` compile under MSVC `/W4 /WX`
  (C2124) by using `std::numeric_limits<double>::infinity()` instead of
  `1.0/0.0`. No product-semantic change.
- Corrected `docs/LK_FULL_PARITY_MATRIX.md` row 71 (target_hit BLOCKED →
  CODE_ONLY) and the stale status aggregate.
- Current-tree native verification with MSVC 2022 + throwaway SQLite
  (disposable fixtures, no live BDS): core, stats_reader, remote_api,
  killed_by_storage, tick_interval, busy_tick, state_transition,
  block/drop/pickup/use/jump/hurt/direct_kill, parity_observation, player_mode
  all PASS; Python `unittest discover tests` 124 PASS.
- NOT a full-parity or player-test-ready claim. This environment has no
  clang-cl/llvm tools/reference SDK, so no current-tree server DLL candidate was
  built here. See `docs/OVERNIGHT_PLAYER_TEST_HANDOFF.md` (NOT_READY).
- Protected live BDS, companion 18767, auth/firewall/network and other projects;
  no git push/release.

## FINAL AUDIT 2026-11-07 — concurrent source drift; do not treat an older DLL as current-tree validation

The independent killed_by/projectile/paging work below produced candidate15E4
with81 native source/header +3 critical script hashes matching at build and at
the first post-build audit. AFTER that, concurrent Command Code edits changed
`core/stats_reader.cpp`, `levilamina/runtime.cpp`, `levilamina/runtime.hpp` and
the3 builder/header-compile/PE-policy scripts (06:15:54..06:17:05 JST). Final
audit correctly FAILED current-tree hash match. These changes are preserved;
no reset/revert/overwrite or interference with the other build/process.

- Candidate15E4/ZIPF980 remains a successfully built **recorded snapshot**, NOT
  proof that the NOW-current working tree compiles/runs. Its captured source/
  script hashes are retained in run-a2b17a0840b446e3b8a77d1df9cacdbe.
- Also checked coordinator's candidateFEB82D /run-43f928ed49d8449a80f100a2eb3d08fe:
  its stored source hashes likewise differ for the same3 C++ files. Its earlier
  normal-host empty smoke must NOT be attributed to those later source edits or
  actual trading/breeding/gameplay. That used world must not be reused.
- This turn's latest Python108 PASS48.617s, optimized inventory2 PASS, recorded
  C++ reader/API/storage/helper fixtures and96-row zero-accepted inventory remain
  scoped evidence, not a blanket native-current-tree or full9/77 acceptance.
- Need coordination/source freeze before another definitive current-tree
  build/test/normal-host handoff. Do not keep rebuilding against active edits or
  promote snapshots to latest/full PASS. User direction/one coordinated owner
  is needed for this final verification boundary. Remaining mandatory capture
  gaps still exist independently of this concurrency issue.

Protected BDS748, viewer28268/127.0.0.1:18767, original DLL hash and earlier real
MSPT acceptance preserved. No host online BDS was started by this work; no
security/auth/network changes, Git push/release or unrelated project changes.

## 2026-11-07 — Command Code: villager trade + breeding post-origin hooks

Independent Command Code increment (full 9/77 remains mandatory; this is not a
completion claim). New MCAPI hooks on the pinned 26.60 headers:

- `player_trade.cpp`: `ServerPlayer::$openTrading` after origin; resolves the
  target and counts `minecraft:talked_to_villager` only for
  villager/villager_v2/wandering_trader. Pre-origin right-click is not counted.
- `player_breed.cpp`: `BreedableComponent::mate`; copies `mLoveCause` before
  origin, requires a non-empty offspring list and a resolved online Player, then
  counts `minecraft:animals_bred`. Feeding without offspring is not counted.
- `player_target.cpp`: `TargetBlock::$onProjectileHit`; after origin resolves the
  projectile owner and counts `minecraft:target_hit` only for an online Player.
  An unowned/environment projectile is never attributed.
- Reader: all three keys surface in `minecraft:custom` and are rankable only by
  the exact `custom:minecraft:<key>`; unit mixing still refused.
- Delay-import allowlist extended by exactly the reviewed MCAPI symbols used
  here (`openTrading`, `mate`, `onProjectileHit`, `getOwner`).
- Fixture tests assert all three keys and their per-key rankings.

Verified in this increment: pinned-header `/W4 /WX` compile; full server-derived
candidate build/prelink/link/import resolution; fresh isolated BDS
`.build/isolated-bds/run-368608bfa2c342c482e9f34ee0713fb3` with
`native_loaded=true`, `native_enabled=true`, `server_started=true`,
`exit_code=0`, `forced_termination=false`, `database_exists=true`.
LATEST candidate DLL SHA256
`89DC8A3FD1E45DCE21B9E93CAABC09C1AE74DAFB2B7DB37213C55BD9674391FB`,
ZIP `707ADB46B6933C3A23BEA28176AFB8F06A2DBC82466FFE9B7ED8F5E558C7E0E0`
(`.build/server-candidate/run-71022aa4b5e149af9b01030339422df9`; built
2026-11-06T21:25:05Z with the then-current tree, source hashes verified before
and after). The earlier FEB82D snapshot is superseded by this build.
No real player has traded, bred, or hit a target with this DLL; actual
event→SQLite remains unverified for these three keys.
See `docs/COMMANDCODE_TRADE_BREED_20261107.md`.

## 2026-11-07 — typed killed_by/projectile capture and complete bounded personal paging

Full9/77 mandate remains mandatory. MSPT acceptance is preserved, not repeated.
Read full parity/LK/latest status/official inputs and existing source first. No
AGPL implementation, comments or translations copied. Sites skill was applied
only as a LOCAL existing-checkout workflow: no registration/publish/new project.
No protected BDS/world/DLL/old SQLite/18767 or security/network/auth/GitHub writes.

Implemented independently:

- `player_death.cpp`: preserve v1 `cause:N` death contract; append separate
  `PlayerKilledByEntity` / `ll26.60.resolved_killer_dead_transition.v1` with an
  actual resolved namespaced actor type. Direct cause2/equal IDs or child
  projectile cause3/distinct nonzero IDs, same-Level source actor, not self.
  Snapshot before origin; weak same-entity dead transition and no nested calls
  after origin. No source/old attacker dereference after origin. Unresolved,
  environment, block and other causes excluded from typed attribution. Type
  capture failure does not discard the original death observation.
- `player_kills.cpp`: direct source remains direct-only with its old validation
  flag restored. Child projectile gets NEW source
  `ll26.60.projectile_owner_dead_transition.v1`, strict signed64 ID relationship,
  same-Level resolved Player. Do not reinterpret expanded data as old v1 direct.
- Append-only event text/daily type `killed_by_entities`; schema4/history kept.
  Typed attribution does NOT increment victim deaths/kills again. Two bounded
  queue submissions are not an atomic pair and can independently drop.
- Native killed_by personal/ranking SQL and strict Python read path reject
  numeric causes, malformed/duplicate JSON keys, missing/wrong typed flags,
  unknown attribution, invalid type/ID/time. Public output omits raw player and
  source/damaging identifiers. Native killed/CUSTOM mob-player totals include
  both named kill sources; other older native categories still need a uniform
  strict-reader audit and are not full acceptance.
- Japanese `/actions`: separate killed_by entity/path list + ranking and
  projectile-owner kill list + ranking. Does not mix environment deaths or
  projectiles into direct-only values; uses textContent and clears on logout.
  Existing18767/root assets untouched; new features require a separately
  user-started authenticated loopback companion. No persistent viewer started.
- Removed the200-type truncation from native personal reader, GUI and C API.
  All-type count/total with bounded retained page,253-type fixture, CUSTOM time
  rows sorted/paged together. GUI12/page,API1..100/page; empty total=null,
  CUSTOM mixed-unit total=null; metadata total_entries/has_more and explicitly
  collection_complete=false. Cardinality budget100000, busy wait50ms and
  cooperative SQLite VM100ms deadline (not hard OS/disk/game-thread latency).
- Public four-function C ABI declarations + documented ownership/error codes;
  noexcept exception containment(-5), clear stale output on negative return.
  NOT LegacyRemoteCall wire compatibility or actual other-Mod verification.
- Preserved intervening pre-origin hurt observations, but again excluded them
  from confirmed CUSTOM damage_taken/dealt: the source explicitly records
  phase=pre_origin_amount despite its misleading historical 'real' source name.
  GUI category read failures/unknown category now fail visibly, not fake empty
  success or raw UUID fallback. Other mandatory source semantics remain pending.

Verification (owned disposable fixtures only): targeted15 Python tests
PASS9.300s incl401/200→Japanese DOM; final Python108 PASS49.488s;
4-category/auth/escaping/stale/empty/logout DOM PASS (not browser rendering).
Final C++ reader/API/actual StorageWriter fixtures PASS, including253 types and
typed attribution without duplicate deaths; observation helpers PASS;
PE policy10 valid/34 negative PASS. Logs
`.build/player-action-tests/run-fc85ed32b5404090977bdf674888a9cd/`;
reader evidence `.build/stats-reader-tests/run-512483ce341a4d4c8b69625730bdd40b/`;
helper evidence `.build/player-action-tests/run-ede05b782a334b11b97fe526e69a03cd/`.
Final candidate evidence will be recorded below after compilation; no BDS claim.
One C API /W4 /WX compile correctly rejected added dllexport on a prior plain
declaration; fixed with export-only header macro, no warning suppression.
One concurrent exec launch returned CreateProcessAsUserW denied5 while a native
compile was active; waited for that job, no security change or rapid retry loop.
The first candidate attempt then encountered a REAL shared compile.log file
lock: another build was using `.build/pinned-sdk-header-compile`. Did not stop,
clear or overwrite the other process/output. Candidate builder now passes a
NEW candidate-owned native-header-compile directory; verifier rejects reuse,
out-of-project output and redirected ancestors. Historical standalone/probe
default remains unchanged for compatibility. Retrying only the changed-source
build with its OWN outputs; not re-running successful Python/core tests.
Concurrent coordinator added player_trade/player_breed native units; preserve
them but do not treat this turn's reader/attribution fixtures as their gameplay
acceptance. Any source change during build must still fail the existing hashes.
Own isolated attempt `run-09e4f79bf9164e369a0944b1c983bd05` compiled19 native
units/linked, then correctly FAILED PE policy (28 actual delay names vs26 max).
The two additional names were independently checked against pinned official
ServerPlayer::$openTrading and BreedableComponent::mate declarations AND actual
llvm-readobj output, not guessed. Added only their exact mangled names/max28;
breed also requires reviewed weak-reference API. Negative tests reject wrong
return/class/parameter shape and missing weak API:12 valid/40 rejected PASS.
Post-rejection exact delay policy audit on that DLL passed, but it is STILL not
packaged/accepted. All81 native source/header hashes independently matched.
Canonical new full build is needed to persist full prelink/link/package evidence;
critical builder/header-compile/PE-policy script hashes now also fail on concurrent
change and are retained separately. No actual BDS/OS policy was changed.

| User-visible area | Current verification boundary |
| --- | --- |
| Playtime/period rankings | Existing SQLite/JST/session UI retained; native closed-session lower-bound semantics retained |
| Activity/killed_by/projectile | New capture source + fixtures; actual gameplay still unverified |
| Rankings/personal GUI/API | SQL/paging fixtures; actual LL forms/other-Mod call still unverified |
| TPS/true MSPT | Earlier genuine host acceptance retained; no rerun or new DLL runtime claim |
| All9 categories/77 keys/3commands/4API/history/translations/Web | All required; per-key real-play/SQLite/UI acceptance NOT complete |

Details: `docs/KILLED_BY_ATTRIBUTION.md`, updated dashboard/read-safety/matrix
docs. Next external acceptance needs NEW normal-host world/unused ports and this
new candidate, not a previous smoke/MSPT world. Full parity remains IN_PROGRESS.

Recorded snapshot canonical build PASS (see FINAL AUDIT above for later source drift), no BDS launched:

- `.build/server-candidate/run-a2b17a0840b446e3b8a77d1df9cacdbe/`.
- DLL `15E4F5F03AB5D4F97E32621830ECBB304E29AACE73C7143677225DA49ACB6A05`.
- ZIP `F9800510DE8FA92B2C7BCCD014B37752A3C7C23BBC4BD6F80C079F550D1B486C`.
- Official22,202 headers /19 native units +core, prelink/link/PE/package PASS.
  `candidate-evidence.json`,81 source/header hashes and3 critical script hashes
  independently rechecked:0 changed. Earlier rejected artifacts NOT promoted.
  candidate_packaged=true, bds_load_verified=false, launch_authorized=false,
  matched_server_sdk_verified=false; no new runtime/actual-player acceptance.
- Read-only preservation audit: BDS748 start2026-11-05 01:38:04 unchanged;
  companion28268 start2026-11-06 06:30:51 unchanged. netstat independently shows
  127.0.0.1:18767 LISTENING28268. Get-NetTCPConnection returned no visible row,
  so it was not used as proof. Protected DLL hash remains
  B286F054EF1CCF9D90CABBBF3943543FEA2B6FF78805D2EB7F044FF89D6D8A00.
- Updated matrix killed_by/animals_bred/talked_to_villager declarations: candidate
  sources exist/MCAPI verified, not incorrectly 'API absent', but no real action
  PASS. Final inventory and NaN/Infinity rejection regression noted below.
- FINAL Python regression after strict NaN/Infinity rejection:108 PASS48.617s,
  normal inventory tests included; optimized inventory2 PASS0.005s. Tracked
  `git diff --check` PASS (not an untracked-file audit). Final logs
  `python-tests-final.log` / `inventory-optimized.log` in the log root above.
  Refreshed requirement inventory96 rows/zero accepted:
  `.build/parity-audit/run-1e743bb2e0354148b53af562a746b040/inventory.json`.
  Tests do NOT promote missing mandatory sources or real-play/GUI/API acceptance.

Exact commands used (cached environment paths above in this entry/history):

```powershell
scripts/verify_stats_readers.ps1 #3 C++ binaries; actual fixture writer/closed DB
scripts/verify_player_drop.ps1 #boundary helpers only
scripts/verify_interval_runtime_import.ps1 #updated12 valid/40 rejected
python -W error::ResourceWarning -m unittest discover -s tests -p 'test_*.py' -v
node tests/analytics_dashboard_ui_tests.cjs
scripts/build_server_candidate.ps1 #new unique artifact, no BDS
python scripts/audit_lk_parity.py
```

## 2026-11-07 — native reader/GUI build PASS; full-parity actual-player acceptance still pending

Read the NEW full-parity mandate first, then LK integration/latest STATUS and the
Command Code semantic handoff. Preserved the intervening user/coordinator code,
including block/craft/travel/damage/interaction/commands/C API and earlier real
MSPT acceptance. The prior 09C23056 candidate's empty normal-host smoke is NOT
proof of these NEW reader/GUI changes or of any real player action.
No protected BDS/world/DLL/SQLite/18767 changes, security/network/auth changes,
host connector retry/bypass, Git operations/push/release or unrelated edits.
No AGPL code/comments/translations copied; keys are the mandated public IDs and
new Japanese labels are independently authored.

Concrete changes:

- `core/closed_session_time.hpp` + `stats_reader.cpp`: removed indefinite open-
  session wall-clock accumulation. Closed/consistent integer sessions only;
  exclude negative/future/invalid rows; merge duplicate, nested and chained
  overlaps before summing, never count gaps. Since-death uses closed time after
  the last matching death, NOT elapsed offline wall time. Summary and per-key
  rankings use identical SQL. Both are explicitly **closed-session lower bounds
  in 50ms-converted ticks**, NOT true online server-tick/full-LK parity.
- Fixed `/stats` input alias: copy player_id before clearing result (the caller
  passes `summary.player_id` itself). Re-read display name instead of erasing it.
  Detect SQLite step errors, bound ranking limit/offset, saturate homogeneous
  totals. CUSTOM incompatible-unit totals are unavailable: C API `total:null`,
  GUI explicit explanation. No cm+damage+ticks+action score.
- Stopped promoting interaction pre-notifications into confirmed CUSTOM open/
  eat/play results, attack counts or pre-origin hurt amounts into confirmed
  CUSTOM damage_taken/dealt. Raw observation history remains intact; unsupported
  ranking keys return failure, not fictional successful statistics.
- `stats_command.cpp` + `stats_page.hpp`: own-ID/current-Player callbacks for
  previous/next personal and ranking pages, live re-read on click, separate
  failure form, CUSTOM77 key selector (12/page), category switches. No delayed
  old Player pointers. Personal reader still has a **200-type window**, rank
 10/page, max1000pages. This is NOT acceptance of complete unbounded pagination.
- Completed independently authored Japanese labels for all77 catalog keys and
  safe unknown-key fallback. Labels/enum/selector entries do NOT collect data.
- `scripts/audit_lk_parity.py` + tests: compare exact ordered77 requirement/catalog
  keys and full matrix coverage; emit96 individual gate rows (9 categories,
 77 CUSTOM,3commands,4API,3support). All actual-player/SQLite/GUI/API acceptance
  remains unverified/false unless separately proven. Matrix declarations are
  explicitly not independent API availability/registration/collection proof.
- Updated Japanese docs/matrix/old F9D1 host handoff; scoped self-review in
  `docs/NATIVE_STATS_READER_SAFETY.md` is NOT an independent security review.

Actual verification already completed (new test fixtures only):

```powershell
$env:BEDROCKINSIGHT_CLANGCL_ROOT='C:\Dev\LL2660Toolchain-20261029\clang+llvm-23.1.4-x86_64-pc-windows-msvc\bin'
$env:BEDROCKINSIGHT_SQLITE_ROOT='C:\Users\chihirosub\AppData\Local\.xmake\packages\s\sqlite3\3.54.0+100\b897e7007f3d49769c227e337e14fdef'
scripts/verify_stats_readers.ps1 #2 C++ fixture binaries PASS, all77 labels + aliased input + crash/open/overlap/gap/time rankings + C API null/paging
scripts/verify_player_drop.ps1 #observation predicates/exception preservation PASS, not engine proof
scripts/verify_interval_runtime_import.ps1 #9 valid/31 rejected PASS, not runtime proof
python -W error::ResourceWarning -m unittest discover -s tests -p 'test_*.py' -v #103 PASS44.200s
node tests/analytics_dashboard_ui_tests.cjs #4-category/auth/escaping/no-sample/stale/logout DOM PASS, NOT browser
python scripts/audit_lk_parity.py #96 rows, zero accepted
```

C++ reader/API evidence: `.build/stats-reader-tests/run-f37de285a37f474d86f3e85d34665434/`.
Observation/gate evidence: `.build/player-action-tests/run-23d1573334b0412192353a0c03fa4527/`.
Python/DOM evidence: `.build/player-action-tests/run-adf20cc43e9c4ef8a4b0add38a229469/`.
Initial inventory: `.build/parity-audit/run-d0b261657e654fc5b23d47fa75f66148/inventory.json`;
docs were subsequently clarified, so a refreshed input digest will be recorded below.

NEW changed-source full native candidate build completed via
`scripts/build_server_candidate.ps1` with cached toolchain/packages:17 native
units,22,202 verified official headers, core compile, prelink/link, PE/import
validation and package PASS. No BDS was launched/staged/loaded by this work.

First attempt `.build/server-candidate/run-e09ab5bddebc467497c0c4811a04acc2/`
compiled17 native units and linked a DLL, but correctly FAILED final PE policy:
the intervening durability source introduced actual compiler/prelink symbol
`?getDamageValue@ItemStackBase@@QEBAFXZ` and total26 delay APIs, whereas the old
reviewed list allowed25. Inspected the actual PE and verified pinned official
`MCAPI short ItemStackBase::getDamageValue() const` (header line186). Added ONLY
this exact name and the matching26-name bound, requiring reviewed hurt/weak APIs;
did not permit arbitrary delay imports or weaken runtime/security/auth. Extended
parser tests now10 valid/34 rejected PASS (wrong return type/constness and missing
hurt rejected). Build script now persists its PRE-build source/hash snapshot
even on later failure; this is evidence, not permission to skip final hashes.
The failed candidate is NOT packaged/accepted. The following complete fresh
build passed without bypassing the exact-name gate:

- Root `.build/server-candidate/run-f13f2046ad01465d9426d6582de8c656/`.
- DLL SHA256 `7EDC44BCA007464ACD1762D360525DBA1C58039C90417EF27EFD72CA4384E8D1`.
- ZIP SHA256 `5E6028245410E7FB2EACBF8890126F6BE73DBFE47EF8C411CB384711E69AF223`.
- `candidate-evidence.json` and `source-hashes-before.json`; independently
  rechecked all75 source/header records after build:0 changed.
- `candidate_packaged=true`, `bds_load_verified=false`,
  `matched_server_sdk_verified=false`, no normal release/runtime acceptance.
  The old09C empty smoke does not apply to this new binary.
- Refreshed96-row inventory (all accepted=false):
  `.build/parity-audit/run-7d6fc9332754414fb8daa0250d34ae6e/inventory.json`.
  Normal and `python -O` inventory tests2/2 PASS, including duplicate matrix
  rejection; logs `.build/parity-audit/run-c70ac8acfb9a419ca0d901e9b5ad171e/`.
  Existing `companion.action_acceptance` verifies only its listed partial action
  sources, NOT all9/77 or actual LegacyRemoteCall/gameplay. No blanket PASS.
- Final read-only preservation audit: protected BDS748 start2026-11-05 01:38:04,
  companion28268 start2026-11-06 06:30:51 unchanged;127.0.0.1:18767 still listening
  on28268. Original protected mod DLL SHA256 still
  `B286F054EF1CCF9D90CABBBF3943543FEA2B6FF78805D2EB7F044FF89D6D8A00`.

|Scope|Implementation/fixture|New-source DLL|Actual player / SQLite / in-game GUI / API|
|---|---|---|---|
|Stored playtime + since-death lower bounds|Reader/API C++ PASS|7EDC candidate compile/link PASS|NOT RUN; full tick equivalence NOT established|
|GUI paging/77-key selection|Code + pure page model/label tests PASS|7EDC compile PASS|NOT RUN, 200-type window remains|
|CUSTOM interaction/damage success|False promotion removed, true sources missing|7EDC compile PASS|NOT ACCEPTED|
|Nine categories + CUSTOM77 parity|Individual96-row gap inventory, no blanket PASS|Global build PASS, NOT per-key proof|NOT ACCEPTED; many absent/limited sources|
|LegacyRemoteCall wire compatibility|Independent C ABI only|7EDC exports linked|NOT VERIFIED with another Mod|
|MSPT / original four-category viewer|Preserved/regression PASS|Prior accepted baseline unchanged|09:18 actual MSPT remains accepted, not repeated|

Mandatory remaining work includes killed_by ENTITY attribution (cause codes are
not entity types), craft commit, complete durability breaks, teleport/dimension/
external-motion exclusions, remaining special keys, complete paging/compatibility
and per-key/category real-player → SQLite → GUI/API proof through the external
authorized normal-host route. No offline-sandbox online-mode BDS retry. The
entire user mandate is NOT complete; PROJECT_STATUS remains IN_PROGRESS.

## 2026-11-07 JST — UPSTREAM-MAPPED PARITY PASS: real sleep, sneak duration, real damage-dealt, durability break, key-scoped CUSTOM ranking

Read `LK_STATS_UPSTREAM_HOOK_PORT_MAP_20261107.md` first and implemented against its
Hook/Handler semantics (reference only, no AGPL copy). Live BDS PID748 / companion 18767 /
other projects untouched; no push/commit/auth changes. PROJECT_STATUS remains IN_PROGRESS.

### Implemented in code (new, not doc-only)
- **Sleep success**: upstream `Player::$startSleepInBed` is not exported by LL/prelink, so I
  used `Player::isSleeping()` (virtual, no import) polled on `ServerLevelTickEvent` and count
  only false→true transitions (`player_sleep_result`, cause `ll26.60.sleep_flag_transition.v1`).
  The old bed right-click interaction key was removed so attempts are no longer counted.
- **Accurate sneak time**: `PlayerSneakingEvent`→`PlayerSneakedEvent` wall-clock interval
  converted to ticks (`player_sneak_time`, `ll26.60.sneak_duration.v1`), finalized on
  disconnect/shutdown. Distinct from the 250 ms flag-observation counts.
- **Real damage dealt (fixed)**: removed `PlayerAttackEvent`-as-damage entirely. `ActorHurtEvent`
  now yields the real pre-origin amount for `damage_taken`, and `damage_dealt` only when the
  damaging entity's `ActorUniqueID.rawID` matches a tracked online player (public
  `Actor::mLegacyUniqueID`, virtual `getDamagingEntityUniqueID`). Cause is
  `ll26.60.actor_hurt_real_damage.v1`.
- **Broken (fixed)**: `hurtAndBreak` now requires `Item::isDamageable()` + `max_damage>1` (virtual
  vtable reads, no import) and same-slot 1→0, emitting `item_broken_result`
  (`ll26.60.durability_item_destroyed.v1`) with max_damage/durability_path details. The old
  slot-loss cause no longer feeds `broken`.
- **Craft/trade separation guard**: `CraftHandlerBase::postRequest` now reads `mCraftActionType`;
  `CraftRepairAndDisenchant` (anvil repair/enchant) is recorded as `ll26.60.anvil_result.v1` and
  excluded from `crafted`. Trade remains BLOCKED (no exported Trade-container discriminator).
- **CUSTOM ranking fixed**: `StatsReader::ranking` requires an explicit `custom:<key>`; bare
  `custom` is refused in `/stats rank`, the C API and tests. Each supported key has its own
  fixed SQL; incompatible units are never summed.
- **play_time** now includes the active session (`now - started_at_ms`) with bounds; new
  `sleep_in_bed`/`sneak_time` custom keys; analytics CTE/metrics for `damage_dealt_tenths`,
  `broken_items`, `sleep_successes`, `sneak_time_ticks`; dashboard fields updated.

### BLOCKED with grounded symbols (documented in the matrix, never claimed)
`animals_bred` (BreedableComponent::mate, ActorUniqueID::INVALID_ID MCNAPI), `fish_caught`
(BucketItem::$_useOn), `talked_to_villager` (ServerPlayer::$openTrading), `traded_with_villager`
(_handleTransfer Trade container), `damage_blocked_by_shield` (Player::$_hurt), absorption/resistance
(SharedAttributes::ABSORPTION MCNAPI), raids/target_hit/time_since_rest/enchant split.

### Evidence actually run
| Check | Result |
| --- | --- |
| Pinned-26.60 compile of runtime, event, storage, player_damage, player_hurt, player_craft, travel, interact, stats_command, stats_reader, remote_api | PASS |
| Fake-SDK runtime compile (`/DBEDROCKINSIGHT_FAKE_SDK`) | PASS |
| Full candidate build + prelink/link/PE import validation | PASS: `.build/server-candidate/run-f13c762e6ac449898744c27475e3f7e8`, DLL `118F032C91103F657F6B9C2A9D209AA67E252F1C0C36A64024474C1A4A99258B`, ZIP `D3E175A83C5785AD8484F3BB7D4A8316659A9061792BA5DFEED37D4B7162ACDB` |
| `parity_observation_tests` | PASS |
| `stats_reader_tests` (sleep/sneak/broken/custom-key ranking) | PASS |
| `remote_api_tests` (`custom:<key>` required) | PASS |
| Python suite | 101 PASS |
| Fresh isolated BDS smoke (empty allowlist) | PASS: `.build/isolated-bds/run-fb9449daa4824f3f917c2746862a7b89`, ports 59900/59901, loaded/enabled, DB created, exit 0 |
| Real player action → SQLite / command output / GetProcAddress-in-BDS | **NOT RUN** |

Matrix: CODE_ONLY 54 / UNIT_ONLY 5 / BLOCKED 14 / MISSING 4 → **not full parity**; the blocked
keys require APIs absent from official LL26.60/prelink. No live acceptance claimed.

## 2026-11-07 04:03 JST — NEW FULL-PARITY CANDIDATE ISOLATED BDS SMOKE PASS

- DLL build and package PASS: `.build/server-candidate/run-133311d9ca054d29972d8f8b432f058b/BedrockInsight.dll`, SHA256 `44757AE9D910F40B6BBA4F5318B8EFE9103F103BD8B1CF251236E277046A3797`. This candidate has compiled travel, craft, damage, block-interaction, in-game stats command, read-only reader, four C API exports, plus prior features.
- Verified candidate-specific fresh BDS test: `.build/isolated-bds/run-984db171effc4534bd38977e420bf933/capture-runtime-evidence.json` at isolated ports 59760/59761. `native_loaded=true`, `native_enabled=true`, `server_started=true`, `database_exists=true`, graceful stop issued, `exit_code=0`, `forced_termination=false`, no error; `ISOLATED CAPTURE LOAD PASS`, `FULL_PARITY_BDS_SMOKE_PASS`. Existing live BDS PID748 untouched. Source: exact test `.build/host-full-parity-smoke-20261107.ps1`.
- IMPORTANT NOT YET VERIFIED: genuine player action → new event → SQLite → in-game form or C API. `event_to_db_verified=false`, `matched_server_sdk_verified=false`, `normal_release_evidence=false`. Therefore **NOT LK-Stats full parity**. No need to rerun the identical baseline smoke; prioritize real player action tracing and missing keys/semantic correctness instead.
- Active DeepSeek Command Code continuation logs at `logs/commandcode-deepseek-full-parity-20261107-b.jsonl` (was active at 04:03 JST). Read `COMMAND_CODE_FULL_PARITY_CONTINUE_20261107.md` for semantic blockers: combat attack count is NOT damage dealt, interaction-before events do NOT prove success, item slot loss is NOT item broken, CUSTOM ranking must not sum unlike units. Do not claim those are true stats. Finish user-mandated complete parity in further code runs.

### Follow-up (same candidate hash) — build/test gates re-verified and one real fix
- Rebuilt from source: same DLL SHA256 `44757AE9...` and ZIP `EEC7F862...` (deterministic), full
  prelink/link/PE import validation PASS.
- **Real fix:** `?$checkMovementStats@Player@@...` is not exported by the pinned prelink runtime,
  so the original travel hook could not link. Travel is now a `ServerLevelTickEvent` position-delta
  poll (`ll26.60.poll_position_delta.v1`, 5 s coalescing) using only resolvable accessors; the
  command/form BDS delay-imports were added to the reviewed allowlist (25 names, compiler-derived).
- New PASS tests: `tests/stats_reader_tests.cpp` (player lookup, category totals, custom
  `walk_one_cm/open_chest/damage_taken/drop`, derived `play_time`, ranking/paging) and
  `tests/remote_api_tests.cpp` (all four exported C functions, JSON, paging, rejection codes,
  no raw id). `stats_reader` custom now also emits `minecraft:drop` and `minecraft:play_time`.
- Independent fresh isolated BDS smoke: `.build/isolated-bds/run-034b9ababbec4218986d4b5f57736588`
  (ports 59860/59861, empty allowlist): native_loaded/enabled, DB created, exit0. Still no real
  player action → SQLite proof, no command execution proof, `event_to_db_verified=false`.
- **Additional real CUSTOM counters:** refined noteblock/cauldron interactions by main-hand item
  (resolvable `getInventory`/`getSelectedItemSlot`), adding `tune_noteblock`, `fill_cauldron`,
  `clean_armor`, `clean_banner`, `clean_shulker_box` as documented heuristics (not success proof).
  Rebuilt: `.build/server-candidate/run-36e0625a54d44f69bc616b1952e4bc54`, DLL
  `BA82C6C991147E5BDF08B507010851E6CAC4BA361755901254EA12A221BD12CF`, ZIP
  `015061E997C6B082D4FB77A08CBC0B231CD165E04202AC8EA17DA3943C67D0B5`, link/PE PASS.
  Fresh smoke `.build/isolated-bds/run-c2d6521f218f4ba3a919d0a6313dfe67` (ports 59870/59871):
  loaded/enabled, DB created, exit0. Matrix now CODE_ONLY 38 / UNIT_ONLY 5 / MISSING 33 / PARTIAL 1.
- **time_since_death** added as a read-time value (ticks since last confirmed death, else stored
  play_time); StatsReader test asserts it. Final rebuild
  `.build/server-candidate/run-e460b868afe240f7bb2d851c7c5a67f5`, DLL
  `09C23056B07B639294FEC72E6DDF47E1590F1384842B559408D4DE167769F319`, ZIP
  `0C02CF93EC87DF187E755BE2C0A2E8EC5B2F3D4355FB75316538F391E8732A31`, link/PE PASS. Final
  fresh smoke `.build/isolated-bds/run-3447fa95c39248b7ad17d1cc7aad9bd9` (ports 59880/59881):
  loaded/enabled, DB created, exit0. Matrix: CODE_ONLY 39 / UNIT_ONLY 5 / MISSING 32 / PARTIAL 1.
  All four C++ test binaries + 100 Python tests + node DOM PASS on this source state.


## 2026-11-07 JST — NEW GAP CODE: craft completion, travel distances, interactions, damage, /stats GUI, 4-function C API

Implemented new capabilities, then compiled them against the pinned 26.60 headers
(no BDS launched, no live server touched, no git add/commit/push, no other repo edit).

### New native adapters (files)
- `levilamina/player_craft.{hpp,cpp}` — `CraftHandlerBase::postRequest(bool)` Lowest hook.
  Only a success report with typed `ItemStackRequestActionCraftHandler::mCraftResults` entries
  yields `item_craft_result`; quantities are `mCount × multiplier` bounded 1..4096. Failed or
  empty results are dropped, so attempts are never counted as crafts.
- `levilamina/player_travel.{hpp,cpp}` — `ServerLevelTickEvent` position-delta poll (the
  `$checkMovementStats` symbol is not exported by the pinned runtime). Per-tick delta becomes cm
  by mode (walk/sprint/crouch/swim/fly/aviate/fall/boat/minecart/pig/strider/horse/nautilus/
  happy_ghast/ride) using `isSprinting/isSneaking/isSwimming/isGliding/isFlying/isInWater/
  isOnGround/getVehicle`; coalesced every 5 s; non-finite/absurd deltas rejected.
- `levilamina/player_interact.{hpp,cpp}` — `PlayerInteractBlockEvent` → block type re-read from
  the player's block source, mapped to the CUSTOM interaction keys (chests, barrels, shulkers,
  workstations, anvil, beacon, brewing, campfire, dispenser/dropper/hopper, noteblock, bell,
  flower pot, cake, cauldron, jukebox, trapped chest, bed). Unmapped blocks are not recorded.
- `levilamina/player_damage.{hpp,cpp}` — `ActorHurtEvent` (player victim) records real damage
  in tenths + cause code; `PlayerAttackEvent` records deal *actions* only, because the event
  does not expose an amount (`damage_amount_measured:false` in details).
- `core/{travel,interaction,damage,craft}_observation.hpp` + `core/stats_catalog.hpp` — pure,
  unit-testable logic, the nine categories and all 77 CUSTOM keys with Japanese labels and a
  safe key fallback.
- `levilamina/stats_command.{hpp,cpp}` — `/stats gui [category]`, `/stats rank <category> [page]`,
  `/stats player <name> <category>` via `ll::command` + `SimpleForm`, permission `Any`,
  9 category selector buttons, 10/page bounded ranking.
- `core/stats_reader.{hpp,cpp}` — read-only SQLite queries per category (fixed SQL, bound
  identifiers, no user SQL), player lookup by id or exact name, bounded ranking.
- `core/remote_api.{hpp,cpp}` — exported C ABI `getPlayerStats`, `getRankStats`,
  `getPlayerStatsPage`, `getRankStatsPage` returning JSON into a caller buffer (independent
  equivalent of LK RemoteCall; no AGPL/LIP dependency).
- `core/event.{hpp,cpp}` append `ItemCraftResult`, `PlayerTravelDistance`,
  `PlayerInteractionResult`, `PlayerDamageTaken`, `PlayerDamageDealt` (no ordinal changes).
- `core/storage.cpp` maps the new kinds into `player_daily_types` (`items_crafted`, `travel_cm`,
  `interactions`, `damage_*_tenths`) and increments legacy daily meters from travel cm.
- `levilamina/runtime.{hpp,cpp}` owns travel/craft tokens, three new event listeners, registers
  the commands after admission, and publishes the DB path to the C API.
- `companion/analytics.py` adds `crafted_items/craft_actions/travel_cm/interactions/
  damage_taken_tenths/damage_dealt_actions` CTEs, rows, totals, rankings, typed breakdowns and
  definitions; `companion/web/actions.{html,js}` add the Japanese panels + ranking options.
- Build lists: `xmake.lua`, `scripts/build_server_candidate.ps1`,
  `scripts/verify_pinned_sdk_headers.ps1`; delay-import allowlist extended with exactly
  `?$checkMovementStats@Player@@QEAAXAEBVVec3@@@Z`, `?isFlying@Player@@QEBA_NXZ`,
  `?isInWater@Actor@@QEBA_NXZ`, `?postRequest@CraftHandlerBase@@QEAAX_N@Z` (mangled names taken
  from `llvm-nm` on the compiled objects, not guessed).

### Evidence actually run this turn
| Check | Result |
| --- | --- |
| Pinned-26.60 clang-cl compile of the 7 new sources (travel/interact/damage/craft/stats_command/stats_reader/remote_api) | PASS (all .obj built) |
| Compile of changed `runtime.cpp`, `event.cpp`, `storage.cpp` | PASS |
| `tests/parity_observation_tests.cpp` (new) | PASS: travel modes/distances, interaction mapping, damage tenths, craft bounds, 9 categories, 77 unique keys |
| Full Python suite | 100 PASS (95 existing + 5 new `test_parity_activity.py`) |
| `node --check companion/web/actions.js` + authenticated HTTP→Japanese DOM test | PASS |
| Real player / isolated BDS / command execution / GetProcAddress | **NOT RUN** |

### Honest limits (not full parity)
`docs/LK_FULL_PARITY_MATRIX.md` enumerates all 9 categories and all 77 keys. Counts there:
CODE_ONLY 31, UNIT_ONLY 7, MISSING 38, PARTIAL 1. No real player exercised the new hooks; the
DLL was not linked/staged/loaded this turn. Craft commit and NBT are unverified. Damage dealt is
action-count only. 38 CUSTOM keys (breeding, cleaning, fishing, raids, villager trade, time-based
stats, water-surface walking, target hit, shield/absorption/resistance) remain MISSING rather
than being claimed. `minecraft:broken` remains slot-loss evidence, not a confirmed durability
break. Existing live BDS/18767, security rules, old SQLite data and schema v4 were untouched.

## 2026-11-07 02:54 JST — HOST VERIFIED DeepSeek BlockMined/BlockPlaced new native DLL build + isolated BDS load (NEW TOP)

Command Code v1.88.3, authenticated, selected `deepseek/deepseek-v4.1-flash`, actually implemented the player-block post-outcome hook, append-only event kinds, sqlite-backed Japanese analytics/UI, native/fixture tests; detailed scope/evidence in the following DeepSeek checkpoint. Initial Command Code run was capped at45 turns; second `--continue --print` session exited0 after75-turn allowance, with source and test modifications present. Runtime PASS is **load/enable only** on new candidate; NO REAL PLAYER placed/mined a block.

Coordinator's independent normal-host full native candidate build after DeepSeek's checkpoint:
- Script: `.build/host-drop-build-20261106.ps1` invoking updated official `scripts/build_server_candidate.ps1`. Build completed exit0, 22,202 pinned LL26.60 header blobs verified, all native source units compiled, prelink, link, PE/package inspection PASS. Target root `.build/server-candidate/run-4cc81659b00d4899802781af03d63eb5`; DLL SHA256 `5BFA8709479723C40BCF74A1315A15385C0257BDD1DCB5BD2213EBD8D659E4B9`; ZIP SHA256 `702E653C825AFB18004F0DBB0558D595C1A6241F5AB7077CD303BC89E52D1AD2`; `candidate-evidence.json`. This is NOT full matched-SDK/release certification.
- Fresh isolated scratch BDS `.build/isolated-bds/run-440d76766e994de6872bcad67750b013`, ports 59842/59843, online-mode preserved, empty allowlist, no real players, original user BDS untouched. Fresh staging/prepare and isolation preflight PASS, script `.build/host-block-smoke-20261107.ps1`. Actual `scripts/run_isolated_capture.ps1` PASS: native_loaded=true, native_enabled=true, server_started=true, database_exists=true, stop_reason=ready_hold_completed, exit_code=0, stop_command_issued=true, forced_termination=false, service_error=false. See `capture-runtime-evidence.json`. Test process PID37324 was stopped normally; protected existing BDS PID748 was never stopped/reconfigured. Do not reuse this scratch world.
- Independent new-block Python tests 4/4 PASS; DeepSeek complete Python suite 95 PASS, Fake SDK native runtime PASS. These are not real mined/placed events; `event_to_db_verified=false` for new block stats. Current user-requested LK-Stats full parity (all nine categories, CUSTOM77, commands/GUI and four RemoteCall interfaces) remains far from complete. Next prioritize one REAL authorized player's successful mining and placement in another fresh isolated BDS + closed SQLite/API/JS; then craft completion, movement, damage, special actions, in-game GUI and RemoteCall. Do not claim completion yet.
- No Git add/commit/push/release, no existing companion18767 restart or live DLL install, no security/firewall/auth changes, no other projects altered.

## 2026-11-07 JST — IMPLEMENTED: proven BlockMined/BlockPlaced success by block type (+SQLite/API/Japanese UI/tests)

Continued the Command Code/DeepSeek implementation handoff (COMMAND_CODE_DEEPSEEK_TASK.md).
Real code, not a planning checkpoint. No BDS was started/stopped/modified, no live DLL or
world touched, no firewall/auth/network change, no Codex supervisor/other repo edit, no
git add/commit/push/release. Existing single writer, schema4, auth and 18767 viewer unchanged.

What now exists (edited files):

- `src/bedrock_insight/levilamina/player_block.{hpp,cpp}` (new): Lowest hooks on the pinned
  26.60 `GameMode::$destroyBlock(BlockPos const&, uchar)` and
  `GameMode::$buildBlock(BlockPos const&, uchar, HandSlot, bool)` virtual thunks (the `$`
  prefix is required by `ll/api/memory/Hook.h`). Entry copies typed `GameMode::mPlayer`
  (`Player&`) after LL `Player::tryGetFromEntity` identity check, UUID/name, dimension, pos
  bounds, and the entry block type via `Player::getDimensionBlockSource().getBlock(pos).
  getTypeName()` (official virtual + inline accessor; no offsets/guessed slots/packet parsing).
  origin runs once; after a true return the same weak-resolved nonremoved Player, same
  dimension and same position are re-read. Mined is recorded only if entry≠air and the type
  changed; placed only if post≠air and differs from entry. Nested/cancelled/exception/
  vanished/ambiguous are excluded.
- `src/bedrock_insight/core/block_observation.hpp` (new): `block_break_confirmed`,
  `placed_block_type`, and `observe_block_outcome` (origin-once, bool/exception preserving).
- `src/bedrock_insight/core/event.{hpp,cpp}`: appended `BlockMinedResult`/`BlockPlacedResult`
  -> `block_mined_result`/`block_placed_result`; no ordinal or legacy kind changed.
- `src/bedrock_insight/levilamina/runtime.{hpp,cpp}`: new `BlockToken` started after hurt and
  drained/stopped with the same generation/pass-through/single-writer discipline.
- `companion/block_source.py` (new) + `companion/analytics.py`: `bi_block_details` UDF,
  `mined`/`placed` CTEs, metrics `mined_blocks`, `mined_block_actions`, `placed_blocks`,
  `placed_block_actions`, type breakdowns `blocks_mined`/`blocks_placed`, JST day/month/all,
  selected player, null-not-zero for unrecorded periods. Attempt kinds/causes still excluded.
- `companion/web/actions.{html,js}`: two new Japanese panels + profile fields + metrics.
- `scripts/verify_pinned_sdk_headers.ps1`, `scripts/build_server_candidate.ps1`, `xmake.lua`:
  add `player_block.cpp` to compile/object/source lists.
- `scripts/validate_server_release.ps1`: delay-import allowlist expanded to 14 and exact
  `$destroyBlock`/`$buildBlock`/`getDimensionBlockSource` names added, with a paired-set check.
- `tests/block_observation_tests.cpp` (new), `tests/test_block_activity.py` (new, 4 tests),
  `tests/action_dashboard_ui_tests.cjs` extended, `tests/fake_player_block.cpp` (new),
  `tests/native_runtime_tests.cpp` + `scripts/verify_native_runtime.ps1` wired.

Evidence actually run this turn:

| Check | Result |
| --- | --- |
| Compile `player_block.cpp`, `runtime.cpp`, `event.cpp` with cached pinned-26.60 clang-cl options | PASS (objects only, no link/BDS) |
| Prelink with the new object; generated `bedrock_runtime_api.lib` | PASS; resolves `?$destroyBlock@GameMode...`, `?$buildBlock@GameMode...`, `?getDimensionBlockSource@Actor...` |
| `tests/block_observation_tests.cpp` | PASS (state-transition gates, air/replace rules, origin-once) |
| `python -m unittest discover -s tests -p 'test_*.py'` | 95 PASS (new 4 block tests; real HTTP401/200 -> Japanese DOM) |
| Fake-SDK `scripts/verify_native_runtime.ps1` | PASS (no BDS) |
| `node --check` + original dashboard DOM test | PASS |

Honest limits (must not be read as full parity): real-player BDS runtime capture of these
hooks is UNVERIFIED; a new complete DLL candidate was NOT built/staged; creative-instant and
other paths that bypass `GameMode::destroyBlock` may be missed; multi-block placements count
the primary position only; drop items/NBT/material counts are not included; no world-save or
all-path total is claimed. LK-Stats full parity (all 77 CUSTOM keys, craft/travel/damage,
in-game `/stats` GUI, 4 RemoteCall APIs) remains IN_PROGRESS.

## 2026-11-06 — USER REQUIREMENT: LK-Stats v0.13.0 FULL PARITY IS THE MINIMUM (SCOPE CHANGE; NOT CODE PASS)

The user explicitly clarified: **ALL LK-Stats features are required**, not just selected action adapters. Read `LK_STATS_FULL_PARITY_REQUIREMENTS.md` FIRST (new exhaustive minimum: 9 metric categories, 77 upstream CustomType names, all three /stats in-game GUI commands, all four RemoteCall APIs/pagination, persistence/localization/SQLite+Japanese dashboard, per-key actual-game verification matrix). `LK_STATS_INTEGRATION_TASK.md` and `AGENTS.md` now state that these requirements are mandatory. This supersedes previous narrow-scope optional-in-game-UI language; all 77 and 9 types must be accounted for. Two CUSTOM names were struck through in upstream README yet declared in code; verify genuine upstream implementation and document exceptions without inventing successful collection. BlockPlaced is an additional user-requested feature beyond LK's named 9 categories.

NO NEW NATIVE CODE, BUILD OR PLAYER RUNTIME VERIFICATION is claimed in this scope-change checkpoint. Below 18:52 entry remains the last verified development status, including the prior F9D1 native candidate and 85 Python fixture tests. Major missing features include real mined success/typed counts, craft completion, movement by mode, custom interactions, complete combat/damage attribution, all in-game GUI/RemoteCall equivalence and actual gameplay acceptance. Preserve original BDS / 18767 and no GitHub push/release.

## 2026-11-06 18:52 JST — LK acceptance tooling + concrete craft API boundary; no new native success claim

Continued from15:17. Read latest STATUS/AGENTS, full LK task, pinned official
26.60 inventory/craft declarations and exact AGPL5bb8bfb transfer reference.
No AGPL code/comments/translations/artifacts copied. MSPT09:18 acceptance is NOT
repeated. No candidate rebuild, stage, BDS launch, old-world reuse, host-connector
retry,18767 restart, security/network/auth change, Git add/commit/push or release.
This turn does NOT implement a completed-craft counter or establish new real
player capture. F9D1 native code/package remains unchanged and untested in BDS.

Concrete craft findings, not a blanket missing-SDK claim:

- Official _handleTransfer returns ItemStackNetResult, MCAPI; individual transfer
  Success0 is not full request commit. Enum separately includes end failure4 and
  commit failure5. Do not count CreatedOutput transfer as completed craft.
- CraftHandlerBase::postRequest(bool wasSuccess) IS MCAPI. A success-report path
  exists, but its producer ordering and association with one request's quantities
  and final inventory commit are NOT established by this declaration.
- Craft handler endRequest and ContainerScreenValidation.tryCommitActionResults /
  _commit remain MCNAPI. No guessed symbols, virtual slots or unchecked hooks.
  _getOrInitSparseContainer may initialize/mutate; not called just to observe.
- Independent tests/craft_api_compile_probe.cpp uses official member-pointer
  types/static_asserts and retains actual compiler relocations for three MCAPIs.
  scripts/verify_craft_api_probe.ps1 reuses VERIFIED cached header compile options,
  compiles ONE object, no linking, prelink, hook installation or game execution.
  Probe PASS at `.build/craft-api-probe/run-5db2a66250e54fa788dfced92a87ac4a/`.
  Actual imports: `?_handleTransfer@ItemStackRequestActionHandler@@QEAA?AW4ItemStackNetResult@@AEBVItemStackRequestActionTransferBase@@_N11@Z`,
  `?postRequest@CraftHandlerBase@@QEAAX_N@Z`,
  `?getScreenContext@ItemStackRequestActionHandler@@QEBAAEBVContainerScreenContext@@XZ`.
  None added to candidate's runtime whitelist. See docs/CRAFT_API_FINDINGS.md.

Fixed a real acceptance-workflow defect: run_isolated_capture already authorizes
30..3600 seconds and host docs use600, but verify_real_player_capture rejected
>300. New shared Test-BdsRealPlayerWindow aligns ONLY the post-stop verifier with
the existing runner bound. Strict int/long, not bool/string/float. Runner timeout,
allowlist, online-mode, stop policy and execution authority are NOT expanded.
PS tests cover5 valid/8 invalid windows plus20 existing negative native-pair cases
and assert runner's declared bound cannot silently drift. No BDS was run to test it.

Added companion.action_acceptance and tests/native_actions_ui_check.cjs:

- CLI accepts only existing private real-player-evidence report and its closed
  snapshot, rejects protected original world, traversal/reparse/outside paths,
  oversized evidence/DB and WAL/SHM/journal. Original BDS DB is NEVER opened.
- Requires actual normal-stop/native-enable/preflight flags, same one-real-player
  access/identity/candidate, native closed session/events, schema4/integrity/FK,
  runtime/staging/stdout/DLL/copy hashes; verifies unchanged inputs afterward.
- Operator observations MUST be supplied separately, explicit supported positive
  counts only. Empty/zero/unsupported/attempt-as-success cannot earn acceptance.
  Independent raw arithmetic validates exact source/kind/actor/amount/details,
  real session bounds, unique activation and durable start/end markers. Native
  flags/true-return/quantity scopes are not promoted into world/craft completion.
- New own ephemeral loopback viewer only, anon401/auth200, opaque selected Player,
  every requested positive ranking; all-time AND each observed JST day/month
  computed separately from raw rows. Missing per-period observations remain null.
- Actual HTTP HTML/JS hashes match evaluated assets. Node renders real/actions
  code, compares Japanese quantities/types/hand/cause/mode and period/Player query,
  forbids innerHTML, tests logout. Not hard-coded to the old64-item fixture.
  Real viewer token remains in memory and is NOT sent to Node; no raw identity,
  names, tokens or subprocess diagnostics in result. Own listener is closed.
- Output explicitly retains independent_gameplay_attestation=false and
  browser_verified=false; only matched_metrics are checked, all others are
  NOT ACCEPTED. Edited evidence/fixtures cannot independently prove gameplay.
  This new tool has NOT been run on real new action evidence; fixture PASS below
  is checker correctness only, not event-to-DB/host acceptance.

Actual commands (cached clang-cl root unchanged):

```powershell
scripts/verify_craft_api_probe.ps1 -HeaderEvidence .build/server-candidate/run-d0b293d34d2f42c4bb7e918f9bab131e/header-compile-evidence.json
scripts/verify_real_player_capture_policy.ps1 #PASS20 negative pairs +5 valid/8 invalid windows
python -W error::ResourceWarning -m unittest discover -s tests -p 'test_*.py' -v #85 PASS27.675s
python -O -W error::ResourceWarning -m unittest discover -s tests -p 'test_action_acceptance.py' -v #10 PASS1.528s
node tests/analytics_dashboard_ui_tests.cjs #original4-category/auth/no-sample/stale DOM PASS
node --check tests/native_actions_ui_check.cjs #PASS
python -m companion.action_acceptance --help #earlier CLI smoke PASS, no host data loaded
```

Final85-test/optimized/DOM logs:
`.build/player-action-tests/run-9dc9a81d10ef4f4a9698125034068c3d/`.
Initial traversal rejection test surfaced FileNotFoundError before path-normalization
rejection; now '..' is rejected BEFORE filesystem traversal. Subsequent strict and
optimized tests pass. Tests cover actual fixture HTTP/DOM on7 items, no writes/
PII/token output, forged/failed/empty smoke evidence, source/detail/actor/time/
activation/normal-stop failures, observations/API/session tampering, CLI refusing
fixture as host report, JST day+month boundary plans and real three-period HTTP.

18:48 read-only preservation audit: protected BDS748 start2026-11-05 01:38:04 and
companion28268 start2026-11-06 06:30:51 unchanged; netstat127.0.0.1:18767 listening
28268. Protected original mods DLL SHA256 still B286F054...8A00. All48 F9D1 native
source/header hashes independently MATCH. No need to rebuild successful native
candidate; its old build proof remains qualified (not full SDK/runtime proof).

|Feature|Native source / implementation|Compile|Synthetic SQL/API/JS|New candidate enable / actual actions|
|---|---|---|---|---|
|Playtime/session/block attempts|Existing preserved|F9D1 historical PASS|Regression PASS|Original Rome136093ms baseline, no new claim|
|MSPT/effective TPS|Existing preserved|F9D1 historical PASS|Regression PASS|Genuinely accepted218A09:18, not repeated|
|Drop true result|Implemented|F9D1 unchanged PASS|PASS + raw/API/JS checker|NOT RUN; old95FE empty smoke is not action proof|
|Death flags/cause|Implemented|F9D1 unchanged PASS|PASS + strict source checker|NOT RUN / UNVERIFIED|
|Sprint/sneak flags|Implemented, coalesced|F9D1 unchanged PASS|PASS + strict source checker|NOT RUN / UNVERIFIED|
|Pickup inventory gain|Implemented, bounded|F9D1 unchanged PASS|PASS + strict source checker|NOT RUN / UNVERIFIED|
|Jump upward impulse|Implemented, not full jump|F9D1 unchanged PASS|PASS + strict source checker|NOT RUN / UNVERIFIED|
|Direct-source dead transition|Implemented, not all kills|F9D1 unchanged PASS|PASS + strict source checker|NOT RUN / UNVERIFIED|
|Use true response/inventory loss|Implemented, not consumption|F9D1 unchanged PASS|PASS + strict source checker|NOT RUN / UNVERIFIED|
|Craft completion|Three MCAPIs found; commit ordering/quantity scope pending|Probe object ONLY PASS, NOT shipped|Unavailable; unsupported metric rejected|NOT IMPLEMENTED|
|Travel/break/complete consumption|No new safe verified adapter this turn|—|Unavailable|NOT IMPLEMENTED|

Updated docs/ANALYTICS_DASHBOARD.md, PLAYER_ACTIONS.md, scoped SECURITY_REVIEW.md,
PLAYER_ACTION_HOST_VALIDATION.md and new CRAFT_API_FINDINGS.md. Host handoff now
includes independently recorded observations plus private-copy/API/Japanese JS
consistency command. Authorized external normal-host route + actual participant
is still needed for real new actions, not the offline Codex sandbox or a repeated
empty smoke. Whole request and PROJECT_STATUS remain IN_PROGRESS.

## 2026-11-06 15:17 JST — LK C6: use true-return observations, not consumption/completed-use claims

Continued after14:54 direct-attack checkpoint. Latest AGENTS/STATUS, full LK task,
official26.60 declarations and PlayerUseItemEvent publisher reviewed. MSPT09:18
acceptance remains valid historical baseline; no rerun/resolver rework. No BDS
staging/launch, used-world reuse, host/approval retry,18767 restart, security/auth
change, Git add/commit/push or release. AGPL5bb8bfb reference facts only; no code,
comments/translations/artifacts copied. Original four categories/static assets,
schema4/migrations/single writer preserved. New actions are NOT deployed to18767.

Independent player_use.cpp/.hpp + use_observation.hpp wrap official MCAPI
GameMode::$useItem(ItemStack&,HandSlot) at Lowest. Exact typed GameMode::mPlayer
reference (NOT unknown PlayerTransactionSubject storage), checked Player/UUID,
entry item namespace/count and bounded main-inventory type-total snapshots.
Only normal true return plus same weak-resolved nonremoved Player, same inventory
identity and1..128 unchanged slots is recorded. Old GameMode/Player/ItemStack
never dereferenced after origin. Nested calls invalidate inner+outer, origin once,
bool/engine exception preserved, observer exceptions suppressed. Pin/shared token/
generation/disable pass-through and failure cleanup retained. Two bounded scans,
no NBT/item writes, no SQLite in hook; existing bounded single-writer queue only.

Event ItemUseResult/item_use_result is append-only, not legacy ItemUse:
cause=ll26.60.gamemode_use_return.v1,amount=1,actor=copied Player UUID/name,
primary=ENTRY type,secondary=main_hand|off_hand (pinned0/1),post UTC time and
activation ID. Strict details hook=GameMode::$useItem,
phase=return_true_inventory_snapshot,scope=main_inventory_type_total,
entry_units/slots/before_units/after_units/loss_units; nested=false,
effect_completed_verified=false,consumption_verified=false. Loss=max(0,before-after)
and <=entry count; excessive/negative/out-of-bounds/changed-container cases omit.
No positive loss is NOT promoted into consumption or fabricated coverage zero.

Scope is TRUE RESPONSE plus valid main-inventory snapshots, NOT finished effect,
successful consumption, all uses, durable result or causal inventory mutation.
Other Mods can change counts; offhand/equipment counts are outside measured main
inventory. False/cancelled/pre-notification/unknown source are not success.
Player::$useItem is void; consumeItem argument is not proof. ItemStackBase
hurtAndBreak return alone and old post-call stack pointer are not used as a break
counter. Craft, authoritative filtered travel, actual break/consumption/completion,
indirect/projectile kill attribution and verified block outcomes remain unfinished.

New strict bounded JSON UDF/source/type/hand/amount/player mapping/time predicates
feed SQL use_return_actions and use_inventory_loss, selected Player, positive JST
day/month/all ranks, types/hand top100 and first/last/scope/source/definition.
Japanese/actions clearly separates attempts/true response/net decrease/completion;
missing/nonpositive collection stays null. TextContent/opaque selection, loopback,
auth/Host/Origin/read-only unchanged. New separately authorized process only:
127.0.0.1:18768/actions with own generated token, not a currently running new URL.

Actual cached-toolchain commands/results (same env roots as preceding checkpoint):

```powershell
scripts/verify_player_drop.ps1 #all previous + use delta/bounds/zero/increase/origin/exception PASS
scripts/verify_native_runtime.ps1 #fake SDK162 durable rows; stale/disabled/failed-use-install cleanup PASS
scripts/verify_pinned_sdk_headers.ps1 #22202 official headers,10 native objects PASS
& "$env:BEDROCKINSIGHT_CLANGCL_ROOT/llvm-nm.exe" --undefined-only .build/pinned-sdk-header-compile/player_use.obj
scripts/verify_interval_runtime_import.ps1 #8 valid/27 rejected; exact use signature+weak requirement
python -W error::ResourceWarning -m unittest discover -s tests -p 'test_*.py' -v #75 PASS26.301s
node tests/analytics_dashboard_ui_tests.cjs #base4 categories/auth/selectors/no-sample/stale PASS
node --check companion/web/actions.js #PASS
scripts/build_server_candidate.ps1 #10 units,prelink/link/package/named imports PASS
```

First new-adapter compile caught incorrect `.get()` on the actual direct Player&
typed member; corrected to pinned reference, no offset/ABI workaround. Initial
fake test variable-name collision corrected. Subsequent complete native/fixture
builds pass. Five new Python tests cover response vs delta/zero/increase, malformed/
attempt/legacy/false-claim rejection, JST/ties/pages/selection, schema1..4 read-only
hash, bounded types/future records, actual fixtureHTTP401/200 to Japanese JS DOM.
DOM tests are a VM DOM harness, NOT browser rendering or engine actions.

Evidence:

- Boundary logs `.build/player-action-tests/run-24bc4b3ef67440cabd403c002a743729/`.
- Final75-test/DOM/JS logs `.build/player-action-tests/run-20b7ff2a650d4e1fb0becd779cbc2c97/`.
- New candidate `.build/server-candidate/run-d0b293d34d2f42c4bb7e918f9bab131e/`.
- DLL SHA256 `F9D1DA65216DB0F6FC7AAC8148FB6B3E2981C9A33759C6783A260754EAD4959E`.
- ZIP SHA256 `958E0368348F034C7387DE9DAB8E731A93B256352751671060DCD7A6829CAC23`.
- candidate-evidence.json:40 LL names resolve/961 official exports,10 objects,
  old-client=false,prelink=true,SymbolProvider1.3 retained,msvc_delayimp=false.
 48 native source/header hashes independently MATCH at15:14; full matched SDK /
 runtime dependencies /BDS load fields remain false, not a general release PASS.
- Actual object import `?$useItem@GameMode@@QEAA_NAEAVItemStack@@W4HandSlot@@@Z`;
 only this exact reviewed name added to strict10-name runtime delay gate.
-15:14 read-only audit: protected BDS748 and companion28268 same start times;
 protected original mods/BedrockInsight DLL remains B286F054...8A00.
15:16 netstat confirms127.0.0.1:18767 LISTENING PID28268. No token inspected.

|Feature|Source / implementation|F9D1 compile|Fixtures / SQL / UI|Latest candidate enable|Real action→SQLite→UI|
|---|---|---|---|---|---|
|Playtime/sessions/block attempts|Existing preserved|PASS|Regression PASS|NOT RUN|Historical Rome136093ms baseline; no new claim|
|Busy MSPT/effective TPS|Existing preserved|PASS|Regression PASS|NOT RUN|Accepted09:18 on218A; not repeated|
|Drop true result|Player::$drop / implemented|PASS|PASS|NOT RUN (old95FE empty smoke only)|UNVERIFIED|
|Dead flag / cause|Player::$die + weak state / implemented|PASS|PASS|NOT RUN|UNVERIFIED|
|Sprint/sneak flags|Typed notification + delayed flags / implemented|PASS|PASS|NOT RUN|UNVERIFIED|
|Pickup main-inventory gain|Player::take true + delta / implemented|PASS|PASS|NOT RUN|UNVERIFIED|
|Jump upward impulse|Mob::jumpFromGround + direct state / implemented|PASS|PASS|NOT RUN|UNVERIFIED|
|Direct-source dead transition|Mob::$die + matching source Player / implemented|PASS|PASS|NOT RUN|UNVERIFIED|
|Use true response/main-inventory loss|GameMode::$useItem + snapshots / implemented|PASS|PASS|NOT RUN|UNVERIFIED|
|Craft/travel/break/complete consumption|Success/lifetime/filtered authority still pending|—|Unavailable|—|NOT IMPLEMENTED|

docs/PLAYER_ACTIONS.md, ANALYTICS_DASHBOARD.md, scoped SECURITY_REVIEW.md and
PLAYER_ACTION_HOST_VALIDATION.md updated. Host handoff now pinsF9D1 and includes
actual use/hand/count comparisons with separate response/decrease semantics.
External coordinator's authorized normal-host route plus willing participant is
required for fresh-world actual actions; current single-player helper must NOT
be hand-edited into PvP/allowlist expansion. No sandbox BDS retries/approval bypass.
Next implementation work remains craft completion semantics, authoritative travel
filtering and lifetime-safe break verification. Whole request/project NOT COMPLETE.

## 2026-11-06 14:54 JST — LK C3: direct-attack source attribution + observed Mob dead transition

Continued after14:36 jump checkpoint. Latest STATUS/AGENTS, full LK task and
official pinned declarations/publishers reviewed; MSPT remains ACCEPTED, not
repeated. No old resolver fix/smoke redo, staging, BDS launch, host-connector/
approval retry, used-world reuse or18767 restart. AGPL5bb8bfb...combat reference
inspected factually only, no code/comments/translations/artifacts copied.
Existing four-category base, schema4/migrations/single writer and old static
index/app.js/CSS preserved; new features are NOT hot-deployed to18767.

Independent player_kills.cpp/.hpp and direct_kill.hpp add one supported MCAPI
Mob::$die Lowest wrapper. Eligible source: pinned EntityAttack=2 (static_assert),
entity=true, child=false, block=false, nonzero matching getEntityUniqueID and
getDamagingEntityUniqueID. Victim's own Level.fetchEntity(id,false) must resolve
to the same checked nonremoved Player with UUID, not victim/self, same Level.
No last-attacker/name-based/unknown attribution or projectile getOwner inference.
Only victim dead flag false BEFORE and true AFTER normal origin completion on
same nonremoved weak-resolved Actor counts. Already-dead/replaced/removed/not-dead/
nested/disabled cases omit. Identity checked before post flag read; no source,
attacker or old victim dereference after origin. Entry copies credited UUID/name
and victim type/class. Actor type must be namespaced and agree with checked
player-vs-mob class; victim IDs/names not stored. Origin exactly once; observer
exceptions preserve engine exception; nested calls invalidate outer and inner.

Scope is the entry-reported direct-attack Player plus an observed dead-flag
transition in THIS Mob path, NOT all kills, true causal killer under arbitrary
mods, unique physical death or durable world death. Projectile/environment/
indirect/unknown and non-EntityAttack causes (incl MaceSmash) unmeasured. Player
deaths bypassing Mob::$die and quickly removed victims can be missed. Flag reset
and later re-establishment can form another observation; no absolute physical
death uniqueness promised. Existing victim-side PlayerDeath source/notifications
stay separate; new event does NOT inflate legacy full kills or victim deaths.

Official source072b9ffe...MobDie publisher is pre-origin notification, not used
as success. Source const virtuals and Level.fetchEntity use exact official C++
declarations, not MCNAPI thunks/guessed slots/offsets. LL Actor::getTypeName/isDead,
checked Player and weak API used; no health/absorption/unsupported ABSORPTION.
Actual official object import inspected:
`?$die@Mob@@QEAAXAEBVActorDamageSource@@@Z` plus named LL Actor::getTypeName.
PE gate adds ONLY exact Mob die, requires reviewed weak constructor; full set now
nine exact reviewed runtime names. Source/Level virtual ABI, weak dead-state and
coverage still require actual engine execution, not compile inference.

Schema4 unchanged, append-only DirectKillObservation/direct_kill_observation,
cause=ll26.60.direct_attack_dead_transition.v1,amount=1,actor=credited Player,
primary=ENTRY victim type,secondary=mob|player,post UTC timestamp,activation ID.
Strict bounded details hook=Mob::$die,phase=post_dead_transition,damage_cause=2,
source_entity/ids_equal/resolved_player=true,source_child/block/self/before_dead/
nested/world_death_verified=false,after_dead=true. Existing bounded queue/single
writer, no SQLite in hook, O(1) reads/copies. DLL pin/pass-through disable/shared
in-flight token/weak Capture/generation retained. Install failure closes previous
bridges/listeners/worker; disable drains before re-enable. Stale callback rejection
and partial activation cleanup covered by fake lifecycle, NOT real MC calls.

New companion direct_kills/direct_mob_kills/direct_player_kills SQL metrics use
only exact source/actor mapping/type-class/amount=1/strict JSON flags. Positive
JST day/month/all rankings, selected Player, entry victim types top100, first/
last/scope/source/definition wired into Japanese/actions. Missing opposite kind
or collection remains null, never inferred zero from a mob/player observation.
No victim raw IDs exposed; textContent and opaque selection retained. Auth,
loopback/Host/Origin/read-only limits unchanged. New authorized process only:
127.0.0.1:18768/actions with OWN random token; no new persistent viewer started.

Actual cached-toolchain commands:

```powershell
$env:BEDROCKINSIGHT_CLANGCL_ROOT='C:\Dev\LL2660Toolchain-20261029\clang+llvm-23.1.4-x86_64-pc-windows-msvc\bin'
$env:BEDROCKINSIGHT_SQLITE_ROOT='C:\Users\chihirosub\AppData\Local\.xmake\packages\s\sqlite3\3.54.0+100\b897e7007f3d49769c227e337e14fdef'
$env:BEDROCKINSIGHT_XMAKE_PACKAGES='C:\Dev\LL2660BuildCache\.xmake\packages'
scripts/verify_player_drop.ps1 #prior boundaries + direct-source/32 transitions/repeat/respawn/exception PASS
scripts/verify_native_runtime.ps1 #fake SDK161-row drain, stale/disabled/failed-install cleanup PASS
scripts/verify_pinned_sdk_headers.ps1 #9 official native objects PASS; inspect actual Mob die import
scripts/verify_interval_runtime_import.ps1 #7 valid /23 rejected incl wrong Mob die/missing weak
python -W error::ResourceWarning -m unittest discover -s tests -p 'test_*.py' -v #70 PASS,22.175s
node tests/analytics_dashboard_ui_tests.cjs #baseline4-category Japanese DOM PASS
scripts/build_server_candidate.ps1 #official22202 headers,9 units,prelink/link/package PASS
node --check companion/web/actions.js
node --check tests/action_dashboard_ui_tests.cjs
```

Boundary logs `.build/player-action-tests/run-575a37eac26440a385131372ac5cf597/`;
full70 Python/DOM logs `.build/player-action-tests/run-6ff3595a5d424cd4bfa1f0932ef79405/`.
New5 Python tests: credited Player vs victim-side death, class/type/count/selected
breakdown, notifications/legacy/projectile/environment/self/unknown/wrong-flag
exclusion, missing opposite kind=null, JST periods/ties/pages, schema1..4 read-only
hash stability, top105->100/future exclusion, actual ephemeral HTTP401/200 ->
Japanese/actions DOM with hostile literal names. C++ checks source predicates,
32 dead-transition combinations, repeated pre-dead vs respawn, origin/errors.
Fake lifecycle emits one mob and one player observation to writer and verifies
legacy kills=0/deaths unchanged, no stale replay. It does NOT execute source
virtuals/Level lookup/Mob hook/weak engine path. No gameplay/browser screenshot
proof asserted. All actual build/tests passed; no online sandbox attempt.

NEW candidate `.build/server-candidate/run-e7cd517e08a34462a57df10916cf1ce0`:
DLL SHA256 `E1940880DFB943D6CBB8DA7B399999F8F9F5A1902F4F3508CE85CEC31088A6C3`;
ZIP SHA256 `8F403DE4D1FB8F2B686B9E07C45A93A4BC72DBB82E76AD16C13B7BB0D19A88CC`.
40 named LL imports resolve against961 official exports; SymbolProvider retained,
old_client_import_library_used=false.14:52 audit: all45 recorded source/header
hashes match, nine native objects, DLL/ZIP digests match. Manual server-function
candidate is NOT full matched-SDK/general release certification. **ThisE194
candidate has NEVER been staged/loaded/enabled in BDS.** Old95FE empty smoke and
218A genuineMSPT cannot be borrowed as new direct-kill/ABI execution acceptance.

| Feature | Source | Implemented / compiled | Synthetic SQLite/API/Japanese UI |E194 isolated enable | Live action / SQLite / UI |
| --- | --- | --- | --- | --- | --- |
| Direct-attack Player attribution + victim dead transition | MCAPI Mob die + official Source/Level virtuals + LL weak/type/dead | Implemented,E194 PASS | PASS incl5 new Python/32 C++ combinations | NOT RUN | NOT OBSERVED / NOT OBSERVED / fixture only |
| Jump impulse / pickup delta / delayed sprint-sneak | Prior supported adapters | Preserved,E194 PASS | Regression PASS | NOT RUN | NOT OBSERVED / NOT OBSERVED / fixture only |
| Victim Player dead-flag / drop returned-true entry units | Prior adapters, separate sources | Preserved,E194 PASS | Regression PASS | NOT RUN;95FE older drop empty smoke only | NOT OBSERVED / NOT OBSERVED / fixture only |
| Playtime/rankings/block attempts | Existing durable sources | Preserved | Regression PASS | Historical evidence only | Real136093ms session retained; blocks remain attempts |
| Processing MSPT/effective TPS | Existing busy timer | Preserved | Regression PASS |218A prior genuine proof | Accepted09:18,6windows/604ticks; not repeated |
| Full/indirect kills / completed jump/full pickup/use consumption/break/craft/travel | Need specific supported postconditions | Pending | 未計測 | Pending | Pending |

Updated docs/PLAYER_ACTIONS.md, docs/ANALYTICS_DASHBOARD.md,
docs/PLAYER_ACTION_HOST_VALIDATION.md and scoped docs/SECURITY_REVIEW.md. No
independent reviewer identity/runtime certification claimed. Fresh normal-host
trial needs consenting participant, NEW root/free ports, actual direct Mob
attack/death rows plus other actions/API401+200/Japanese UI/normal stop/integrity.
Player-vs-player requires explicit SECOND participant and two-player isolated
profile approval; existing single-player helper/allowlist must not be hand-edited
or protected two-player server reused. Otherwise PvP stays untested, not fake PASS.
No coordinator runtime evidence yet; no offline-sandbox retry or denied host-route
bypass. Remaining use/break/craft/travel work can proceed without MSPT redo.
14:52 read-only check: PID748/PID28268 retain original starts, protected2bbb DLL
stillB286F054...D8A00. No security/auth/network/other-project change, Git write retry,
push or release. PROJECT_STATUS remains IN_PROGRESS.

## 2026-11-06 14:36 JST — LK B: ground-jump direct-state impulse + period rankings/Japanese UI

Continued after14:16 pickup checkpoint. Latest STATUS/AGENTS and full LK task
read; old pending-MSPT notes remain superseded. Genuine MSPT ACCEPTED, not redone.
No95FE smoke repeat, used-root reuse, staging, BDS launch, host-connector/approval
retry, live DLL replacement or18767 restart. AGPL reference used factually only;
no upstream code/comments/translations/artifacts copied. Existing four-category
base viewer, schema4/migrations/queue, original index/app.js/CSS preserved.

Independent player_jump.cpp/.hpp plus jump_observation.hpp observes the supported
MCAPI Mob::jumpFromGround Lowest entry/normal exit. Official checked LL Player
conversion and same-Mob comparison exclude nonplayers. Pre-state must be grounded,
nonriding/nondead with UUID; post-state weak-reacquires the same nonremoved Player
and requires same dimension, no mount/death, direct PosDelta.y positive and greater
than baseline. No old Mob/Player dereference after origin. Origin exactly once;
observer errors cannot replace engine exceptions. Equal repeated calls/no-op,
airborne pre-state, nested calls (outer AND inner), invalid/removed/replaced actor,
mount/death/dimension changes are omitted. Bounded quantization x1e6, finite |Y|<=16;
tiny rounded/elevated out-of-cap changes are deliberately unmeasured.

This is an upward DIRECT STATE observation during a ground-jump method, NOT
input notification count, completed physical/airborne jump count, distance, or
causal ownership proof under arbitrary other mods. Immediate post ground flag
may remain true until physics; no airborne completion is inferred. This function
may not fire on every server/player movement path; runtime coverage still unknown.
Native code writes no position/velocity/item/NBT and intercepts no packet.
O(1) state checks, bounded names/details, existing single-writer queue; no SQLite
or high-frequency input sampler in hook. DLL pin, shared in-flight token, weak
Capture/activation generation and pass-through disable retained. Install failure
closes prior bridges/listeners/writer; disable drains before re-enable.

Important API finding: pinned Actor::getVelocity for Mobs reads post-tick movement
or a vehicle, NOT direct immediate jump impulse. It is NOT used. Exact official
inline Actor::getPosDelta reads state vector, LL isOnGround checks the official
OnGroundFlagComponent. PlayerJumpEvent's publisher is packet-condition derived
with cancellation teleport, not a confirmed physics jump and not reused here.
Actual object import inspection verified new MCAPI names:
`?jumpFromGround@Mob@@QEAAXXZ`, `?getVehicle@Actor@@QEBAPEAV1@XZ`.
Static gate admits only this reviewed pair plus existing six names, requires
weak constructor, rejects signature/owner/vehicle/weak mismatches. No guessed
symbol/offset/virtual slot, MCNAPI isJumping or absorption accessor introduced.

Schema4 unchanged, append-only PlayerJumpImpulse / player_jump_impulse,
cause=ll26.60.jump_pos_delta.v1,amount=1,primary=minecraft:player,post UTC timestamp,
activation ID. Bounded details hook=Mob::jumpFromGround,phase=post_upward_impulse,
measurement=pos_delta_y_x1e6,grounded_before=true,vehicle_before/after=false,
before_y_micro/after_y_micro/dimension,world_jump_verified=false. No legacy
successful-distance/use/kill aggregate repurposed. Exact-source/mapped-player/
amount=1/type/strict JSON flags/bounded integer delta/dimension guards in reader.
No valid observation means null, never notification-derived success/zero.

SQL-backed jump_impulses metric, JST day/month/all-time positive deterministic
ranking, selected player, count/scope/source/first/last/definition connected to
new Japanese/actions. Literal textContent/opaque IDs retained. Server auth,
loopback/Host/Origin/read-only budgets unchanged. New authorized process only:
127.0.0.1:18768/actions with own random token; none persistently started here.
18767 original Python stays in memory, new routes/features are not hot-deployed.

Actual commands (same cached env; no downloads):

```powershell
$env:BEDROCKINSIGHT_CLANGCL_ROOT='C:\Dev\LL2660Toolchain-20261029\clang+llvm-23.1.4-x86_64-pc-windows-msvc\bin'
$env:BEDROCKINSIGHT_SQLITE_ROOT='C:\Users\chihirosub\AppData\Local\.xmake\packages\s\sqlite3\3.54.0+100\b897e7007f3d49769c227e337e14fdef'
$env:BEDROCKINSIGHT_XMAKE_PACKAGES='C:\Dev\LL2660BuildCache\.xmake\packages'
scripts/verify_player_drop.ps1 #drop/death/mode/pickup + jump direct-state boundaries PASS
scripts/verify_native_runtime.ps1 #fake SDK lifecycle/159-row drain/old-generation rejection PASS
scripts/verify_pinned_sdk_headers.ps1 #8 official native units PASS; actual jump/vehicle imports inspected
scripts/verify_interval_runtime_import.ps1 #6 valid /21 rejected signature/dependency/shape cases
python -W error::ResourceWarning -m unittest discover -s tests -p 'test_*.py' -v #65 PASS,19.403s
node tests/analytics_dashboard_ui_tests.cjs #baseline Japanese4-category DOM PASS
scripts/build_server_candidate.ps1 #official22202 headers,8 units,prelink/link/package PASS
node --check companion/web/actions.js
node --check tests/action_dashboard_ui_tests.cjs
```

C++ boundary evidence:
`.build/player-action-tests/run-836c56f232964748929fb1a4f0e91280/`.
Full65 Python/baseline DOM logs:
`.build/player-action-tests/run-cb3fbf3d197545e5a97d5b3ee4420cf9/`.
Five new Python tests cover source-vs-notification, wrong flags/delta/amount/type/
version/dimension, unknown/null, selected scope, JST periods/ties/pages, old-schema
1..4 read-only hash stability, first/last/future exclusion and actual ephemeral
HTTP401/200->Japanese/actions DOM with literal hostile names. C++ covers grounded/
mount/death/dimension/identity/nested predicates, nonfinite/cap/rounding bounds,
same-value repeat and origin/exception behavior. Fake jump lifecycle seam proves
queue drain, install-failure cleanup and stale/disabled rejection, NOT actual
native jump hook or state-vector/weak engine execution. No gameplay or browser
screenshot proof asserted. All compile/test commands passed, no online attempt.

NEW candidate `.build/server-candidate/run-146e37a42ac749b2b2e4794fbb5af9fb`:
DLL SHA256 `7AA2CAB4501E7AA93EC5E47957E37ED3AE6E76FFE436B96F5B9ADE61D687CB66`;
ZIP SHA256 `BFE311F55C736183AA38498D1ADE26DD3DAFB13FCAEC70F584FA232BF72871F8`.
39 named LL imports resolve against961 official exports; new LL isOnGround only.
SymbolProvider retained, old_client_import_library_used=false.14:34 audit: all42
source/header hashes match, eight native objects, DLL/ZIP digests match evidence.
Manual server-function candidate remains NOT a full matched-SDK/general release
certification. **This7AA2 candidate has NEVER been staged/loaded/enabled in BDS.**
Do not borrow95FE smoke or218A genuineMSPT acceptance as new action/ABI proof.

| Feature | Verified source | Implemented / compiled | Synthetic DB/API/Japanese UI |7AA2 isolated enable | Live action / SQLite / UI |
| --- | --- | --- | --- | --- | --- |
| Ground-jump upward direct-state impulse | MCAPI jumpFromGround/getVehicle + LL ground/weak + official state getter | Implemented,7AA2 PASS | PASS incl5 new Python/boundaries | NOT RUN | NOT OBSERVED / NOT OBSERVED / fixture only |
| Pickup main-inventory delta | take + checked LL/const virtual readers | Preserved,7AA2 PASS | Regression PASS | NOT RUN | NOT OBSERVED / NOT OBSERVED / fixture only |
| Sprint/sneak delayed flags | typed LL + real flag getter | Preserved,7AA2 PASS | Regression PASS | NOT RUN | NOT OBSERVED / NOT OBSERVED / fixture only |
| Dead-flag transition/cause | die + LL isDead/weak | Preserved,7AA2 PASS | Regression PASS | NOT RUN | NOT OBSERVED / NOT OBSERVED / fixture only |
| Drop true-return units/type | drop result | Preserved,7AA2 PASS | Regression PASS | NOT RUN;95FE prior empty smoke only | NOT OBSERVED / NOT OBSERVED / fixture only |
| Playtime/rankings/block attempts | Existing durable sources | Preserved | Regression PASS | Historical evidence only | Real136093ms session retained; blocks are attempts |
| Processing MSPT/effective TPS | Existing busy timer | Preserved | Regression PASS |218A prior genuine proof | Accepted09:18,6windows/604ticks; not repeated |
| Completed physical jump/full pickup/use consumption/break/kill/craft/travel | Need supported lifetime-safe postconditions | Pending | 未計測 | Pending | Pending |

Use/break declaration review: Player::$useItem is MCAPI void, and
ItemStackBase::hurtAndBreak MCAPI bool exists. consumeItem or entry damage+delta
does not alone prove actual consumption/break. Need verified call timing and
lifetime-safe same-item/owner post-state; no stale item dereference or invented
success counter added. Remaining C adapters can continue independently, not a
claim all APIs are absent or permission to redo accepted MSPT.

Updated docs/PLAYER_ACTIONS.md, docs/ANALYTICS_DASHBOARD.md,
docs/PLAYER_ACTION_HOST_VALIDATION.md (fresh7AA2 candidate, actual ground jumps,
rows/API/UI comparison, no input-count substitution) and docs/SECURITY_REVIEW.md.
Focused self-review only, no independent identity/runtime certification. Genuine
action acceptance requires new normal-host root/free ports/explicit participant,
actual actions, SQLite/source/401+200/Japanese UI, normal stop and integrity.
No sandbox online BDS retry or host-route denial bypass.14:34 read-only check:
PID748/PID28268 retain original starts and protected2bbb DLL remainsB286F054...D8A00.
No security/auth/network/other-project change, Git write retry, push or release.
PROJECT_STATUS remains IN_PROGRESS.

## 2026-11-06 14:16 JST — LK B: successful-take main-inventory delta + type/ranking/Japanese UI

Continued after14:00 modes checkpoint. Read latest STATUS/AGENTS, full LK task,
OFFICIAL_INPUTS and pinned declarations/publishers. Genuine MSPT stays ACCEPTED;
no MSPT/old resolver redo, no95FE smoke repeat, no used root reuse. No BDS launch,
staging, host-connector/approval retry, security/auth change or18767 restart.
AGPL upstream release5bb8bfb... was a factual reference only; no code/comments/
translations/artifacts copied. Existing sidecar/schema4/queue/old static assets retained.

Implemented independent player_pickup.cpp/.hpp and pickup_observation.hpp.
Exact MCAPI Player::take(Actor&,int,int) Lowest observes true return AND actual
positive main-inventory type-total gain. A pre-origin checked LL ItemActor
conversion copies item type/entry units, UUID/name, bounded inventory totals,
inventory identity/size and official player weak ref. Origin exactly once.
After successful return, only a weak-reacquired same nonremoved Player is read;
no old ItemActor/ItemStack/Player reference dereference. Same inventory and size,
1<=net gain<=entry units<=255,1..128 slots required. Partial entry64/gain3 counts3,
NOT64. Failed/cancelled/exception/zero/decrease/ambiguous oversize gain, changed
container/size, removed/replaced player are omitted. Nested take invalidates outer
as well as excluding inner; observational errors cannot alter engine bool/exception.

This is explicitly main-inventory net gain DURING a true-return take scope, NOT
all pickup, a client-action count, exact causal transfer of one ground entity,
NBT-specific detail or durable-world/inventory persistence. Same-type changes by
other mods can affect the net observation; equipment/offhand/cursor/other containers
are not included. Official Player::getInventory assumes initialized inventory;
actual container virtual dispatch and weak Player resolution still need real BDS.
Maximum128 slots per scan, two scans per observed call, no per-tick/packet polling
or SQLite in the hook. Existing bounded single-writer queue; no retry of dropped
events. Shared in-flight token, module pin, pass-through disable, weak Capture and
activation generation protect stale callbacks. Pickup install failure closes prior
death/drop/busy bridges and writer; disable drains and closes before re-enable.

Pinned LL26.60 source072b9ffe... verified: PlayerPickUpItemEvent is BEFORE take,
mutable args/cancellable, and cannot alone count success. Player::getInventory,
ItemActor::tryGetFromEntity + const item(), Player::tryGetFromEntity and weak refs
are official LL APIs. Container/FillingContainer getItem/getContainerSize are
official const virtual declarations, not guessed slot indices or MCNAPI thunks.
Actual official-object compiler import observed:
`?take@Player@@QEAA_NAEAVActor@@HH@Z`. Static PE gate admits only this new exact
name and requires reviewed weak constructor; no arbitrary import/ABI whitelist.
No item/NBT/container mutation, init-container call or packet interception added.
Pinned jump publisher remains packet-condition derived; no physical-jump count
was inferred from it. Remaining categories stay individually unmeasured.

Schema4 unchanged, append-only EventKind::ItemPickupDelta / item_pickup_delta,
cause=ll26.60.take_inventory_delta.v1, amount=net gained units, primary_type=item ID,
post-observation UTC timestamp, activation ID in session_id. Bounded details:
hook=Player::take, phase=return_true_inventory_delta,scope=main_inventory_type_total,
entry_units/slots/before_units/after_units,nested=false,durable_transfer_verified=false.
No success-oriented legacy aggregates repurposed. Reader requires mapped player,
exact source/type/integer quantity, JSON booleans/scope and matching bounded delta.
Missing or invalid collection is null, never zero or notification-derived success.

New companion metrics pickup_inventory_items / pickup_delta_actions separate units
and observations; SQL JST day/month/all-time positive-only deterministic rankings,
selected player, top100 item types, first/last/scope/source/uncertainty definition
are connected to Japanese/actions. Uses textContent/opaque IDs; auth/loopback/
read-only budgets unchanged. Old18767 process and original index.html/app.js/CSS
untouched. New authorized process only:127.0.0.1:18768/actions with OWN random token;
none was persistently started here. Existing four-category base view preserved.

Actual cached-toolchain commands:

```powershell
$env:BEDROCKINSIGHT_CLANGCL_ROOT='C:\Dev\LL2660Toolchain-20261029\clang+llvm-23.1.4-x86_64-pc-windows-msvc\bin'
$env:BEDROCKINSIGHT_SQLITE_ROOT='C:\Users\chihirosub\AppData\Local\.xmake\packages\s\sqlite3\3.54.0+100\b897e7007f3d49769c227e337e14fdef'
$env:BEDROCKINSIGHT_XMAKE_PACKAGES='C:\Dev\LL2660BuildCache\.xmake\packages'
scripts/verify_pinned_sdk_headers.ps1 #7 official native objects PASS; inspect actual take import
scripts/verify_player_drop.ps1 #drop/death/mode + pickup arithmetic/origin/exception boundaries PASS
scripts/verify_native_runtime.ps1 #fake SDK lifecycle/queue/158-row SQLite drain PASS
scripts/verify_interval_runtime_import.ps1 #5 valid /17 rejected, incl wrong take signature/no weak API
python -W error::ResourceWarning -m unittest discover -s tests -p 'test_*.py' -v #60 PASS,17.428s
node tests/analytics_dashboard_ui_tests.cjs #baseline Japanese4-category DOM PASS
scripts/build_server_candidate.ps1 #official22202 headers,7 units,prelink/link/package PASS
node --check companion/web/actions.js
node --check tests/action_dashboard_ui_tests.cjs
```

New5 Python tests: partial delta vs entry/attempt count, selected type breakdown,
unknown/null/invalid version/JSON/type/flags/quantity/bounds/source, JST boundaries,
deterministic ties/pages, schema1..4 read-only hash stability, bounded105->100 types,
actual ephemeral HTTP401/200 -> Japanese/actions DOM with literal hostile names.
Full60 tests/DOM logs:
`.build/player-action-tests/run-dc686d63532848efaed33ceb10c41ba1/`.
C++ boundary logs:
`.build/player-action-tests/run-df4a58b6a95b42eeaa0871dfeb39da28/`.
Native runtime test uses a FAKE pickup lifecycle seam, not engine inventory/hook
execution; it proves admitted delta drain, install-failure cleanup, disabled and
old-generation callback rejection. Core tests cover partial/full gain, changed
size/identity, invalid/overflow quantities, origin exactly once and exception
preservation. No real player pickup, virtual container or weak Player execution
is proved by these tests; DOM is not a browser screenshot/gameplay proof.
During concurrent compile, two shell starts hit the known WindowsApps
CreateProcessAsUserW denied5 wrapper issue; no command ran. Waited for the existing
compile and continued normally, no policy override or fast retry loop. Build/tests
themselves passed; no network/auth failure was reclassified as a DLL result.

NEW artifact `.build/server-candidate/run-1f019fef719e4b40b1659bcf7bb92b2a`:
DLL SHA256 `5FC02F422CBD84A70A1D34BCB478E9D85CF3305DB176BECABE16303A038B7FCF`;
ZIP SHA256 `93C737B590D88F0258B2F80112BD359A9AD93581A8471A6246D5F845A3F48974`.
38 named LL imports resolve against961 official exports;4 new LL readers above.
SymbolProvider preserved, old_client_import_library_used=false. Exact runtime
MCAPI set is prior tick/count/drop/die/weak-constructor plus verified take.
14:14 audit: all39 source/header hashes match evidence, DLL/ZIP digests match.
Manual server-function candidate is NOT full matched-SDK/general-release certification.
**This5FC0 candidate has NEVER been staged/loaded/enabled in BDS.** No95FE empty
smoke or218A genuineMSPT proof is borrowed as new action/ABI execution acceptance.

| Feature | Source found | Implementation / compile | Synthetic DB/API/Japanese UI |5FC0 isolated enable | Live action / SQLite / UI |
| --- | --- | --- | --- | --- | --- |
| Pickup-scope main-inventory type delta | MCAPI take + LL checked item/player/inventory + official const virtual readers | Implemented,5FC0 PASS | PASS incl5 new Python/boundaries | NOT RUN | NOT OBSERVED / NOT OBSERVED / fixture only |
| Sprint/sneak delayed flags | Typed LL + real flag getter | Preserved,5FC0 PASS | Regression PASS | NOT RUN | NOT OBSERVED / NOT OBSERVED / fixture only |
| Dead-flag transition/cause | Player::$die + LL weak/isDead | Preserved,5FC0 PASS | Regression PASS | NOT RUN | NOT OBSERVED / NOT OBSERVED / fixture only |
| Drop true-return entry units/type | Player::$drop result | Preserved,5FC0 PASS | Regression PASS | NOT RUN; old95FE empty smoke only | NOT OBSERVED / NOT OBSERVED / fixture only |
| Playtime / rankings / block attempts | Existing durable sources | Preserved | Regression PASS | Existing historical evidence only | Real136093ms session retained; blocks are attempts |
| Processing MSPT / effective TPS | Existing busy timer | Preserved | Regression PASS |218A genuine prior proof | Accepted09:18,6windows/604ticks; not repeated |
| Physical jump / successful use / kill / craft / travel / item break / full durable pickup | Need supported category postconditions | Pending | 未計測 | Pending | Pending |

Updated docs/PLAYER_ACTIONS.md, docs/ANALYTICS_DASHBOARD.md, normal-host real-player
handoff docs/PLAYER_ACTION_HOST_VALIDATION.md and focused docs/SECURITY_REVIEW.md.
No independent reviewer identity/runtime certification claimed. Normal-host next
trial must use NEW root/free ports/explicit consenting participant, actual full/
partial/no-room pickup where feasible, drop/death/modes, SQLite/source/401+200/UI,
normal stop/integrity. Untested edge cases must stay marked untested, not fabricated.
No online CodexSandboxOffline launch or host-route denial bypass; no coordinator
runtime evidence for this candidate yet. Remaining adapters may continue without
redoing accepted MSPT. Read-only14:14 check: PID748/PID28268 retain original starts,
protected2bbb DLL stillB286F054...D8A00. No Git write retry, push/release, security,
auth/network or other-project change. PROJECT_STATUS remains IN_PROGRESS.

## 2026-11-06 14:00 JST — LK B: bounded delayed sprint/sneak observations + SQL/API/Japanese UI

Continued from13:37 C1 after latest STATUS/AGENTS and full LK task review. Genuine
MSPT remains ACCEPTED; no MSPT rerun, old resolver fix,95FE smoke repetition,
staging, online BDS launch or host-connector/approval retry. Existing BDS roots,
worlds,18767 viewer and original web/index.html/app.js/styles.css are untouched.
No AGPL source/comments/translations/artifacts copied; references used factually.

Independent player_modes.cpp/.hpp and pure mode_observation.hpp use four typed
LL26.60 pre-origin start/end notifications only to queue bounded pending state,
NOT count input packets. A250ms..2s later ServerLevelTick callback reacquires the
same nonremoved actor using official WeakEntityRef and reads real server flags
via LL getStatusFlag/inline isSprinting/isSneaking. Only baseline!=desired and
actual==desired produces an observation. Already-cancelled sneak, expired,
removed/replaced/mismatched observations are omitted. MCNAPI setters are NOT
hooked, no packet interception/guessed offsets, no engine/world/item mutation.
getStatusFlag is a named official LL export, no new MCAPI import beyond80BD.

Tracker max256 players, two pending states/four fixed counters each; duplicate
requests do not extend2s TTL and short/opposite toggles coalesce. Flag polling
max4Hz, aggregate queue submissions at>=5s or JST day boundary/disconnect/disable.
No per-input/tick DB writes. Capture::mutex serializes tracker and synchronous
nonretained sink; no delayed raw Capture/Player task or sink mutex recursion.
Stop flushes already observed primitives before writer drain, not pending input.
Queue rejection has no replay/double count; crash can lose unflushed observations
(normally about5s, longer if ticks stall). Capacity/coalescing/weak failures make
these conservative observations, NOT exact complete input/transition totals.

Existing schema4/migrations/single-writer retained. New append-only
event_kind=player_mode_observation, cause=ll26.60.mode_flag_observed.v1,
primary=sprint|sneak, secondary=start|stop, amount=observed aggregate count.
details phase=delayed_flag_observation, poll_us=250000, flush_us=5000000,
coalesced=true, duration_measured=false, first_ms/last_ms/jst_day. Each row stays
within one JST date; event timestamp is last actual observation, NOT flush time.
Activation run ID stays in session_id. No distance/duration/current-mode inference
and no contribution to successful move/use/kill aggregates.

Companion exact-version/mapped-actor/type/edge/integer/strict JSON/cadence/date
guards exclude legacy, malformed or unmeasured rows. SQL SUM(amount), not row
or packet COUNT, powers sprint_starts/stops and sneak_starts/stops, JST day/month/
all-time positive-only deterministic rankings, player selection, mode breakdown,
first/last observations and scope/source/definition in new/actions. No record
means null, including a missing opposite edge, never inferred zero or current
state. Names remain literal textContent; UUIDs stay opaque. Auth, loopback,
read-only SQLite budgets unchanged. New process only:127.0.0.1:18768/actions
with its OWN per-start token; no persistent new viewer started here.

Additional factual craft review: LK fixed release's CreatedOutputContainer transfer
reference plus official26.60 declarations. Official _handleTransfer bool args are
isSrcHintSlot/isDstHintSlot/isSwap, not meaning inferred from upstream variable
names. Transfer Success alone is not yet verified as whole-request commit;
screen/request queue completion includes MCNAPI gaps. No observer is allowed to
create/init containers just to read them. Thus craft remains unmeasured pending
a verified supported nonmutating completion path. This is a conservative inference,
not an assertion of an upstream runtime defect. Other remaining categories stay
pending individually, not fabricated zero or a claim that all SDK events are absent.

Actual commands (cached toolchain only, no downloads):

```powershell
$env:BEDROCKINSIGHT_CLANGCL_ROOT='C:\Dev\LL2660Toolchain-20261029\clang+llvm-23.1.4-x86_64-pc-windows-msvc\bin'
$env:BEDROCKINSIGHT_SQLITE_ROOT='C:\Users\chihirosub\AppData\Local\.xmake\packages\s\sqlite3\3.54.0+100\b897e7007f3d49769c227e337e14fdef'
$env:BEDROCKINSIGHT_XMAKE_PACKAGES='C:\Dev\LL2660BuildCache\.xmake\packages'
scripts/verify_player_drop.ps1 #drop/state/mode boundaries PASS; actual player_modes.cpp + fake SDK
scripts/verify_native_runtime.ps1 #11 listener lifecycle/admission/SQLite drain157 rows PASS, fake SDK
scripts/build_server_candidate.ps1 #official22202 headers,6 native objects/prelink/link/package PASS
python -W error::ResourceWarning -m unittest discover -s tests -p 'test_*.py' -v #55 PASS,14.533s
node tests/analytics_dashboard_ui_tests.cjs #baseline Japanese4-category DOM PASS
node --check companion/web/actions.js
node --check tests/action_dashboard_ui_tests.cjs
```

Retained C++ boundary evidence:
`.build/player-action-tests/run-8dc64fb386d041dca643bd220a4b94df/`.
Retained full55 Python/DOM logs:
`.build/player-action-tests/run-3ef11c0c0b094f8188195f4d55b28c0b/`.
Five new Python tests cover batch quantities vs packet/row count, nullable edges,
source/type/bool/time/JST guards, day/month/all/ties/paging/selection, old schema1..4
read-only hash stability and actual fixture HTTP401/200->Japanese/actions DOM with
hostile literal names. C++ tests cover8 before/wanted/actual combinations, delay,
duplicates/short toggles/TTL/clock reversal, JST split, idempotent stop/disconnect,
removed/mismatched actors, Level filtering and256-entry capacity. Native lifecycle
test includes cancelled sneak exclusion and activation-stamped observed sprint.
These are synthetic tests, NOT real gameplay or browser screenshot evidence.

NEW candidate `.build/server-candidate/run-1e41eb1abef443dfbdae838a6bc33031`:
DLL SHA256 `01FCBC46371F7D090ABAAB132DE85E95AD2E8535526677883AEB82CD50780D8D`;
ZIP SHA256 `03B9FBDBF328C30AFCEF1B6A64182DA99473421F12DABB4F8991CF6C0C718CBD`.
34 named LL imports resolve against961 official named exports; SymbolProvider
retained; old_client_import_library_used=false. Exact reviewed runtime MCAPI
whitelist unchanged (tick/count/drop/die/weak constructor).14:00 read-only audit:
all36 recorded source/header hashes match, DLL and ZIP match the evidence.
Manual server-function candidate is NOT full matched-SDK/general-release proof.
**This01FC candidate has NEVER been staged/loaded/enabled in BDS.** Do not borrow
95FE empty smoke or218A MSPT acceptance as new action/weak-ref execution proof.

| Feature | Verified source | Implemented / compiled | Synthetic SQLite/API/Japanese UI |01FC isolated enable | Actual action / SQLite / UI |
| --- | --- | --- | --- | --- | --- |
| Sprint/sneak delayed start/end flags | Typed notifications + LL getStatusFlag/weak ref | Yes,01FC PASS | PASS incl5 new Python, boundary tests | NOT RUN | NOT OBSERVED / NOT OBSERVED / fixture only |
| Dead-flag transition/cause | Player::$die + LL isDead/weak ref | Preserved,01FC PASS | PASS | NOT RUN | NOT OBSERVED / NOT OBSERVED / fixture only |
| Drop true-return entry units/type | Player::$drop result | Preserved,01FC PASS | PASS | NOT RUN;95FE earlier empty smoke only | NOT OBSERVED / NOT OBSERVED / fixture only |
| Playtime / rankings / block attempts | Existing durable sources | Preserved | Regression PASS | Historical protected proofs only | Real136093ms session retained; blocks remain attempts |
| Processing MSPT / effective TPS | Existing busy timer | Preserved | Regression PASS |218A genuine historical proof | Accepted09:18,6windows/604ticks; not repeated |
| Pickup / jump / successful use / kill / craft / travel / item break | Need category-specific supported postconditions | Pending | 未計測 | Pending | Pending |

Updated docs/PLAYER_ACTIONS.md, docs/ANALYTICS_DASHBOARD.md,
docs/PLAYER_ACTION_HOST_VALIDATION.md with new candidate and explicit participant
test steps, and docs/SECURITY_REVIEW.md with focused self-review. No independent
reviewer identity/runtime safety certification claimed. New normal-host real-player
trial must use fresh root/free ports, preserve online auth, exercise each action,
compare SQLite/source/401+200/Japanese UI, normal stop and integrity. Explicit
participant/name/XUID required; no empty smoke repeat or sandbox online retry.
No host-route execution was possible/attempted here after the earlier approval
denial. Remaining adapters may be implemented incrementally without redoing MSPT.
Read-only13:58/14:00 check: PID748 and PID28268 retain original starts, protected
2bbb DLL stillB286F054...D8A00. No security/auth/network/other-project changes,
Git write retry, push or release. PROJECT_STATUS remains IN_PROGRESS.

## 2026-11-06 13:37 JST — LK C1: post-processing player dead-flag capture + Japanese analytics

Continued from the13:21 action checkpoint, after reading latest STATUS/AGENTS and
the full LK task. MSPT stays ACCEPTED; old wait/service/crash notes are historical.
No successful95FE/drop smoke or MSPT baseline rerun. No online BDS launch, staging,
host-connector/approval retry, live DLL replacement or18767 restart in this turn.

Implemented independent `player_death.cpp/.hpp` plus the pure state-transition
boundary: Player::$die Lowest calls origin once, observes false->true via pinned
LL Actor::isDead (IsDeadFlagComponent). Pre-origin stable UUID/name/cause snapshot
and WeakEntityRef; post-origin reacquires the same nonremoved Actor via the
registry/entity generation and compares identity WITHOUT dereferencing the old
Player. Already-dead/reentrant/removed/replaced/disabled/failed observations do not
increment. Engine exceptions survive; observer exceptions cannot bypass origin.
Manual install errors fail enable and close prior drop/busy bridges/writer.
Pinned pass-through hooks, shared in-flight state and capture activation generation
protect disable/re-enable. No unsafe isAlive/absorption MCNAPI, guessed offsets,
packet interception, killer inference or per-tick SQLite.

Schema4 unchanged: event_kind=player_death, cause=ll26.60.player_dead_transition.v1,
amount=1, primary_type=minecraft:player, secondary_type=cause:<pinned numeric code>,
details require hook/phase/before=false/after=true/killer_attribution=unmeasured.
Existing pre-origin PlayerDeathNotification remains SEPARATE. Writer deaths counts
receive only this new postcondition, not pre-notifications. API confirmed_deaths
uses exact new provenance and typed fields/details, never legacy daily totals.
Unknown cause=-1, codes outside-1..34 clamped unknown; no attacker attribution.
Missing collection is null, NOT zero; no current-online/current-dead inference.

SQL-backed JST day/month/all-time player/total rankings and cause breakdown,
first/last observation and definition are wired into the existing new/actions
page. Cause labels are independently written Japanese labels. Original web/index,
app.js/styles.css remain untouched; the old18767 process stays in memory. Newly
started authorized companion: http://127.0.0.1:18768/actions, own random token only.
No new persistent viewer started. Definition is a dead-FLAG transition, not proof
of a durable world death, unique physical deaths under arbitrary other mods, or
complete historical coverage. Weak/removed resolution can deliberately undercount.

Actual verification:

```powershell
scripts/verify_player_drop.ps1 #drop boundary +32 transition combinations/respawn/exception PASS
scripts/verify_native_runtime.ps1 #fake SDK lifecycle/SQLite drain156 events PASS
scripts/verify_interval_runtime_import.ps1 #4 valid /15 malformed or unreviewed cases rejected
scripts/build_server_candidate.ps1 #official22202 headers,5 native objects, prelink/link/package PASS
python -W error::ResourceWarning -m unittest discover -s tests -p 'test_*.py' -v #50 PASS
node tests/analytics_dashboard_ui_tests.cjs #baseline Japanese4-category DOM PASS
```

5 new Python tests distinguish notifications/legacy/wrong-version from post-state,
validate strict JSON booleans/codes/quantity/actor, unknown-vs-zero, selected causes,
JST day/month/all/ties/pages, schema1..4 read-only hash stability, actual ephemeral
HTTP401/200 -> Japanese/actions DOM with hostile literal names. Existing drop
and MSPT/stale/auth/playtime tests retained.50 PASS in12.282s; logs:
`.build/player-action-tests/run-11d2043b5b4d498f80349052420b1e38/`.
C++ boundary logs: `.build/player-action-tests/run-47fed92067df4fc1a4119475b6c4e0ff/`.
The weak-ref lifetime/engine ABI itself is NOT proven by these synthetic tests.
Initial compile-only probe failed on unqualified core Actor vs engine ::Actor
and the SDK enum alias; fixed using ::Actor qualification/direct typed enum.
Corrected official-object probe: run-cf21185085e64f5b899dee7064e00b75, then full
native build PASS. No ABI workaround or MCNAPI substitution used.

NEW artifact `.build/server-candidate/run-ef291d651a05440bb742ed4d8f0ef5ea`:
DLL SHA256 `80BDA5594F4153C7A039F51F80FA499CC6D7B4A8C8311E7F1247A82C734DF41A`;
ZIP SHA256 `472D130D37E9DED7BE5D1A24E38AE8EAE0C3BC7B1380438962E0DB8B6387DAE4`.
33 named LL imports resolve against961 official named exports, SymbolProvider
retained. Exact newly compiled MCAPI names are Player::$die and the official
StackResultStorageEntity(WeakStorageEntity const&) constructor; whitelist admits
only the reviewed pair plus prior drop/count/tick, rejects guesses/duplicates.
At13:37 current C++ source/header hashes all match this candidate and DLL digest.
This manual server-function candidate is NOT full matched-SDK/release certification.
**This80BD candidate has NEVER been staged/loaded/enabled in BDS.** Do not borrow
the older95FE smoke or218A real-MSPT result as acceptance for new death behavior.

| Feature | Official source | Code/compile | Synthetic SQLite/API/Japanese UI | This new candidate BDS enable | Actual action/SQLite/UI |
| --- | --- | --- | --- | --- | --- |
| Player dead-flag成立 | MCAPI die + LL isDead/WeakEntityRef | Implemented;80BD PASS | PASS incl32 boundary cases/5 new Python | NOT RUN | NOT OBSERVED |
| Death processing notification | Existing pre-origin typed LL | Preserved/compiled | PASS, separate counter | NOT RUN80BD | No confirmed-death claim from notification |
| Drop returned-true quantities/types | Existing independent hook | Preserved/compiled | PASS |95FE earlier smoke only;80BD NOT RUN | Still NOT OBSERVED |
| Playtime/rankings/block attempts | Existing durable sources | Preserved/compiled | Regression PASS | Existing historical proofs only | Real session136093ms remains valid; blocks remain attempts |
| Busy MSPT/TPS | Existing tick timer | Preserved/compiled | Regression PASS |218A genuine host proof retained | Accepted6windows/604ticks09:18, not repeated |
| Pickup/jump/sprint/sneak/craft/kill/travel/item break | Need verified success/transition semantics | Pending, no invented hooks | 未計測 | Pending | Pending |

Focused self-review and limitations are appended to docs/SECURITY_REVIEW.md; no
independent reviewer model identity or runtime safety certification claimed.
Updated docs/PLAYER_ACTIONS.md and docs/ANALYTICS_DASHBOARD.md. Concrete next
normal-host real-player handoff: docs/PLAYER_ACTION_HOST_VALIDATION.md. Requires
explicit participant/name/XUID, NEW root/free ports, actual drop/death/respawn,
SQLite/source/401+200/Japanese UI/normal stop/integrity. No empty smoke repetition.
Meanwhile continue remaining verified adapters incrementally; no need to wait or
redo MSPT. All used roots (including24156/769103/b63e) remain untouched.
Read-only13:37 check: PID748/PID28268 retain original starts and protected2bbb
DLL stillB286F054...D8A00. No security/auth/network/other-project changes, Git
write retry, push or release. PROJECT_STATUS remains IN_PROGRESS.

## 2026-11-06 13:21 JST — LK action checkpoint: drop result + SQL/API/Japanese viewer

Read the NEW10:49 coordinator result and current AGENTS/LK task. MSPT is genuinely
accepted; no MSPT redo or sandbox online-mode BDS retry. This checkpoint preserves
the coordinator's95FE...host-smoke candidate and does NOT assert a real player drop.
The quota/WindowsApps-pwsh errors recorded by the coordinator are historical;
no fast API retry loop, execution-policy override, security change or host-connector retry.

Implemented the first independent action slice: append-only ItemDropResult,
Player::$drop Lowest hook, pre-origin UUID/name/item/count snapshot, emit only on
nonrandom true return, origin exactly once, exception isolation, pinned pass-through
disable, weak capture/generation admission, bounded existing single-writer queue.
Schema4/migrations and success aggregates are unchanged. Returned true plus entry
stack quantity is explicitly NOT a verified inventory delta or durable item entity.
The reader supplies dropped_items/drop_actions SQL rankings and per-item top100,
JST day/month/all, selected player, positive-only/unknown-null, exact source/type/
integer-quantity guards. Existing death notifications/use attempts are also viewable
in the new Japanese /actions page. No AGPL source/comments/translations copied.

Existing web/index.html, app.js and styles.css were NOT edited. Added actions.html/
actions.js and new-process static routes; the old18767 Python process is untouched.
For a newly started authorized companion: http://127.0.0.1:18768/actions with its
OWN per-start random token. No new persistent viewer was started by this work.
Instructions/source semantics/reference links: docs/PLAYER_ACTIONS.md and
docs/ANALYTICS_DASHBOARD.md. This is not an in-DLL HTTP service.

Actual verification (not native action observations):

```powershell
scripts/verify_player_drop.ps1 #C++ boundary PASS, entry quantity, true/false/random, exceptions
scripts/verify_native_runtime.ps1 #fake SDK lifecycle/queue/SQLite PASS,155 durable events
scripts/verify_pinned_sdk_headers.ps1 #official22202 headers, four native objects PASS
scripts/verify_interval_runtime_import.ps1 #3 valid /12 rejected count/tick/drop exact symbols
scripts/build_server_candidate.ps1 #new artifact14ff...link/prelink/import/package PASS
python -W error::ResourceWarning -m unittest discover -s tests -p 'test_*.py' -v #45 PASS
node tests/analytics_dashboard_ui_tests.cjs #baseline DOM PASS
```

The new5 Python tests cover unit-vs-stack count, per-player/type, unknown/malformed
sources, JST boundaries, ties/pages, bounded types, schema1..4 read-only/hash stability,
actual ephemeral HTTP401/200 and new Japanese action DOM with literal hostile names,
units/unknown/selectors/logout. All45 tests were retained after the13:20 resume,
11.276s, ResourceWarnings-as-errors. Logs:
`.build/player-action-tests/run-877752b7477846a5a25eeb7509143f4b/`.
Native boundary logs: `.build/player-action-tests/run-67be9e8058b04234882f3f50ef34ecb0/`.
DOM checks are NOT browser screenshots or gameplay proof. Focused self-review in
docs/SECURITY_REVIEW.md; no independent model-identity/runtime certification claimed.

Build14ff root `.build/server-candidate/run-14ff7e8fa16e463981d3990016def6b5`:
DLL B1FCD1793084C9E64FE0498DCF2F8CA7834D9B1AB55CDF0F828FA0BE8E7B2DED,
ZIP22DA363CDA92F72D960BDC2FC61F82B1D1658A98F9E86D032DE395204DC92619.
This was built BEFORE the coordinator's10:49 update; no duplicate build/smoke after
that update. The authoritative host-smoke artifact remains302d.../DLL95FE... below.
At13:20 ALL current C++ source/header hashes match302d candidate-evidence.json;
its DLL hash independently matches95FE8E3B6E84FDE206DE8CD94F07C2416BB1CC4A2C74C74034B8B8944C40B45E.
Its PE imports include only the exact reviewed MCAPI drop/count/tick, plus the
official LL getTypeName export; SymbolProvider remains linked. No old client ABI.

| Feature | Hook/source | Implemented/compiled | Synthetic SQL/API/UI | Official BDS enable | Real action/SQLite/UI |
| --- | --- | --- | --- | --- | --- |
| Playtime/joins/leaves | Existing typed LL | Preserved | Regression PASS | Previously verified | Real136093ms session previously verified |
| Block changes | Pre-origin typed LL | Preserved **attempts** | Regression PASS | Existing capture | No success/material claim |
| Busy MSPT/TPS | Level tick entry/exit | Preserved | Regression PASS | PASS218A | Real6 windows/604ticks/API/DOM/stop accepted09:18 |
| Drop true-return units/actions/types | Exact Player::$drop + LL item accessor | PASS95FE/current source | PASS incl new/actions | PASS302d in new24156 root | **NOT OBSERVED / UNVERIFIED** |
| Death notifications/item-use attempts | Existing pre-origin typed LL | Preserved | Positive-only/null and new UI PASS | Candidate enables | No confirmed death/use success claim |
| Pickup/jump/sprint/sneak | Official publishers inspected; pre-origin/input semantics | Success capture pending | 未計測 | No per-action proof | Pending |
| Kill/craft/travel/item break | Need success/attribution/transition semantics | Pending | 未計測 | Pending | Pending |

Next useful work is the remaining independently verified action adapters, not an
empty smoke/MSPT rerun. Real drop acceptance needs a NEW normal-host isolated
allowlisted-player run, observed item quantities/failure/random exclusion, durable
SQLite->authenticated/actions->normal stop/integrity. Used24156/769103/b63e roots
are not reused/deleted. Do not update live BDS/18767.13:21 read-only check: PID748
and PID28268 retain prior starts; protected2bbb DLL remainsB286F054...D8A00.
No Git write attempted (.git remains read-only), no push/release, no other projects.

## 2026-11-06 10:49 JST — LK-Stats player-drop build + isolated BDS enable/exit0 VERIFIED (NEW TOP)

This new entry supersedes old instructions to rerun the completed MSPT acceptance. The coordinator independently performed targeted work while Codex has an ACTUAL usage limit. Codex attempt100 at 10:34 JST returned explicit `You've hit your usage limit ... try again at 12:36 PM`. Do not misdiagnose this as only supervisor heuristics or rerun quota-bound Codex repeatedly before reset. Attempt99 also had separate Windows Codex execution-sandbox `CreateProcessAsUserW failed: 5` for WindowsApps pwsh; normal self-hosted host commands work.

**New native player-drop feature:** Added `src/bedrock_insight/levilamina/player_actions.cpp/.hpp`, `core/drop_observation.hpp`, event kind `item_drop_result`, `Player::$drop` true-return hook with stack count/item type, exclude randomized drops, semantic caution: returned true is not full inventory-delta proof. Existing SQLite writer, Python companion `dropped_items`/`drop_actions`, category breakdown, Japanese UI and tests now exist. Targeted `python -m unittest discover -s tests -p test_drop_activity.py -v`: 5 tests PASS (fixture-only UI/API, not a real dropped item).

**Native candidate build PASS through authorized normal host:** `.build/server-candidate/run-302dfbce990d48f189a76d34729779b7/` DLL SHA256 `95FE8E3B6E84FDE206DE8CD94F07C2416BB1CC4A2C74C74034B8B8944C40B45E`; `candidate-evidence.json` and matching ZIP. Real official pinned LL26.60 headers compile + prelink + final DLL link; no matching SDK/general release guarantee. Build ran through `.build/host-drop-build-20261106.ps1`; Codex's WinApps `pwsh.exe` sandbox CreateProcess error was not encountered.

**Fresh real isolated BDS load/enable PASS:** first new root `run-769103cb5bdc4731a84fcc8d147d3724` actually loaded/enabled new DLL and exited0 but smoke runner misclassified because it searched obsolete hard-coded text `BedrockInsight enabled: join/leave and block attempt capture`. Fixed BOTH observed and final detection in `scripts/run_isolated_capture.ps1` to match stable `BedrockInsight enabled: join/leave`. Reran with FRESH root `.build/isolated-bds/run-24156b943d2e406cb58b93f71d08e09e`, ports 59700/59701, online auth unchanged, empty allowlist. Staging/preflight PASS, `ISOLATED CAPTURE LOAD PASS`; runtime evidence native_loaded=true, server_started=true, native_enabled=true, stop_reason=ready_hold_completed, exit_code=0, forced_termination=false, stop_command_issued=true, database_exists=true. Both roots are USED; NEVER reuse/delete/reset. No old BDS or 18767 viewer modified. **No real player/drop was performed**; do not claim item-drop event-to-DB runtime acceptance or a fully released DLL.

NEXT: When official Codex usage capacity recovers at indicated 12:36 PM local, continue player action features from `LK_STATS_INTEGRATION_TASK.md`, but do not needlessly rebuild the proven candidate or rerun empty smoke. For genuine drop semantics, use a NEW authorized isolated BDS with one explicitly allowlisted test player and test exact item-drop action, persistent SQLite+API+Japanese UI, then item pickup/jump/sprint/sneak/craft/kill/movement. Do not weaken offline Codex sandbox network rules, BDS auth or any PC policy; keep real host verification through self-hosted sub-PC. Do not GitHub push/release.

## 2026-11-06 09:18 JST — TRUE PROCESSING MSPT ACTUAL HOST ACCEPTANCE PASS (HISTORICAL; VERIFIED)

**The formerly BLOCKED measured MSPT acceptance has been COMPLETED by coordinator with authorized self-hosted Praxis Sub PC normal-host pathway.** This is a new confirmed result, superseding the older 08:45 / 08:38 "WAITING_FOR_COORDINATOR" lines below. The user requested execution; it was performed. The next work priority is **LK_STATS_INTEGRATION_TASK.md** (user-approved v0.13.0-reference action stats), NOT redoing MSPT or repeatedly awaiting host tests.

Actual NEW separate official BDS 1.26.60.6 / LeviLamina 26.60.0 scratch root:
`.build/isolated-bds/run-b63e53baf8634f9da2378dab8500b8a7`.
Never reuse or overwrite this used root. 59670/59671, online-mode=true, empty allowlist, no cheats, HTTP/rollback disabled. Existing protected BDS and running companion18767 were not stopped/reconfigured. Candidate root `.build/server-candidate/run-0388efb2490c4235b9cae627af0f7f60`, DLL SHA256 `218a8bc7d6a3646b317cb05d1cf8f2a2c9f62d1739fa1aa903a5adb99970cfbf`, matched with staging and run evidence. Isolation preflight PASS after fresh3395-item file verification.

**Real host runtime:** BDS loaded/enabled BedrockInsight, established Minecraft-services connectivity, ticked with corrected SymbolProvider delay resolver, persisted real busy `Level::$tick/Highest+Lowest` samples, and exited **normally exit_code=0**, `stop_command_issued=true`, `forced_termination=false`. Runner said `ISOLATED CAPTURE LOAD PASS`. BDS start 2026-11-06T00:17:25Z, stopped 00:18:12Z. Unlike the historical C8F crash and Codex offline-network failures, this corrected candidate actually ran and recorded ticks.

**Live genuine measured data**, from own scratch SQLite (not synthetic fixture): `.build/mspt-evidence/run-368cedb8114f48e9a84018d51b52835f/live.json`, captured during server run.
- `sampling_status=measured`, method `Level::$tick/Highest+Lowest`, `processing_mspt=0.709581`, `peak_mspt=1.4421`, `effective_tps=20.18`, `sample_count=101` within observed `window_ms=5004.9663`. 20.18 reflects actual tick count and elapsed wall time, not a clamped maximum.
- Loopback companion: anonymous HTTP401, authenticated HTTP200; actual Japanese JS DOM showed exactly MSPT `0.709581`, TPS `20.18`, online `0`, provenance text and measured-source timing. This was DOM verification with real JSON, not a browser screenshot. Own ephemeral loopback listener shut down. Existing port18767 never modified.
- Post-stop independent combined checker `scripts/verify_mspt_capture.ps1` PASS. Original durable DB unchanged, integrity and closed WAL/SHM checks, same activation START -> **six real 5s busy windows/604 actual ticks** -> END, normal exit0, live API/DOM matched persisted sample. `.build/mspt-evidence/run-08e18a3070204664948b80279e892819/acceptance.json`, `consistency.json` and private closed snapshot; `consistency_check_passed=true`; source DB sha256 `0F08F08E88B313EF4DE8490DC0717F1F368EF9F10F27B154DEF598D414ED4081`. No synthetic substitution or credentials.
- Evidence limitations: unloaded stopped sample is NOT currently running data, no production traffic/multiplayer load benchmark, browser screenshot not tested, no claim of general reliability under arbitrary load or a production release. Existing user-world deployment not performed.

**Next authorization:** Read `LK_STATS_INTEGRATION_TASK.md`, pinned LeafKnife/Stats v0.13.0 (LL26.60) as reference; independently implement missing player actions in BedrockInsight using verified LL APIs, preserving AGPL compliance, SQLite existing data and dashboard. Focus first on successful pickup/drop/jump/sprint/sneak, confirmed death/kill, craft, movement and item use, explicitly distinguish attempts and confirmed events. Incremental real isolated BDS checks required for every category. No GitHub push/release without user request. MSPT is accepted; do not repeat identical tests without a new regression.

## TRUE PROCESSING MSPT — network-audit boundary 2026-11-06 08:45 JST (HISTORICAL; SUPERSEDED)

Read CODEX_SANDBOX_NETWORK_AUDIT_2026-11-06.md in full, then the historical host
failure/MSPT task/current AGENTS. Three enabled outbound Block rules target
CodexSandboxOffline SID suffix1003, not ordinary host1001. The supplied normal-
host DNS/TCP443 check proves one endpoint, and old host BDS actually reached
services/enable. This strongly supports sandbox network confinement, but the
historical BDS child's token was not captured; do not claim token-proven causation.
No firewall/identity/proxy/sandbox/authentication change or new network probe.

Corrected218A...candidate is already built/reviewed; no resolver rework, rebuild
or offline-sandbox online-mode BDS retry. No new host root/result after fef29.
Real MSPT remains UNVERIFIED. The separate normal-host connector was already
denied by approval-policy-never at08:41; that denial was NOT retried or bypassed.
Need the coordinator's existing authorized normal-host pathway to execute
docs/MSPT_HOST_VALIDATION.md on a fresh world/ports and return live.json plus
post-stop acceptance/runtime evidence. LK-Stats remains queued until that gate.

Updated the host-validation Japanese instructions with this audit boundary.
No code/test/SDK/runtime changes this checkpoint, prior40-test result retained
without rerunning it as busywork. Read-only process check08:45: PID748 same start,
companion18767/PID28268 same06:30:51 start and127.0.0.1 LISTEN. No protected BDS,
viewer, old-world, other-project or security changes; no Git retry or push.

## TRUE PROCESSING MSPT — closed-run acceptance gate 2026-11-06 08:38 JST (NEW TOP)

Read **HOST_MSPT_RUNTIME_FINDING_2026-11-06.md FIRST**, then full MSPT task,
AGENTS/dashboard task and current status. The host note describes the same
08:06 C8F... crash addressed by the preceding218A...SymbolProvider link fix.
No new host-run result has appeared. Do NOT redo that fix/download/build, retry
unchanged child-route launches, reuse used roots or start LK-Stats yet.

**08:41 JST resume / explicit host-connector boundary:** re-read the host finding,
full MSPT task, AGENTS/official inputs and latest evidence. No new isolated root
or host result exists after fef29; candidate218A... and40-test checkpoint remain.
Discovered the provided Praxis Sub PC normal-host terminal connector and tried
ONLY a read-only project-path/DLL-hash/process/listener identity check. The tool
rejected it with **"MCP tool call requires approval, but approval policy is never"**.
No host process/command was started. Do not bypass this denial with other tools,
change approvals/proxy/auth/security, or label the earlier services issue universal.
This Codex session cannot carry out the authorized normal-host validation itself;
the coordinator must execute docs/MSPT_HOST_VALIDATION.md externally and provide
the fresh live.json + final acceptance/runtime evidence. No redundant rebuild,
test rerun, new BDS launch, old-root write, Git write or LK activity expansion
was made on this resume. Local read-only check confirmed protected PID748 and
companion18767/PID28268 still running. Real MSPT remains UNVERIFIED.

HOST_VALIDATION_STATUS: WAITING_FOR_COORDINATOR
HOST_VALIDATION_LAST_CHECKED: 2026-11-06 08:50 JST — no new isolated root after fef29,
no acceptance.json, and no live.json newer than the existing08:38 fixture.
PID748 and companion PID28268 retain their recorded start times. No approval
retry, rebuild, test rerun or BDS launch on this resume.

This checkpoint implements the missing **combined post-stop acceptance check**,
not real MSPT acceptance. `scripts/verify_mspt_capture.ps1` validates fresh-root/
candidate/runtime hashes, rejects still-running/protected roots and non-normal
exit, holds a read-only source DB lock and copies only a closed DB to a new owned
evidence folder. `companion/mspt_acceptance.py` checks the private copy read-only:
schema4/integrity/foreign keys, same activation start->at least2 real tick windows
->end, no invalid/restart/unknown/frozen source, runtime time bounds, same-source
live401/200 API values and Japanese DOM output matching a persisted busy window.
It never publishes a stopped historical sample as current. This is consistency
of supplied evidence, NOT independent attestation against edited files.

`telemetry.parse_busy_observation` is shared by existing latest-lifecycle live
reads and this historical checker; existing end/start/invalid/stale behavior is
preserved. `verify_ticks` now uses explicit gates (not removable python asserts),
records actual HTTP status codes and emits successful JSON only AFTER its own
ephemeral listener/thread is closed. --wait-seconds is bounded1..180 to cover
host startup; it never starts/stops any BDS. Japanese DOM check now accepts
unknown online/null as **未計測**, without discarding genuine MSPT/TPS.
No HTML/JS/CSS asset or server18767 restart/update.

Focused requested Sol/xhigh review `01a1ae0c-b1e5-7632-af2c-b42c9fdbb92d` found
**HIGH**: PowerShell `@($db+'-wal',$db+'-shm')` concatenates into one path, so the
sidecar check could miss source WAL/SHM. **FIXED** using a shared suffix-by-suffix
`validate_closed_sqlite.ps1` guard under the source lock, before/after copy and
before PASS. Identical bug in the existing real-player verifier fixed too; that
verifier now queries its private closed copy, never original DB reader-sidecars.
Marker regression covers no sidecars/WAL-only/SHM-only/both and wrapper ordering.
Final delta review: HIGH remediated, no residual HIGH/CRITICAL. Model/effort
identity not independently attestable; static review, not runtime certification.

Verification actually executed:

```powershell
python -W error::ResourceWarning -m unittest discover -s tests -p 'test_*.py' -v #40 PASS
python -O -W error::ResourceWarning -m unittest discover -s tests -p test_mspt_acceptance.py -v #6 PASS, gates remain active
node tests/analytics_dashboard_ui_tests.cjs #PASS
scripts/verify_closed_sqlite.ps1 #1 absent +3 independent sidecar rejects +2 wrapper order checks PASS
python -m py_compile companion/telemetry.py companion/mspt_acceptance.py companion/verify_ticks.py
```

New6 tests cover correct two-window evidence, C06D007E/services exit3/forced stop,
wrong types, interrupted/mixed/malformed/one-window data, wrong/stale/missing
API/DOM proof, unknown online and actual CLI on a closed PRIVATE FIXTURE copy
with unchanged DB hash/no WAL/SHM. Fixtures are NOT native observations.
PowerShell wrapper additionally rejected the protected2bbb root and the actual
failed9d84/fef29 roots without copying any DB or starting a server. These are
negative tests, not a successful full-wrapper runtime acceptance.

Logs/evidence: `.build/mspt-evidence/run-acceptance-gate-20261106/`;
sidecar fixtures `.build/closed-sqlite-tests/run-a003a591e0b748afa6659043bcdb310e/`.
New checker fixture DBs are clearly marked under mspt-evidence/fixtures; older
checker-test fixture DBs may be directly under mspt-evidence/run-* (named
fixture-closed.sqlite3, synthetic run-eeee source). NEVER cite them as native proof.

| Current gate | Status |
| --- | --- |
| Native218A...candidate | Unchanged; all native source/header hashes rechecked against its build evidence |
| Resolver repair / build | Prior PASS; no redundant rebuild, no live DLL replacement |
| Combined durability/API/DOM checker |40 Python tests + marker/negative wrapper tests PASS, fixtures only |
| Genuine MSPT | STILL UNVERIFIED: corrected trialfef29 failed services before enable; old host9d84 crashed using oldC8F |
| New actual host run | Not supplied yet; no BDS launched this checkpoint |
| Protected state at08:36 JST | PID748 same start;18767 PID28268 (06:30:51 start) still127.0.0.1 LISTEN; no writes/restarts |

NEXT EXTERNAL ACTION: coordinator's normal authorized host route must run
[docs/MSPT_HOST_VALIDATION.md](docs/MSPT_HOST_VALIDATION.md) with corrected
`.build/server-candidate/run-0388efb2490c4235b9cae627af0f7f60` and NEW world/ports.
Capture live.json while alive (`--require-busy --ui-dom`), then run the new
post-stop verifier. Need two genuine busy windows + API/DOM + normal exit0/end/
integrity before MSPT PASS and the approved LK-Stats phase. No auth/network/
security change or bypass is proposed. No response/result from host is assumed.
Original DLL218A8BC7D6A3646B317CB05D1CF8F2A2C9F62D1739FA1AA903A5ADB99970CFBF,
ZIP12FBAD4D2E7C9DB0D31CA5267583062C80CDFDE4F9B6C5223C8097A7F8CD6DC0;
original protected DLLB286... remains unchanged. No blocked Git write retries or push.

## TRUE PROCESSING MSPT — host crash diagnosed / resolver fixed 2026-11-06 08:20 JST (NEW TOP)

Host/coordinator trial newly found at `.build/isolated-bds/run-9d84e900b55a4b199ff9994fe497b12d`
(59640/59641, PID36528) reached LOAD/ENABLE/Server started using C8F24...,
then exited **0xC06D007E (-1066598274)** at 08:06:14 JST, approximately first
5-second sample. No stop command/forced termination. NOT a services failure:
services connected, sampler runtime acceptance FAILED. Read-only SQLite inspection
integrity=ok/schema4; only busy/interval START markers, NO genuine samples.
Do not restart/reset/delete that used root or deploy C8F24... to any server.

Root cause strongly supported by link inspection + standalone reproduction:
the manual candidate used regular `delayimp.lib`, which tries to LoadLibrary
the virtual `bedrock_runtime.dll` on first MCAPI invocation. LL26.60 actually
depends on **SymbolProvider v1.3.0**, source commit
`6c93ec45c8455992ee726d92df60316c8e731c44`, which resolves that module through
LL's exported `resolveSymbol`. Corrected build uses its pinned cached x64/MD/
clang-cl static library, owned prelink copy, `/NODEFAULTLIB:delayimp.lib`.
No guessed addresses/offsets, fake runtime DLL, proxy/auth/security changes.

Corrected candidate `.build/server-candidate/run-0388efb2490c4235b9cae627af0f7f60/`:
DLL SHA256 **218a8bc7d6a3646b317cb05d1cf8f2a2c9f62d1739fa1aa903a5adb99970cfbf**.
28 LL imports resolved (includes resolveSymbol), exact old count/tick delay API
whitelist unchanged. C++ capture/timer sources unchanged. This is BUILD evidence,
not yet runtime PASS. Final ZIP (includes updated SymbolProvider/Pier notices)
SHA256 **12fbad4d2e7c9db0d31ca5267583062c80cdfde4f9b6c5223c8097a7f8cd6dc0**.
Pre-notice ZIP preserved; repack history recorded in candidate-evidence.json.

`scripts/verify_runtime_delay_resolution.ps1` PASS using the real cached static
helper and a standalone synthetic-import EXE (NOT BDS): CRT preloads one import,
two calls return42, resolves once, no bedrock_runtime.dll loaded. Evidence
`.build/runtime-delay-tests/run-7b8029f6cb184a469d385de6a8d5de33/`.
Negative control with regular MSVC delayimp catches **the same C06D007E**;
it is a standalone EXE with a synthetic import, NOT a server or crash dialog.
This strongly explains the host failure, but no crash stack/dump was available;
do not claim an engine-stack-proven diagnosis or a tested runtime repair.
`scripts/verify_capture_isolation.ps1`:18 candidate +3 loader rejections PASS.
New launch policy rejects old candidates lacking reviewed resolver metadata;
staging now fails before extracting one. Existing already-used roots unchanged.
Genuine MSPT gate remains **UNFINISHED**; LK-Stats work stays queued.

Focused read-only delta audit requested gpt-6.1-sol/xhigh, reviewer
`01a1adfa-f717-7d12-baf0-6c86d946b356`: no HIGH/CRITICAL findings. Independently
checked all11 original/prelink-copy hashes, resolver ABI/export, final DLL hash,
early staging/launch gates. No selectable model/effort attestation was exposed;
this is static review, not model-identity/runtime safety certification.

One corrected-candidate trial after audit/package/preflight, **08:16:27–08:17:45 JST**:
`.build/isolated-bds/run-fef29fe40a414419bdeb27b0a1cd397d`, PID27844,
59660/59661, full3395-file fresh isolation PASS, online-mode=true/empty allowlist,
HTTP/rollback/cheats disabled. DLL218A... LOAD confirmed; signaling signed in,
then Minecraft-services connection failed **before enable**, self-exit3,
stop_command_issued=false/forced_termination=false/database_exists=false.
No live native samples/API/UI/normal-stop acceptance; verifier timed out honestly.
This trial cannot exercise the resolved MCAPI calls/hook timer and does not
establish whether the host crash is fixed in BDS. Do NOT relaunch/reset/delete
either used root or repeat the unchanged child-route attempt.

The new `companion.verify_ticks --require-busy --ui-dom` can validate actual
fresh persisted native rows through anonymous401/authenticated200, then render
that response using the unchanged Japanese HTML/JS. `native_dashboard_ui_check.cjs`
is a DOM test, NOT a browser screenshot or proof that18767 runs new code.
It prints no viewer credential and stops only its own ephemeral HTTP listener.
Fixture regression now exercises this full HTTP->Japanese DOM path (2MSPT/20TPS).
Actual server values are still unavailable; no fixture substituted for real data.

Final regression: **34 Python tests PASS** with ResourceWarnings-as-errors,
DOM suite PASS, py_compile PASS. Logs/verification metadata:
`.build/mspt-evidence/run-resolver-fix-20261106/`.
Source native math/queue code unchanged, prior math/drain154 proof remains valid;
new DLL is recompiled from pinned22,202 headers/prelink/server imports, not core-only.

| Current gate | Evidence |
| --- | --- |
| Native build + resolver | PASS218A...DLL,28 LL names, synthetic positive/negative controls |
| Persistence / authenticated Japanese UI | PASS fixtures only; no genuine busy samples yet |
| Corrected actual BDS | LOAD only; services fail pre-enable, real MSPT/TPS still UNVERIFIED |
| Protected state | PID748 same start;18767 PID28268 still loopback LISTEN; old DLL B286... unchanged |
| Next action | Host/coordinator normal-route fresh trial; do not start LK action phase yet |

Exact commands executed with cached compiler/packages (same paths as prior entry):

```powershell
scripts/verify_runtime_delay_resolution.ps1
scripts/verify_capture_isolation.ps1
scripts/build_server_candidate.ps1
python -W error::ResourceWarning -m unittest discover -s tests -p 'test_*.py' -v
node tests/analytics_dashboard_ui_tests.cjs
python -m py_compile companion/verify_ticks.py companion/telemetry.py companion/analytics.py companion/server.py
#Used root below; commands are historical evidence, NOT a rerun instruction:
scripts/run_isolated_capture.ps1 -SandboxRoot .build/isolated-bds/run-fef29fe40a414419bdeb27b0a1cd397d -StartupTimeoutSeconds 90 -SmokeHoldSeconds 30
python -m companion.verify_ticks --database .build/isolated-bds/run-fef29fe40a414419bdeb27b0a1cd397d/mods/BedrockInsight/data/bedrock_insight.sqlite3 --wait-seconds 90 --require-busy --ui-dom
```

NEXT: async host-route request sent with corrected218A candidate. No response
assumed. A normal host/coordinator should use the new commands below with online
auth/security unchanged. NEVER reuse root9d84/fef29/732045/16b505/2bbb60.

```powershell
$env:BEDROCKINSIGHT_CLANGCL_ROOT='C:\Dev\LL2660Toolchain-20261029\clang+llvm-23.1.4-x86_64-pc-windows-msvc\bin'
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$stage = scripts/stage_isolated_capture.ps1 -CandidateRoot .build/server-candidate/run-0388efb2490c4235b9cae627af0f7f60 -ServerPort 59670 -ServerPortV6 59671 -PassThru
scripts/prepare_capture_executable.ps1 -SandboxRoot $stage.sandbox_root
scripts/check_capture_isolation.ps1 -SandboxRoot $stage.sandbox_root -RequirePatched
#Only after full PASS and ports still unused; runner checks again:
scripts/run_isolated_capture.ps1 -SandboxRoot $stage.sandbox_root -StartupTimeoutSeconds 90 -SmokeHoldSeconds 30
#Concurrent second terminal, substitute the NEW root only:
python -m companion.verify_ticks --database <NEW_ROOT>/mods/BedrockInsight/data/bedrock_insight.sqlite3 --wait-seconds 90 --require-busy --ui-dom
```

Need real fresh busy rows (prefer >=2 windows), authenticated API/Japanese DOM,
normal stop/exit0, busy end marker and closed DB integrity=ok before MSPT PASS.
Any future separate viewer must use a new127.0.0.1 listener (e.g.18768), not
restart18767. No Git write/repeated blocked commit/push, live DLL/old world,
other-project, game UI, proxy/firewall/credentials/security modifications.

## TRUE PROCESSING MSPT — implementation checkpoint 2026-11-06 (NEW TOP)

Read MSPT_IMPLEMENTATION_TASK.md in full after the user override. This supersedes
older "MSPT always null by design" statements. Real processing timing is now
implemented/compiled; **genuine MSPT runtime acceptance is NOT PASSED**.
Existing DLLs and companion18767 were not hot-updated, stopped or restarted.

| Acceptance | Actual evidence / boundary |
| --- | --- |
| Native source/build | PASS pinned Level::$tick MCAPI, error-checked Highest/Lowest hooks, optimized DLL/prelink/package, 27 LL imports resolved |
| Busy math | PASS C++ idle20TPS/2MSPT, lag10TPS/80MSPT, warp60TPS/2MSPT, freeze0TPS/undefinedMSPT, clock/Level/exception/bounds/pause cases |
| SQLite persistence | PASS fake-SDK adapter -> bounded queue -> real SQLite -> drain154 records; not real engine samples |
| Companion/API/UI | PASS fresh persisted fixtures -> authenticated actual loopback HTTP, existing Japanese MSPT card shows2; start/end/invalid/stale/freeze null states; 34 Python tests |
| Real BDS | DLL LOAD confirmed, but NO enable/ready/ticks/DB: Minecraft-services failure, owned child self-exited3; no forced kill |
| Protected state | BDS748 unchanged/running; old PID24864 already absent before this trial, not stopped/restarted by us; original DLL hash B286... unchanged;18767 still listening, no process/token/static-asset update |

Source: `core/busy_tick.hpp`, `levilamina/busy_tick.hpp/.cpp`, Runtime integration,
`companion/telemetry.py`, tests. Studied actual Pier TickStats.cpp and LICENSE at
**f841e43a251444b326557e4e225574d0f65663f3** through the official GitHub read connector:
https://github.com/Maskviva/Pier/blob/f841e43a251444b326557e4e225574d0f65663f3/packages/pier-hooks/src/engine/TickStats.cpp
Apache-2.0 attribution/text in THIRD_PARTY_NOTICES.md and licenses/Pier-Apache-2.0.txt,
included in the final candidate ZIP. No Pier host API or other-project dependency.

Definition: steady-clock elapsed time around Highest hook origin divided by
actual Lowest tick executions; not mTickDeltaTime/50ms idle frame period/CPU
utilization or CPU cycles. Includes Level/inner-Mod processing and blocking,
excludes external scheduler sleep. RAII calls engine origin exactly once in each
of OUR hooks and preserves original exceptions; lower hooks can freeze/warp.
Maximum is actual innermost tick duration, explicitly a different scope from the
outer mean. p95 remains null. Busy-source TPS has NO20 clamp: warp60 stays60.
Frozen observed frames with zero actual ticks have0TPS and undefined/null MSPT.

Fixed-memory/O(1) disjoint windows >=5s; end-aligned wall deltas, max30s and bounded
counts. No per-tick allocation/SQLite/sorting. Short ownership/sampler locks never
span origin or the sink. Multi-Level/thread, recursion, exception and bad clocks
invalidate the source. Manual hook installation checks return codes; module is
pinned first. Successful hooks remain mapped/pass-through after disable rather
than unsafe detach-time unhook. In-flight RAII owns state; sink holds weak Capture
+ generation. Disable ends admission, queues lifecycle markers and drains worker.

Schema v4 unchanged: System/ServerTick cause `ll26.60.busytick.v1`, amount=actual
tick count, session_id=activation ID, online_players=observed count; details JSON
clock=steady, hook=Level::$tick/Highest+Lowest, wall_ns, busy_ns, max_inner_ns,
frames, sampled_peak_online. Legacy integer tick_mspt stays0; no precision-losing
legacy table reuse. busytick.start/end/invalid.v1 is authoritative over intervals.
Reader rejects unsupported/future/malformed rows without old-row fallback. >20s
stale/start/end/invalid data supplies no current MSPT/TPS/online. Existing UI reads
processing_mspt and the reason includes method/window/inner maximum; no web asset
changed and already-imported18767 remains old code in memory.

Focused audit requested gpt-6.1-sol/xhigh, reviewer
`01a1ade4-55c3-75d3-9ce1-5fa7a66f7e19`. No HIGH/CRITICAL reported. P2 found: a
prepared valid report could overtake another thread's invalid marker. **FIXED** by
sticky Capture::busy_invalidated under existing Capture mutex, reset only for a
new generation. Test valid -> invalid -> delayed valid asserts one sample, one
invalid marker and154 drained records; PASS. Final delta review: P2 resolved, no
new severe findings. Reviewer states model/effort selection was not attested;
this is direct static review, NOT model-identity/runtime certification. Earlier
review request `01a1adbb-1483-7a43-a203-3c77992e0afe` hit usage quota and yielded
NO audit; do not count it as completed.

Also fixed activation-ID allocation escaping noexcept enable(). Test-only global
new failure now returns false without worker/hooks/data directory, not BDS
termination. That allocator override is never linked into the mod. Hook-install
failure seam proves worker cleanup. New starts hide crash-left samples during
warmup; end markers include empty runs. No synthetic player disconnects.

FINAL optimized server candidate:
`.build/server-candidate/run-944f9b89a0834d458bb8eadb9a3f5a5a/`

- DLL SHA256 **c8f24edf1527b79cebdc88811f4edfe98d6d0ac17ae2dd3b062407ee04cfa19d**
- ZIP SHA256 **66f765ccae0df2bafe1e80f465a08f6000f307c5f33b7149b7d2dfcd4536075d**
- 22,202 pinned headers, actual official LL26.60 functions; prelink0.8.8/runtime-
  data26.60.6-server.5. Old client imports FALSE. 27 LL imports include hookEx/unhook.
- Delay policy only allows known getActivePlayerCount and optional tick MCAPI
  symbols (2 valid/10 rejection tests), not arbitrary engine symbols.
- First busy link introduced MSVCP140_ATOMIC_WAIT.dll via atomic<shared_ptr>;
  static policy rejected it. Replaced with short ownership mutex; final imports
  have no new CRT dependency. No broad whitelist bypass.
- Intermediate `run-a5cc59fbfb00439d80914f3864b20cfc` /2BCA... superseded by P2 fix,
  not final runtime evidence. Its ZIP hash was updated after notice packaging;
  final build includes notices directly. Normal complete SDK/release certificate
  remains FALSE; no fake xmake/ABI acceptance.

REAL NEW trial **07:55:25–07:56:43 JST**:
`.build/isolated-bds/run-732045cc4e8d4d72963de4bcc3e16f68`, owned PID45008,
ports59612/59613, unique fresh world, empty allowlist/max1 visitor, online-mode=true,
cheats/HTTP/rollback disabled. Original full candidate isolation PASS3395 files,
after completed audit and patched-executable check. No used world reused.
Logs confirm official BDS1.26.60.6/LL26.60.0 LOAD of new DLL; then signaling sign-in,
"Waiting for Minecraft services...", **"Could not connect to Minecraft services.
This is required to accept connections in online mode."** Child self-exited3 at
~76s before90s deadline; stop_command_issued=false, forced_termination=false,
native_enabled/server_started/database_exists=false. NO real MSPT/tick execution
proven. `companion.verify_ticks --require-busy` timed out without fabricated data.
Same pre-enable boundary as the earlier Codex-child trial, not proof of sampler
fault or proxy cause. Earlier host-side baseline and real joins did succeed, but
did NOT exercise this new timer. Do not repeat unchanged launches or relax auth/
firewall/proxy. New root/evidence and all old used roots/worlds retained.

Evidence: new root capture-runtime-evidence.json + stdout/stderr/staging/patch;
final candidate candidate-evidence.json + compile/prelink/link/PE/source hashes.
Tests and metadata: `.build/mspt-evidence/run-05bfbb7aebc74fe0a4d3f77ef3b12f46/`.
Math: `.build/busy-tick-tests/run-fb439b1cd5f543f788808bcc083ceb09/`.
Actual verification (same cached compiler/SQLite env paths as06:52 entry):

```powershell
scripts/verify_busy_tick.ps1                    # busy math PASS
scripts/verify_native_runtime.ps1               #154-record fake SDK/OOM/install failure/drain PASS
scripts/verify_interval_runtime_import.ps1       #2 valid/10 rejected PASS
scripts/verify_server_release.ps1               #23 rejects PASS
scripts/verify_capture_telemetry_mode.ps1        #6 rejects PASS
scripts/verify_capture_isolation.ps1            #12+3 rejects PASS
scripts/verify_capture_player_mode.ps1          #28+8+7,14 cases PASS
python -W error::ResourceWarning -m unittest discover -s tests -p 'test_*.py' -v
#34 PASS = original18 + interval8 + busy5 + notification3
python -m py_compile companion/analytics.py companion/telemetry.py companion/server.py companion/verify_ticks.py
node tests/analytics_dashboard_ui_tests.cjs      #2MSPT, then stale/unavailable PASS
scripts/build_server_candidate.ps1             #final C8F... DLL/ZIP PASS
#Already executed concurrently; NEVER reuse this USED root:
scripts/run_isolated_capture.ps1 -SandboxRoot .build/isolated-bds/run-732045cc4e8d4d72963de4bcc3e16f68 -StartupTimeoutSeconds 90 -SmokeHoldSeconds 30
python -m companion.verify_ticks --database .build/isolated-bds/run-732045cc4e8d4d72963de4bcc3e16f68/mods/BedrockInsight/data/bedrock_insight.sqlite3 --wait-seconds 90 --require-busy
#Runtime FAIL pre-enable services; verifier timeout, NOT MSPT PASS.
```

One PS5 logging wrapper treated unittest's normal verbose stderr as ErrorAction
Stop and aborted. Used existing Invoke-BdsLoggedNative capture helper instead;
final34 PASS with ResourceWarnings-as-errors, no hidden test failure. Earlier
process-output handles did not survive new user-priority messages; unaccepted
intermediate artifacts are preserved and not cited as build proof.

Pre-MSPT activity delta retained, not expanded: distinct pre-origin death
notification/item-use attempt kinds, no old success-counter inflation. New API
positive counts/rankings; absent record=null/unknown, never measured0. Native
runtime UNVERIFIED; protected old DLL still only captures earlier scope. Planned
extra UI variant abandoned before creation; default/static assets unchanged.

NEXT EXTERNAL COORDINATION: a normal host/coordinator must run one NEW fully
checked stage with final C8F... candidate and online auth unchanged. Async request
sent; no response assumed. Do not restart/modify protected BDS or18767. Example:

```powershell
$env:BEDROCKINSIGHT_CLANGCL_ROOT='C:\Dev\LL2660Toolchain-20261029\clang+llvm-23.1.4-x86_64-pc-windows-msvc\bin'
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$stage = scripts/stage_isolated_capture.ps1 -CandidateRoot .build/server-candidate/run-944f9b89a0834d458bb8eadb9a3f5a5a -ServerPort 59622 -ServerPortV6 59623 -PassThru
scripts/prepare_capture_executable.ps1 -SandboxRoot $stage.sandbox_root
scripts/check_capture_isolation.ps1 -SandboxRoot $stage.sandbox_root -RequirePatched
#Only if all gates pass; choose other unused distinct high ports if necessary:
scripts/run_isolated_capture.ps1 -SandboxRoot $stage.sandbox_root -StartupTimeoutSeconds 90 -SmokeHoldSeconds 30
#Concurrent second terminal, substitute that NEW stage path:
python -m companion.verify_ticks --database <NEW_STAGE>/mods/BedrockInsight/data/bedrock_insight.sqlite3 --wait-seconds 90 --require-busy
```

Need real fresh busy rows/API plus normal exit0/integrity/end-state evidence.
View new real samples with a NEW authenticated loopback companion18768 against
that approved DB while WAL exists; do not stop/restart18767. Original PID24864
already absent before our trial; no restart/files change. BDS748/original DLL
unchanged. No Git write/blocked commit retry/push, other-project, game UI or
security/network/credential changes. Full SPEC remains IN_PROGRESS, not COMPLETE.

## New user priority 2026-11-06 07:03 JST: true processing MSPT via verified Pier algorithm

REQUEST: user explicitly requests REAL processing MSPT, based on a working Bedrock/LeviLamina Mod, not misleading tick delta time. Reference verified on GitHub: `Maskviva/Pier` Apache-2.0, exact revision `f841e43a251444b326557e4e225574d0f65663f3`, `packages/pier-hooks/src/engine/TickStats.cpp`. It wraps actual `Level::$tick` origin with priority detours to measure busy time excluding idle sleep, counts actual ticks for freeze/warp, and computes MSPT = sum busyNs / real ticks / 1e6. Our earlier `ServerLevelTickEvent` pre-origin hook cannot measure processing time alone.

PREPARED_FOR_CODEX: `MSPT_IMPLEMENTATION_TASK.md` full scoped specification with explicit source URL, validated semantics, acceptance matrix, pinned 26.60 ABI, 5-second persistence, SQLite provenance, Japanese authenticated companion UI, safety, real isolated smoke validation and status/doc obligations. Updated `AGENTS.md` and `ANALYTICS_DASHBOARD_TASK.md` to make this the highest priority. User requests Codex to implement. No new native source build or BDS test from this preparation. Earlier phase-2 interval candidate exists but processing MSPT is NOT YET IMPLEMENTED/VERIFIED.

CODEX_LIMIT: attempt 87 ended with actual usage limit after 07:02 JST and explicitly indicated retry at 07:21 AM Japan time; supervisor logged it will retry after 35 minutes (~07:37). Do NOT claim that Codex is presently coding while this quota pause persists. Do NOT disturb the existing two BDS processes or existing 127.0.0.1:18767 companion.

## Analytics Dashboard User Request 2026-11-06

2026-11-07最新追補はSTATUS最上段：種類別killed_by／子飛び道具の独立ソース、
200種類打ち切り撤去、GUI／4関数APIページング、例外境界、108 Python／C++保存fixture。
真のMSPTは受入済みのまま再試験しません。全9／77の実プレイ受入は未完了。


2026-11-07 continuation: current native reader/GUI fixes, 103Python tests, C++
reader/API/page/77-label fixtures, per-key96-row gate inventory, initial PE gate
failure and exact reviewed MCAPI follow-up are recorded in the NEW TOP section.
The full9-category/77-key/3GUI-command/4API mandate remains IN_PROGRESS; partial
adapters and old empty-smoke evidence are not full acceptance.

18:52 update: exact craft API probe (no shipped counter), fixed runner/verifier
window mismatch, and raw action/operator/API/actual-Japanese-JS acceptance checker
with separate JST day/month/all calculations.85 Python/10 optimized +PS policy
and one-object probe PASS. See NEW TOP. F9D1 unchanged; NO real new action PASS.

15:17 C6 update: use true-return + same-inventory snapshots, separate response/
net-decrease SQL metrics and Japanese/actions.75 Python + F9D1 native10-unit
compile/prelink/link/package/import/hash PASS. See NEW TOP. Latest candidate and
new action→SQLite/UI real acceptance NOT RUN; craft/travel/break still unfinished.

14:54 C3 update: direct matching-source Player credit plus same-victim dead flag,
Mob/Player/type SQL rankings and Japanese/actions.70 Python +E1949-unit native
compile/prelink/link/package/import/hash PASS. See NEW TOP. Native actions and
new candidate runtime acceptance remain NOT RUN; no protected deployment.

14:36 jump update: grounded same-Player direct PosDelta.y upward impulse, SQL
period rankings and Japanese/actions.65 Python tests +7AA2 candidate8-unit
compile/prelink/link/package/import/hash PASS. See NEW TOP. New candidate and
all new action->SQLite/UI runtime acceptance remain untested; no live deployment.

14:16 pickup update: true-return take plus bounded same-main-inventory net type
increase, item/unit/action SQL rankings and Japanese/actions.60 Python tests +
5FC0 candidate7-unit compile/prelink/link/package/import/hash PASS. See NEW TOP.
This candidate and actual new action->SQLite/UI remain untested in BDS; no deployment.

14:00 LK B update: bounded delayed sprint/sneak flag observations,5s/JST batches,
four period-ranking metrics and Japanese/actions.55 Python tests +01FC candidate
6-unit compile/link PASS; source/hash/import audit PASS. See NEW TOP. New candidate
BDS enable and actual player-action/SQLite/UI remain unverified, no live deployment.

13:37 C1 update: post-origin dead-flag source, cause breakdown/SQL rankings and
Japanese/actions added;50 tests +80BD candidate compile/link PASS. See NEW TOP.
New candidate/runtime actions not yet observed;95FE smoke is historical only.

13:21 action checkpoint: see NEW TOP and docs/PLAYER_ACTIONS.md for drop-return
units/type rankings and the separate/actions UI.45-test + native build/smoke
boundaries are explicit; actual player drop and the remaining LK categories pending.

Latest true-MSPT acceptance is in the NEW TOP section; older null-by-design
statements below are historical and do not describe current source policy.
08:20 update: corrected218A...native candidate/SymbolProvider fix and genuine
runtime boundary are in the new top entry; no new DLL was staged to live servers.
08:38 update: combined closed-run checker and40-test checkpoint are at the new
top; genuine host acceptance remains pending, not supplanted by fixture PASS.

### 2026-11-06 06:52 JST — native interval candidate built; new isolated load verified, service failure before enable

Phase 1 companion is preserved, NOT rebuilt/discarded. The coordinating process
started a separate companion on **127.0.0.1:18767** after the 06:24 entry; it is
still listening and was NOT stopped/reconfigured. Its already-imported Python
code stays Phase 1; no web asset changes in this continuation. New provider code
applies to a future authorized new companion process, not hot-patching that one.

| Category | Existing running user interface / DLL | New Phase 2 implementation and verification |
| --- | --- | --- |
| Playtime | Phase 1 durable session/JST interface preserved | Existing overlap/crash/boundary tests still PASS |
| Activity | Joins/leaves + block **attempts** only | No new gameplay hooks; death/use/kill/craft/distance remain 未計測 |
| Rankings | SQL day/month/all supported counters preserved | Deterministic/positive-only/auth/escaping regression tests PASS |
| Effective TPS / online | Existing DLL has no sampler; still 未計測 | Native 5s sampler and matching SQLite/API/UI read path implemented; measured/stale HTTP+DOM fixtures PASS; REAL tick execution NOT yet verified |
| Processing MSPT | 未計測 | Remains null: callback intervals are NOT processing time |

Official publisher source fetched through the supported GitHub read connector at
fixed commit **072b9ffe0e9d9e0ec846f11d294880e892fe7770**:
`src/ll/api/event/world/ServerLevelTickEvent.cpp` publishes BEFORE `origin()` in
`ServerLevel::$_subTick`, not a dimension-tick loop. Local evidence copy:
`.build/tick-publisher-inputs/ServerLevelTickEvent.cpp`, SHA256
`6dee31091e9daec87011a0cd3bcf5f472a9fef35873144c548e1f5e6d82387b6`.
This verifies publisher semantics, not observed once-per-game-tick runtime cadence.

Implemented header-only `core/tick_interval.hpp` and real `runtime.cpp` typed
listener. `steady_clock` interval count / elapsed microseconds, >=5s windows,
upper TPS clamp 20, initial warmup, duplicate/backward timestamps ignored,
>30s pause windows discarded. Mixed Level identities invalidate the activation.
Constant-size/O(1) per-tick state, no per-tick allocation/DB write; one count read
via official **Level::$getActivePlayerCount MCAPI** and immutable queue submission
per window. Existing single-writer SQLite persists the event. Runtime lifetime,
generation gating, unload pin and queue admission remain in place.

No schema bump/migration rewrite: v4 `events` stores known System/ServerTick cause
`ll26.60.interval.v1`, amount=interval count, session_id=activation ID, details JSON
clock=steady/window_us/sampled_peak_online, online_players=window-end count.
tick_mspt stays ZERO; no interval telemetry enters legacy server_tick_samples.
Clean disable/mixed-Level invalidation queues `ll26.60.interval.end.v1` before
drain. Rejection/crash can lose an end marker; a 20s freshness bound still applies.
Companion validates the latest durable sequence, provenance/typed bounds/window/
clock/time, never falls back behind invalid/future/end records. Stale/stopped/
invalid rows do NOT supply current TPS/online. Sampled peak is explicitly a 5s
observation maximum, not exact peak or per-player online. UI's existing numeric/
unavailable/reason fields show these values without changing served assets.

Actual server-derived x64 optimized DLL/package build PASS:

- Candidate: `.build/server-candidate/run-0bd09308338145d49f50587e55e0aae6/`
- `BedrockInsight.dll` SHA256 **b69b47dafd51591a6782424097d4b9ff9b1ad8f4d12358adda6a4f46393ddd20**
- `BedrockInsight-server-candidate.zip` SHA256
  `30936b835608d142149030dbe4da4b8e43eab0983a893420e7f21c133eb3f5d3`.
- 22,202 pinned headers revalidated; **25** LL named imports resolved against
  actual official server DLL. Matched prelink0.8.8 / runtime-data26.60.6-server.5.
  No old client import library. Genuine native entry, not core-only/dummy DLL.
- Initial `run-ef2430856695434bbd2627e9b02c34da` compile/prelink/link succeeded but
  old static policy rejected newly used bedrock_runtime.dll delay import. That
  partial artifact is NOT accepted/package evidence. Fixed with explicit opt-in
  exact one-module/one-symbol `?$getActivePlayerCount@Level@@QEBAHXZ` whitelist,
  rejecting unknown/ordinal/extra/bound-unload shapes. Default parser still denies
  delays. Rebuilt full candidate above; no blanket import-policy relaxation.
- Manual server-function candidate path remains distinct from a complete normal
  xmake SDK/release certificate. `matched_server_sdk_verified=false` and original
  normal-release gates are not falsified.

Targeted **gpt-6.1-sol / xhigh** review requests accepted: sampler/reader review
`01a1ada6-f20a-7430-b556-a5757ffec78c`, launcher/import delta review
`01a1adab-6184-79e0-bab8-46adee550305`. Both reported no NEW HIGH/CRITICAL, no
execution/edits; actual backend model identity not independently attested.
Static review is not runtime certification. Severe-risk gate completed before
launch; details/limits in docs/SECURITY_REVIEW.md.

NEW isolated test only, **not either protected BDS**:
`.build/isolated-bds/run-16b505e62f5f48099aa58859a30bc71c`.
Explicit distinct ports **59582/59583**, new unique world, empty allowlist,
online authentication true, max1 visitor, cheats false, HTTP/rollback disabled.
Original file/profile/config/mod/process/world/port gates PASS for **3395 files**.
Added validated optional high ports and empty-smoke hold3..30s (default3);
existing default/player policies unchanged. No current test-server file edited.

**Actual runtime result: FAIL before enable, NOT a tick measurement PASS.**
06:49:34–06:50:51 JST, owned child PID60780, official BDS1.26.60.6/LL26.60.0.
Logs confirm new BedrockInsight DLL **loaded**, but NOT enabled/ready. After
"Signed in to signaling service successfully" and "Waiting for Minecraft services...",
the child's own final log says **"Could not connect to Minecraft services. This
is required to accept connections in online mode."** It self-exited code **3**
after ~76s, before the 90s startup deadline. No stop command, no forced termination,
no SQLite DB created, no sampler callback or genuine online/TPS sample proven.
The service error preceded any test stop; do NOT assert a proxy cause or blame
the sampler (its enable path had not run). No security/network/auth changes or
online-mode bypass. The 90s read-only verifier timed out without fabricating data.
Keep this USED root/evidence; do not reset/relaunch it or repeat unchanged tests.

Evidence: new root `capture-runtime-evidence.json`, `capture-stdout.log`,
`capture-stderr.log`, `capture-patch-evidence.json`, `capture-staging-evidence.json`;
candidate `candidate-evidence.json`, prelink/link/PE/header evidence.
`.build/native-analytics-evidence/run-d9570b431b954970ae9b69d824554f7f/`
contains 25-test log, measured/stale DOM log and source-hash/runtime-summary JSON.
C++ formula/storage evidence:
`.build/tick-interval-tests/run-8d139a4c425946498b916938a6272ae9/` (real SQLite fixture,
NOT game samples). Direct fixture HTTP correctly refused its future timestamp;
fresh clocked fixture HTTP separately returns measured20/online2/MSPTnull.

Exact verification commands (project cwd, cached toolchain; no downloads):

```powershell
$env:BEDROCKINSIGHT_CLANGCL_ROOT='C:\Dev\LL2660Toolchain-20261029\clang+llvm-23.1.4-x86_64-pc-windows-msvc\bin'
$env:BEDROCKINSIGHT_SQLITE_ROOT='C:\Users\chihirosub\AppData\Local\.xmake\packages\s\sqlite3\3.54.0+100\b897e7007f3d49769c227e337e14fdef'
$env:BEDROCKINSIGHT_XMAKE_PACKAGES='C:\Dev\LL2660BuildCache\.xmake\packages'
scripts/verify_tick_interval.ps1                  # C++ formulas/windows/storage PASS
scripts/verify_native_runtime.ps1                 # fake SDK lifecycle PASS, not BDS
scripts/verify_interval_runtime_import.ps1        # 1 valid / 10 rejections PASS
scripts/verify_server_release.ps1                 # 23 rejection cases PASS
scripts/verify_capture_telemetry_mode.ps1          # 6 invalid inputs rejected before side effects
scripts/verify_capture_isolation.ps1              # 12 + 3 rejection cases PASS
scripts/verify_capture_player_mode.ps1            # 28+8+7 boundaries, 14 log/window PASS
python -W error::ResourceWarning -m unittest discover -s tests -p 'test_*.py' -v
# PASS 25 tests = previous18 + telemetry7, including real localhost auth/HTTP
python -m py_compile companion/analytics.py companion/telemetry.py companion/server.py companion/verify_ticks.py
node tests/analytics_dashboard_ui_tests.cjs        # measured/stale/unavailable DOM PASS
scripts/build_server_candidate.ps1               # native candidate compile/prelink/link/package PASS
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
scripts/stage_isolated_capture.ps1 -CandidateRoot .build/server-candidate/run-0bd09308338145d49f50587e55e0aae6 -ServerPort 59582 -ServerPortV6 59583
scripts/prepare_capture_executable.ps1 -SandboxRoot .build/isolated-bds/run-16b505e62f5f48099aa58859a30bc71c
scripts/check_capture_isolation.ps1 -SandboxRoot .build/isolated-bds/run-16b505e62f5f48099aa58859a30bc71c -RequirePatched
# PASS preflight. The next two commands ran concurrently, not replay instructions:
scripts/run_isolated_capture.ps1 -SandboxRoot .build/isolated-bds/run-16b505e62f5f48099aa58859a30bc71c -StartupTimeoutSeconds 90 -SmokeHoldSeconds 30
python -m companion.verify_ticks --database .build/isolated-bds/run-16b505e62f5f48099aa58859a30bc71c/mods/BedrockInsight/data/bedrock_insight.sqlite3 --wait-seconds 90
# FAIL: child service startup exit3 before enable; read verifier timed out
```

Other encountered tooling limits: default pwsh concurrent CreateProcessAsUser
access denial; used ordinary cmd/Windows PowerShell invocations. CIM inventory
denied, but original .NET process-metadata inventory resolved both BDS paths.
A Git cat-file read against the partial-clone SDK triggered FAILED automatic
lazy fetch through the configured proxy; no repeat/proxy change. Do not use Git
against read-only SDK clones; official connector supplied pinned publisher source.
An initial cmd-quoted Python glob discovered0 tests; corrected full discovery
above ran25 PASS. No Git write/commit retry under known read-only .git; no push.

At **06:52:40 JST**, protected PIDs **748** and **24864** remain live with unchanged
start times. Active capture DLL hash remains
`b286f054ef1ccf9d90cabbbf3943543fea2b6ff78805d2eb7f044ff89d6d8a00`.
No stop/restart/update or authored folder/config write to those servers, no game
UI, proxy/firewall/credentials/service changes. Only the new authorized disposable
world was created; it and evidence are retained, not deleted. Companion18767
was not stopped or restarted; test viewers used their own ephemeral loopback ports.

NEXT: actual fresh sampler execution requires a NEW fully checked isolated stage
when genuine online-mode Minecraft-services startup succeeds, without weakening
security or restarting either existing server. Then verify fresh native windows
through authenticated HTTP, clean-end/stale states and sampler cadence/online
semantics. Existing native build is useful, but 4-category REAL measured delivery
is still incomplete; no processing-MSPT claim. Additional death/use source hooks
are pre-origin notifications; craft/distance have no selected verified typed
publisher and kill attribution is not established. Keep these unavailable until
separate semantic/accessor/runtime verification. Whole PROJECT_STATUS IN_PROGRESS.

### 2026-11-06 06:24 JST — usable read-only localhost companion; native TPS pending

The NEW dashboard priority supersedes the historical BDS-first/UI-blocker
entries below. Actual join/leave capture is already verified. Both BDS processes
were left running: project test PID **24864** and unrelated PID **748**, confirmed
before/after the work. No stop/restart, DLL replacement, server-folder/config
write, game UI action, network/security/credential change or Git push/write.

Implemented **`python -m companion.server`**, a separately launched companion,
NOT in-DLL HTTP. It serves a real Japanese responsive HTML/CSS/JS dashboard and
authenticated JSON at **http://127.0.0.1:18767/** / `/api/v1/analytics`.
The command was actually started against the user's DB and served successfully;
the verification companion was then stopped with its own Ctrl+C. It is NOT left
running. Starting it again creates a fresh console-only token. Native HTTP and
rollback remain disabled; no new native DLL was compiled, staged or loaded.

| Category | User-visible availability | Actual data / remaining limit |
| --- | --- | --- |
| Player playtime | Implemented | Durable valid closed sessions, JST clipping, interval union, h/m/s, starts, last observed, unresolved diagnostics; no infinite open-session estimate |
| Activity | Implemented for wired scope | Raw join/leave and destroy/place **attempt** row counts; successful changes/materials/craft/use/kill/death/distance explicitly 未計測 |
| Rankings | Implemented | SQL-backed JST day/month/all, time/sessions/supported counts, positive values only, stable ties and bounded pages, opaque UI keys instead of UUID/XUID |
| Effective TPS / processing MSPT | Honest 未計測 state, native measurement PENDING | Active capture DLL has no tick sampler; never invert unverified legacy mspt or fake 20 TPS. Current online unknown; closed-session concurrent peak is a labelled historical lower bound |

Why a companion: existing C++ AnalyticsQuery primarily uses UTC daily counters,
lacks closed-session time and native-attempt semantics, and the existing admin/
HTTP provider is not wired. Enabling that in a live DLL would violate the no-
replacement constraint. The companion reuses the actual v1-v4 persisted schema
and capture semantics without changing any migration or the running module.

Read safety: `mode=ro`, connection-local `query_only`, SQLite backup API into
**memory only** for a consistent response; no immutable-WAL shortcut, source
checkpoint/migration/DML or on-disk DB copy. For BDS roots, existing WAL+SHM are
required and held during reading rather than created if missing. Standard WAL
reader shared-memory/locking participation is not server instrumentation.
Tests demonstrate uncheckpointed WAL visibility with unchanged fixture DB/WAL
bytes. Backup is bounded to 256 MiB / 2 seconds, SQL VM to 2 seconds. Unavailable
source reads produce 503/stale-value warnings, never fabricated metrics.

Security: literal IPv4 loopback bind only (no host option); per-start random
256-bit-equivalent viewer token, SHA-256 / constant-time check, 8h expiration,
no fixed/default token, persistent secret file, cookie or browser storage.
Bearer auth is required for data GET; no mutation/admin/rollback endpoint.
Strict Host/same-Origin, no CORS, no-store/CSP/frame denial/nosniff/no-referrer;
8 KiB / total 2-second header budget, serial bounded listener, connection close.
Names render through textContent / safe DOM only. New ephemeral test credentials
were not written into evidence or server files; existing API secrets untouched.

Actual verification (cached Python **3.13.15**, SQLite **3.50.4**, Node **24.22.1**):

```text
python -W error::ResourceWarning -m unittest discover -s tests -p test_analytics_companion.py -v
  PASS: 18 tests (JST day/month/leap boundaries, overlapping/nested/duplicate
  intervals, crash/open/future/invalid safety, same-name stable rankings,
  pagination, attempts vs successes, legacy/no-sample state, read-only/WAL,
  real loopback auth/headers/size/deadline/readonly routes).
python -m py_compile companion/analytics.py companion/server.py companion/verify.py
node --check companion/web/app.js
node tests/analytics_dashboard_ui_tests.cjs
  PASS: DOM/auth/4-category rendering, literal malicious names, period/metric/
  player selectors, empty ranking/no samples and logout (not browser rendering).
python -m companion.verify --database .build/isolated-bds/run-2bbb60dddd2344c3923ae1cbd66fbb98/mods/BedrockInsight/data/bedrock_insight.sqlite3
  PASS: actual source + actual loopback HTTP, anonymous 401, viewer 200,
  index/JS/CSS 200, player selection 200. Own temporary listener stopped only.
```

Real-data response: Rome6882 **136,093 ms = 2m16s**, one started session,
one join / one leave, 0 recorded destroy/place attempts, no unresolved/invalid
session. TPS/MSPT/current online null, valid tick samples 0; confirmed historical
concurrent closed-session lower bound 1. This is the queried durable record, not
proof that unsupported gameplay never occurred or that the server is now empty.

Evidence: `.build/analytics-dashboard-evidence/run-db86d40109eb411d99741dc0a25c9d73/`
contains `python-tests.log`, `ui-dom-tests.log`, `live-http.json` (NO viewer token),
and `verification-metadata.json` with source SHA256s, unchanged active native DLL
SHA256 `b286f054ef1ccf9d90cabbbf3943543fea2b6ff78805d2eb7f044ff89d6d8a00`,
and the post-work live BDS PID inventory.
Initial test error: Python 3.13 TemporaryDirectory's owner-only Windows directory
was not reopenable by managed processes (SQLite open / cleanup WinError 5).
Fixed fixtures to ordinary uniquely named inherited-workspace directories,
retained for inspection; no ACL change or blocked retry. Explicitly closed test
connections to remove ResourceWarning; final run passes without that warning.

Browser UI preview limitation: `cua.createBrowserTab('iab',...)` reported browser
unavailable; browser inventory was empty. No browser security/permissions bypass
or game/desktop UI fallback attempted. Actual screenshot/layout rendering is NOT
verified; HTTP/assets, JS syntax and deterministic DOM flows ARE verified.

`docs/ANALYTICS_DASHBOARD.md` supplies Japanese startup/token instructions,
concrete URL, existing-private-SSH-forward example, all data definitions and
failure cases. README now links to it and corrects old missing-BDS claims.
Targeted native status: official pinned ServerLevelTickEvent declaration and
PlayerDieEvent/MobDieEvent/PlayerUseItemEvent candidate headers exist, but emission
semantics/cadence and actual adapter execution need separately authorized native
work. Do not attach instrumentation or swap DLLs in the current servers.
Genuine TPS/processing-MSPT and additional native event capture are NOT completed;
full SPEC remains IN_PROGRESS. No Git commit retry under known read-only .git.

## 2026-11-06 05:55 JST — genuine Rome6882 join/leave stored; test OP granted and restarted

LIVE_EVENT_TO_SQLITE_VERIFIED: TRUE — **session_start and session_end** for real authenticated Minecraft player Rome6882. Direct read-only SQLite query returned both rows with same player UUID/session ID and matching timestamps. First test runner after manual participation closed with `exit_code=0`, `forced_termination=false`, `PRAGMA integrity_check=ok`, `events=2`.
LIVE_BLOCK_EVENT_VERIFIED: FALSE — no actual block attempts verified yet.
OP_PERMISSION_CONFIGURED: Rome6882 XUID `2535405594124850` was assigned **operator** in this project-local isolated server's `permissions.json` with no other OP accounts; config written while the test server was stopped. `server.properties` has `allow-cheats=true`, `max-players=2`, `online-mode=true`, `allow-list=true`, `transport=nethernet`, port `59472`, same isolated scratch world. This temporary OP status does not affect the pre-existing other BDS. In-game OP command execution NOT yet verified; user must rejoin after restart to confirm.
CURRENT_RUNNING_ISOLATED_TEST: root `.build/isolated-bds/run-2bbb60dddd2344c3923ae1cbd66fbb98` process PID `24864`, started 05:53 JST and `Server started`/`BedrockInsight enabled` confirmed at 05:54. Controller `.build/run-two-player-bds.py` (custom test-only runner) uses `two-player-operator-op.json`, `two-player-console-op.log` and reads `two-player-stop.flag` to request graceful stop. It has a 3600-second maximum run duration; don't assume it remains active after 06:54 JST without rechecking actual process and port.
RUNTIME_CAPTURE_SCOPE: native adapter currently subscribes only to `PlayerJoinEvent`, `PlayerDisconnectEvent`, `PlayerDestroyBlockEvent`, `PlayerPlacedBlockEvent`; block records are attempts, no verified before/after block state. M2 analytics, M3 audit read/search, M4 rollback planning, M5 HTTP/dashboard code exists in core but is NOT fully wired/active in this live DLL; HTTP and mutations remain disabled.
IMPORTANT: This OP run is a user-requested experimental temporary server, not the earlier strictly visitor-only one-player acceptance policy. Do not misclassify it as a normal-release or SDK/ABI completeness certificate. No original BDS processes or worlds were modified.

## 2026-11-06 04:58 JST — existing one-player run inspected; UI control denied; stage USED

EVENT_TO_DB_VERIFIED: FALSE
CURRENT_CLIENT_BLOCKER: Windows computer-use window access denied; manual client navigation/readiness required.
CURRENT_TEST_ROOT: `.build/isolated-bds/run-9331efac81024a2fb82e87e1bd57f7ab` — USED, NORMAL EXIT, NO PLAYER CONNECTION; do NOT relaunch/reset this root.

The account is AVAILABLE and must not be requested again: `Rome sub`, Xbox
XUID `2535421183826045`, authorized local 26.60 client as recorded below.
Using the computer-use skill's supported `@oai/sky` runtime, app inventory
confirmed the running `Minecraft.Windows.exe` at the recorded 1.26.60.05 path.
The subsequent target-window acquisition/activation/state request was rejected:
**`Computer Use was not approved to use windows`**. No screenshot or successful
client input was returned. Stopped UI automation under the skill's access
boundary; no PowerShell/Win32 injection fallback, alternate helper protocol,
client termination, authentication automation or security-setting change.
The last SAVED screen remains Character Selection, not freshly observed here.

Fresh-stage read-only preflight FAILED on changed
`mods/LeviLamina/config/Config.json`. Investigation of this project's own saved
files explains it: the stage had ALREADY been run after the 03:52 checkpoint.
Its existing `capture-runtime-evidence.json` and stdout show a genuine test
from **04:40:55–04:46:08 JST** (19:40:55–19:46:08 UTC), PID 36416 created by
that recorded test, with all 3,395 staged files passing its prelaunch check.
This continuation did NOT launch that BDS or own its handle.

- Native load, enable, server ready, database creation and disabled config
  overrides: true. Candidate DLL SHA256 independently rechecked as
  `b286f054ef1ccf9d90cabbbf3943543fea2b6ff78805d2eb7f044ff89d6d8a00`.
- Actual one-account mode used a 300-second ready-player window. Stop reason
  `player_window_timeout`; connect=0, disconnect=0, pair=false. Normal stop
  issued; `exit_code=0`, `forced_termination=false`, `error=null`; stdout has
  `Disabling BedrockInsight` and `Quit correctly`. `bds_load_verified=false`
  here means FULL one-player target acceptance failed, not that native load
  failed. This is NOT evidence of a gameplay networking/authentication failure.
- Config v42 and an existing world are EXPECTED post-run outputs. The fresh
  checker correctly refuses to reuse them. Do NOT restore v41, remove the
  world/DB or edit hashes to make the old root pass. The older
  `.build/one-player-ready-root.txt` now points to USED evidence, not a ready run.
- Read-only process inventory: 0 BDS in this root; 1 external BDS untouched.
  No WAL/SHM sidecars. Held the closed DB read-only excluding writers and used
  installed `sqlite3 -readonly`: integrity_check=ok, user_version=4,
  foreign_key_check empty, events=0, sessions=0. DB hash unchanged:
  `46f8656458379eef2e7d944f749c7616f6545d2968501fbd33c6228e12dcc04b`.
  Empty rows do NOT prove player capture/drain.
- Inspection evidence/logs:
  `.build/one-player-timeout-inspection/run-1a4ce33955bc4e7ca4df354e3dd4c7a4/`.
  Runtime evidence SHA256:
  `a4021531097829b91e719a9e726446f05138a5f17849c7f29d377ec65075bb03`.
  All ten previously reviewed/tested scripts match their saved 02:46 source
  hashes; no native/script source change, rebuild or replacement audit suite.

NEXT: user manually takes the existing client past Character Selection to the
server list and reports readiness, or the product provides approved Windows
app-control access. Do not attempt to bypass the denial or ask for the known
account again. A concise async request for this UI action was sent. Only AFTER
client readiness, prepare a NEW disposable stage with the same account and
existing candidate, recheck isolation and review applicability, then coordinate
one real join/leave through the bounded runner and read-only DB verifier. Keep
HTTP/rollback disabled and online auth true; no production/network/service
changes. No BDS launch/stop, other-project file access or Git write this turn.

## Historical preparation — 2026-11-06 03:52 JST: READY at that time; superseded by the USED run above

USER_TEST_AUTHORIZATION: The user explicitly authorized operating Minecraft on the secondary PC via LeviLauncher for a real client test. No need to ask again for generic permission.
TEST_PLAYER_VERIFIED: name `Rome sub`, Xbox XUID `2535421183826045`, confirmed from real previous `LHolo-Integrated-Sync-Work/runtime-bds/logs/latest.log` entries "Player connected: Rome sub, xuid: ..." and visible Minecraft game profile.
LOCAL_CLIENT: `C:\Users\chihirosub\AppData\Roaming\levilauncher.exe\versions\1.26.60.05\Minecraft.Windows.exe`, process observed running (PID 39004), and LeviLauncher v1.0.7 is open. The game's current GUI is the initial Japanese Character Selection screen for `Rome sub`.
ONE_REAL_PLAYER_ISOLATED_STAGE: `.build/isolated-bds/run-9331efac81024a2fb82e87e1bd57f7ab`; official BDS + LL + capture candidate are staged and PeEditor prepared, `check_capture_isolation.ps1 -RequirePatched -RealPlayer -PlayerName 'Rome sub' -PlayerXuid '2535421183826045'` passed; port 59472, empty-new-world, online authentication and one-account allowlist. **BDS is NOT launched.** Preserved stage script: `.build/prepare-one-player.ps1` and root path: `.build/one-player-ready-root.txt`.
INTERACTIVE_CLIENT_BLOCKER: The installed Minecraft 1.26.60.5 client is stuck on its character selection UI. Tried mouse input, Win32 SendInput (returned 2, success), Enter/Escape key, WM_MOUSEBUTTON messages, and Windows touch injection (API returned success), at the visible "次へ" button (x 1500,y 1100 on 2048x1152 screenshot); GUI remains unchanged in verified new screen captures. Windows UIAutomation tree contains no child controls. This is NOT evidence of a gameplay networking failure and never achieved a client join. Do not claim event-to-DB verified. Do not use fake account, disable online-mode, reuse existing BDS, or terminate client merely to claim success.
Historical next action (DO NOT execute against this now-used root): Get the currently running client past the Character Selection screen through an authorized interactive control route, then join a NEW checked private server stage during a bounded RealPlayer test. The player route can be 127.0.0.1:59472 from the same PC if the client permits NetherNet loopback, otherwise verify a private Tailscale/LAN route without changing system settings. Run `scripts/run_isolated_capture.ps1 -RealPlayer -PlayerName 'Rome sub' -PlayerXuid '2535421183826045' -SandboxRoot <a NEW ready stage> -StartupTimeoutSeconds 90 -PlayerWindowSeconds 120` ONLY when the client is at server selection. Join once, disconnect, verify clean exit and SQLite through `scripts/verify_real_player_capture.ps1`. Keep other BDS/other projects unchanged.

NEXT_ACTION_REQUIRES_USER_INPUT: manual navigation of the already-open Minecraft client to server selection/readiness, or approved Windows app-control access; account/XUID are known, but the old stage is USED and a new one is required when the client is ready.
DO_NOT_REPEAT_BASELINE: native load/enable/ready/clean-stop already proven; no new client evidence supplied.

Milestone: REAL isolated BDS 1.26.60.6 / LeviLamina 26.60.0 / BedrockInsight 0.1.0 native LOAD, ENABLE, SERVER READY and NORMAL STOP ALL VERIFIED. SQLite integrity_check=ok after shutdown; no player event-to-DB rows yet (no players connected).

Historical wait checkpoint before 03:52 (SUPERSEDED): identity/client inputs
were not yet provided then. They are now known; do not repeat the old request.
The remaining UI-control/readiness and used-stage facts are recorded above.

## Latest continuation — 2026-11-06 02:46 JST: explicit one-player test mode implemented

The native baseline and DLL are unchanged (candidate `run-8041186e70984bd6b8ce7e73eed55e48`,
SHA256 `b286f054ef1ccf9d90cabbbf3943543fea2b6ff78805d2eb7f044ff89d6d8a00`).
No acquisition, native rebuild, BDS staging/launch/stop or DB modification in
this continuation. **EVENT_TO_DB_VERIFIED remains FALSE.** The previous missing
one-player runner implementation is resolved at source/test level, NOT yet
verified with a real client. Prior missing-BDS/service/normal-stop blockers
remain superseded; do not repeat the zero-player smoke run.

Implemented explicit `-RealPlayer -PlayerName ... -PlayerXuid ...` propagation
through stage -> prepare -> check -> run. New `capture_access_policy.ps1`
requires an exact nonblank gamertag and nonzero canonical UInt64 XUID string,
generates exactly one name/XUID allowlist entry with player-limit bypass false,
records mode/identity in staging and runtime/preflight evidence, and rejects
missing/different/extra accounts or implicit opt-in. The default smoke path
STILL requires an empty allowlist and a three-second ready hold. Normal release
SDK guards are unchanged; the alternate shares all physical/world/port/mod/
permission checks and never fabricates an `allowlist_empty=true` fact.

One-player readiness opens a separately bounded 30-300 second window (default
120) after enable+ready, independently of the startup deadline. Exactly one
ordered matching name AND XUID pair in the strict LL26.60 server log format
triggers normal owned-child stop. Deadline, unexpected identity or duplicate/
out-of-order pair stops with failed acceptance. Stdout pumping is capped to
256 lines per iteration so continuous output does not starve deadline checks.
Existing retained-child handle, blank-first-line stop and forced-kill-as-failure
fallback are preserved. No operator/world commands or network/security changes.
The post-exit verifier now requires the same explicit name/XUID, matching
staging/runtime/preflight policy and bounded roundtrip reason; it still demands
the real closed SQLite session pair and integrity checks. Logs alone cannot
set `event_to_db_verified=true`.

Focused static review explicitly requested `gpt-6.1-sol` / `xhigh`, reviewer
`01a1accb-044a-70a2-a5dd-845dcdf28d8b` (Archimedes). Request accepted; backend
model/effort not independently attested. Initial and final-delta reviews found
no NEW HIGH/CRITICAL in the mode, verifier, guide or parameter tests. No severe
native fix was indicated. Reviewer did not edit or execute tests/BDS; this is
not safety certification or game proof.

Actual PS7 and Windows PS5.1 results, both PASS:

- New `verify_capture_player_mode.ps1`: 28 access + 8 physical/default boundary
  + 7 real-entrypoint INVALID-parameter rejections; 14 log/window decisions.
  Entry tests use nonexistent owned paths and fail before staging/native tools/
  process construction; they leave those paths absent. All metadata/log
  fixtures stay out of SQLite and BDS; no actual player/game event is simulated.
- Existing default boundary: 32 metadata + 5 process-metadata + 2 junction
  rejections. Capture build/config: 12 + 3 rejections. Startup classification:
  11 cases + invalid reason. Stored-session metadata policy: 20 negative cases.
  All ten changed PowerShell scripts parse on both hosts.
- Retained test stdout, host versions, timestamps and SHA256s of the ten scripts:
  `.build/one-player-mode-tests/run-b987d128ad9a4bb9bac31ca0541db4a2/`.
  `ps7-metadata-test-evidence.json` / `ps5-metadata-test-evidence.json` explicitly
  label `bds_executed=false`, `event_to_db_proven=false`.

Next requires USER INPUT: exact consenting test gamertag + XUID, signed-in
compatible Bedrock 26.60 client, and approved private route/readiness. No such
identity/client was supplied this turn. `docs/REAL_PLAYER_ACCEPTANCE.md` now
contains runnable account-parameterized preparation/launch/verification commands
and the precise user join-once / stand-still / disconnect sequence. Do not run
them with example credentials or to repeat baseline smoke tests. Stage a NEW
disposable root only when the real user test is scheduled; preserve online-mode,
visitor-only permissions, high ports, no LAN advertising and disabled HTTP/
rollback. Never alter proxy/firewall/services, other projects or active worlds.
No Git write/commit retry: the known read-only `.git` restriction is unchanged.

## Latest continuation — 2026-11-06 02:33 JST: real-player acceptance verifier

The 01:47 JST clean runtime result below remains the current BDS baseline.
No BDS was launched, rebuilt, restaged or stopped during this continuation.
Do not repeat acquisition, native compilation or the zero-player smoke run.
The actual remaining target is ONE authenticated player's benign join/leave
persisted after normal shutdown; **EVENT_TO_DB_VERIFIED remains FALSE**.

Focused current-session review was explicitly requested through the parent
agent tool with `gpt-6.1-sol` / `xhigh`, reviewer
`01a1acbd-71aa-7600-98bc-142c495e8577` (Newton). Requested parameters were
accepted; backend identity/effort are not independently attestable. Static
review found no NEW HIGH/CRITICAL in native join/disconnect UUID/session
handling, transactional session persistence, event-ID resumption or stop/drain.
Follow-up review found none in the two new verifier/policy files and the
client acceptance guide. The reviewer performed no tests or game execution.
No new severe code fix or native rebuild was warranted by these findings.

Implemented `scripts/verify_real_player_capture.ps1` and its pure metadata
policy `scripts/real_player_capture_policy.ps1`. This is a POST-EXIT, read-only
SQLite/log consistency check, not a launcher or synthetic event generator.
It requires the saved clean runtime evidence and candidate DLL hash, rejects
live owned BDS and WAL/SHM sidecars, opens the DB excluding writers, uses the
installed SQLite CLI with `-readonly`, checks integrity/schema/FKs and unchanged
DB/runtime hashes, then demands exactly one ordered native player session
start/end pair, matching UUID/session/times/duration and real connect/disconnect
logs. It writes its report outside the preserved sandbox. Its consistency
checks cannot independently attest that arbitrary evidence was not fabricated;
an actual authenticated client test is still required.

Actual verification this continuation:

- `scripts/verify_real_player_capture_policy.ps1` PASS on PowerShell 7 and
  Windows PowerShell 5.1: two positive in-memory metadata cases (including
  maximum UInt64 UUID components) and 20 negative cases. These NEVER enter
  SQLite or BDS and are explicitly NOT real-player/event-to-DB evidence.
- Three new PowerShell files parse on PS7; PS5.1 executed the policy and full
  read-only checker. Compatibility fixes: explicitly typed UInt64 parsing
  variables and null-safe handling of zero-row SQLite CLI output.
- Read-only checker against the already stopped `run-d64ec7bff59e4b6a97c043136f0a2c3b`
  with the deliberately nonexistent name `__NO_REAL_PLAYER_YET__` correctly
  REJECTED real-player acceptance. Installed SQLite 3.54 CLI reported
  integrity OK, schema v4, FK issues 0, events 0, sessions 0. Normal disable
  and quit logs are present; connect/disconnect are absent. PS5 process exit
  was 2 (expected rejection, NOT a runtime failure or player proof).
- PS7 report: `.build/real-player-evidence/run-dbe591b5fab7415392d9ccc096345c4b/real-player-evidence.json`.
  PS5 report: `.build/real-player-evidence/run-0cc47929d49144ccbe8bff5b057c1123/real-player-evidence.json`.
  Both report `event_to_db_verified=false`, `database_unchanged=true`.
  DB SHA256 before/after: `c9b28996d025e2b49d39e37848be4599a88b230a99af8953f79b1b50e747f5da`.
  Runtime evidence SHA256: `6efaea9586c321f62fe0745834d1cbc5fe0d9be02d5a5ecced6acc5d08315caa`.

Exact remaining inputs and implementation boundary: no consenting player's
gamertag/XUID or usable authenticated Bedrock 26.60 client/private route has
been supplied. An async user question requests those identifiers and client
availability (never credentials). In addition, the CURRENT checker enforces
an EMPTY allowlist and the CURRENT runner stops three seconds after ready;
neither currently supports human acceptance. Do not claim otherwise or edit
staged files to bypass hashes. Next, once identity/client are confirmed,
implement an explicit one-account, bounded private test mode with focused
Sol/xhigh review and rejection tests, preserving default smoke isolation.
Use a NEW disposable root, not the saved baseline. Keep online authentication,
visitor permissions, fresh world, high ports, no LAN discovery and no HTTP/
rollback/world mutation. No network/security or live server changes.

`docs/REAL_PLAYER_ACCEPTANCE.md` provides the precise user/coordinator sequence:
wait for reviewed private window and ready/enable, join the supplied private
address/high port once, stand still 5-10 seconds, disconnect, normal owned-child
stop, then the read-only verifier command. It explicitly labels the private
mode as NOT YET implemented. Stop here until the real client inputs are known;
do not fill the gap with bots, inserted rows or more unchanged baseline runs.
No Git write/commit was attempted because the known read-only `.git` boundary
is unchanged. No push, production deployment, external project or service edit.

## Latest smoke test — 2026-11-06 01:47 JST: CLEAN SHUTDOWN VERIFIED

BDS_ACCEPTANCE: CAPTURE_SMOKE_NORMAL_SHUTDOWN_PASS
NATIVE_ENTRY_LOAD_VERIFIED: TRUE
NATIVE_ENABLE_VERIFIED: TRUE
SERVER_READY_VERIFIED: TRUE
SQLITE_FILE_CREATED: TRUE
SQLITE_INTEGRITY_AFTER_NORMAL_STOP: OK
NORMAL_SHUTDOWN_VERIFIED: TRUE
EVENT_TO_DB_VERIFIED: FALSE
PROJECT_STATUS_REMAINS: IN_PROGRESS

A fresh isolated 26.60.6 server + 26.60.0 LL with `transport=nethernet` and only the capture mod was staged/verified and executed through the project-owned `run_isolated_capture.ps1`. No live server or existing world was changed.

- Isolated scratch root: `.build/isolated-bds/run-d64ec7bff59e4b6a97c043136f0a2c3b`
- Exact BedrockInsight.dll SHA256: `b286f054ef1ccf9d90cabbbf3943543fea2b6ff78805d2eb7f044ff89d6d8a00`.
- `capture-runtime-evidence.json` reports `native_loaded=true`, `native_enabled=true`, `server_started=true`, `database_exists=true`, `stop_command_issued=true`, `forced_termination=false`, `exit_code=0`, `bds_load_verified=true`, `startup_outcome.stop_reason=ready_hold_completed`, `error=null`.
- Exact own child PID 7296, ran 16:47:17–16:47:37 UTC. Normal stop was requested after 3-second ready hold, child exited in approximately 0.5 seconds; stderr and stdout show `Server stop requested`, `Stopping server...`, `Disabling BedrockInsight`, and `Quit correctly`.
- The BDS first redirected-stdin line unexpectedly contains a UTF-8 BOM even when a raw encoding probe shows no BOM. The runner now sends a harmless empty line first, followed by raw ASCII `stop\r\n`, to consume the prefix: the first line logs `Unknown command: ï»¿` but the SECOND valid command performs clean graceful shutdown. This is a WORKING test-runner workaround; root cause remains to be investigated if a warning-free runner is desired. No gameplay data was changed by the first blank command.
- Post-shutdown read-only inventory found ONLY the pre-existing external 1.26.51.1 BDS (PID 748); NO project-owned BDS remained.
- A READ-ONLY Python sqlite3 check of `mods/BedrockInsight/data/bedrock_insight.sqlite3` after shutdown found 135,168 bytes, `PRAGMA integrity_check = ok`, schema including `events`, and 0 event rows. That is correct for a one-player allow-listed empty server with no player connection. Event-to-DB is NOT verified by this smoke test, even though DB file and schema exist.
- These results supersede the earlier service-connection/enable/stop failures below. The host-side tests used actual public Minecraft signaling with `online-mode=true`, with existing ports/firewall/proxies left unchanged. API and rollback remain disabled.

NEXT Codex priority after the usage quota resets: do NOT repeat baseline BDS/LL acquisition, recompile or normal-stop tests unless needed; use the verified isolated capture infrastructure to demonstrate ONE REAL allowed benign player join/leave (and optional safe block-attempt) event reaching SQLite, validate rows and clean drain, or report precisely why a user-driven test is required. Then assess remaining feature parity and packaging quality. Do not touch the active server.

## New current checkpoint — 2026-11-06 01:35 JST

BDS_ACCEPTANCE: NATIVE_LOAD_ENABLE_SERVER_READY_DB_CREATED_SHUTDOWN_UNVERIFIED
NATIVE_ENTRY_LOAD_VERIFIED: TRUE
NATIVE_ENABLE_VERIFIED: TRUE
SERVER_READY_VERIFIED: TRUE
SQLITE_FILE_CREATED: TRUE
EVENT_TO_DB_VERIFIED: FALSE
NORMAL_SHUTDOWN_VERIFIED: FALSE

The coordinator inspected the exact isolated `server.properties` and found `transport` and `server-udp-ports` absent (only a minimal hand-written config). Minecraft Bedrock 26.60 requires NetherNet. `scripts/stage_isolated_capture.ps1` now explicitly emits `transport=nethernet`, `server-port=59472` (high private test port), and `server-udp-ports=59472`, with online-mode and allow-list still true and LAN visibility false. `scripts/validate_bds_isolation.ps1` now requires exactly NetherNet and ties UDP to validated high TCP signaling port. Two negative regression cases were added; full isolated policy tests PASS on Windows PowerShell 5.1 and PS7: 32 metadata + 5 process + 2 junction cases each.

Two real NEW scratch runs, with exactly the authenticated 1.26.60.6 BDS, verified LeviLamina 26.60.0, and candidate DLL SHA256 `b286f054ef1ccf9d90cabbbf3943543fea2b6ff78805d2eb7f044ff89d6d8a00`:
- `.build/isolated-bds/run-5f201f01a6c542188a3c483ab316d02a` at 01:25 JST: `Signed in to signaling service successfully`, `Server started in (3.8s)!`, `BedrockInsight enabled: join/leave and block attempt capture`, `1 mod(s) enabled in (0.3s)`. SQLite file created. No Minecraft-service failure. The runner did not detect ready/enable in real-time because `logs/latest.log` stopped updating early, while the redirected captured stdout contained the messages. It waited 90s, sent `stop` but BDS parsed it with a prefix (`ï»¿stop`), not a valid command; force-killed ONLY its owned child after a 15s stop grace period. Evidence: `capture-runtime-evidence.json`, `capture-stdout.log`.
- `.build/isolated-bds/run-9d2fc8038aaa4ecd9709db2fb68697a1` at 01:34 JST: incremental async stdout monitor correctly detected both `Server started` and `BedrockInsight enabled`, held 3s (`stop_reason=ready_hold_completed`), and attempted stop. SQLite file existed. However BDS still parsed an extra prefix in the **first** stdin command (`Unknown command: i��?stop`), so force-terminated only its owned child after 15s. Outcome remains FAIL for shutdown; `bds_load_verified=false` in runner evidence means FULL smoke acceptance is NOT met, despite true native load/enable/server-ready flags.

The runner's initial attempt to set ProcessStartInfo.StandardInputEncoding threw before starting BDS on Windows PowerShell 5.1 because .NET Framework does not expose that property. This was removed. A separate 5-second Python redirected-stdin test confirmed raw `stop\r\n` bytes are `73746f700d0a` without BOM, so the BDS first-command prefix cause remains unidentified. An experimental safe blank first line before `stop` is now staged in `scripts/run_isolated_capture.ps1` but NOT YET verified on real BDS; do not claim normal shutdown is fixed. Windows PowerShell parser and exact raw bytes were checked before the second run. Only project-owned throwaway BDS children were launched/terminated; a read-only inventory after the second run showed the pre-existing external BDS PID 748 still running.

The previous service connection failure was observed under Codex's restricted process environment (configured unusable loopback proxy), while these successful runs were driven by the host's self-hosted MCP PowerShell without the same proxy values AND used explicit NetherNet settings. Thus one cannot attribute success to NetherNet alone. No proxy/firewall/system security setting was modified; online-mode stayed true. No player events were injected or captured; SQLite file existence is NOT proof of event storage or normal clean DB shutdown.

Next: have Codex diagnose and verify first-stdin-command prefix handling and clean `stop`/SQLite shutdown in yet another fresh scratch world (only when useful); then prove actual benign join/leave and block-attempt event-to-DB rows. Never alter active BDS, world, network config, or unrelated projects. Existing API/rollback remain disabled. **This checkpoint supersedes older Minecraft-service-blocked status below.**

## Current checkpoint — 2026-11-06 JST (2026-11-05 UTC)

BDS_ACCEPTANCE: NATIVE_LOAD_CONFIRMED_ENABLE_BLOCKED_MINECRAFT_SERVICES
NATIVE_ENTRY_LOAD_VERIFIED: TRUE
NATIVE_ENABLE_VERIFIED: FALSE
EVENT_TO_DB_VERIFIED: FALSE

The binary/header/prelink/manifest blockers are resolved for this capture
target. This is now an ACTUALLY LOADED native DLL, not merely a link probe.
It is NOT full server-start, enable, shutdown-drain or event capture acceptance.
Historical progress/blockers below are superseded by this topmost checkpoint.

Final fresh candidate, built **14:54:34 UTC**, with corrected manifest and
real fail-closed server/version guard:

```text
.build/server-candidate/run-8041186e70984bd6b8ce7e73eed55e48/BedrockInsight.dll
size: 1,534,464 bytes
SHA256: b286f054ef1ccf9d90cabbbf3943543fea2b6ff78805d2eb7f044ff89d6d8a00

same run / BedrockInsight-server-candidate.zip
size: 1,046,660 bytes
SHA256: 10b1377292a8cce57ec20759b015207711e54057ab8227b07fc0e790e5c4fea5
```

Full build PASS on Windows PS5.1: revalidated all 22,202 official header
blobs (missing=0, copied=0), freshly compiled native entry/runtime and the
five capture-core sources, processed owned copies with official prelink
0.8.8 + server.5 data and linked /O2 /MD x64/C++20/static SQLite. All **24**
LL imports resolve against the verified actual official server DLL, including
getLoaderVersion/isClient. LL's unified memory override and ll_mod_load
exports remain present. No old client import lib or fabricated SDK package.
Full compiler/link/prelink arguments, source/header/dependency identities,
copied-object/library hashes and inspections are in candidate-evidence.json.
The manual ZIP remains marked NOT_RUNTIME_VERIFIED because full enable/
capture acceptance below did not pass. No normal-release evidence fabricated.

Actual second isolated test:

```text
root: .build/isolated-bds/run-fb88d04ab9e3428aacb982705fd26f6d
runtime: 2026-11-05 14:57:30–14:58:47 UTC
created child PID: 36264
3395 staged file hashes + fresh-world/mod/permission/port/process policy PASS

23:57:39.654 [LeviLamina] Loading BedrockInsight v0.1.0
23:57:39.656 [BedrockInsight] BedrockInsight loaded: capture only; HTTP and rollback disabled
23:57:39.656 [LeviLamina] BedrockInsight loaded
23:57:39.656 [LeviLamina] 1 mod(s) loaded
23:57:39.928 [Server] Version: 1.26.60.6(ProtocolVersion 2225) with LeviLamina-26.60.0
23:57:42.064 [Server] Waiting for Minecraft services...
23:58:47.442 [Server] Could not connect to Minecraft services. This is required to accept connections in online mode.
23:58:47.442 [Server] Server stop requested.
Quit correctly
```

Evidence in that root: capture-staging-evidence.json, capture-patch-evidence.json,
capture-runtime-evidence.json, capture-stdout.log, capture-stderr.log,
logs/latest.log and capture-post-test-evidence.json. The same final DLL hash
was staged and loaded. Real version/platform guard and native load succeeded.
The config migrated to complete v42 with all required disabled overrides
retained, also checked after exit. No "enabled" line or SQLite file was created:
LL enables mods from ServerScriptManager::onServerThreadStarted, which was
not reached before the service-dependent startup failed. No player or synthetic
game event was injected. No SQLite integrity/data-row claim is made.

The bounded runner sent stop ONLY to its created child. Exit 3; no forced
termination. At **15:01:08 UTC**, read-only post-test inventory confirmed
ZERO project-owned BDS, one external BDS and neither test port 59472/59473
bound. Original BDS EXE remained hash-identical after PeEditor; only the fresh
scratch world was generated. Never reused an earlier test world. No other
project/live server/world/service was accessed or modified beyond the explicitly
read-only process-path/port ownership preflight. Online mode remains TRUE;
no proxy/firewall/persistent security setting was changed or bypassed.

Independent Sol/xhigh static review (agent 01a1ac0b-84ee-75e1-854b-88a6005bdf69)
preceded each runtime change: no remaining HIGH/CRITICAL after the config fix,
then no new severe issue in the manifest/version guard or post-exit checks.
Requested parameters accepted; backend model/effort not independently attested.
Reviewer performed no runtime execution. Agent closed after final disposition.

Current focused verification: PS5 + PS7 official input policy 28 rejection
cases, capture-build 12 + config 3 rejections, profile tests and existing
isolation 30 metadata + 5 process + 2 junction cases PASS. Modified native
adapter safety harness PASS with four new unsupported-loader failures; it is
fake SDK testing only, clearly separate from the real loaded-entry evidence.
The full native candidate build and real two-run results above are actual,
not attributed to the reviewer. No Git write/commit retried: .git remains
read-only; project files are still untracked. No GitHub push.

Next: resolve/diagnose permitted Minecraft-service reachability without changing
the deliberately restricted proxy/security policy. Then use another FRESH
throwaway root to prove enable/ready/normal disable/SQLite and actual benign
events. Do not repeat binary acquisition, client-SDK probes or unchanged failed
runtime tests. Do not silently set online-mode=false or weaken the existing
isolation policy; an offline variant needs explicit direction and a separately
reviewed access boundary. Keep HTTP, rollback and other world mutation disabled.
Overall PROJECT_STATUS remains IN_PROGRESS.

## First real isolated execution and concrete manifest fix — 2026-11-05

BDS_ACCEPTANCE: MANIFEST_FIX_REBUILD_IN_PROGRESS

Sol/xhigh agent 01a1ac0b-84ee-75e1-854b-88a6005bdf69 completed the focused
review BEFORE game execution: no remaining HIGH/CRITICAL. Subsequent focused
review of the manifest/runtime guard fix also found no new HIGH/CRITICAL.
Remaining moderate findings were fixed (typed integer config version and
parse/hash from the same JSON byte snapshot with final unchanged checks).
Current text suites: 28 official-input, 12 capture-build, 3 loader-config
rejections. The prelaunch release SDK guard remains separate and intact.

Actual first BDS test ran **14:42:48–14:44:05 UTC**, owned PID 44052,
in .build/isolated-bds/run-ce8413c64e7b4c048d14a12736fb22c0 only.
Original BDS FileVersion/ProductVersion both 1.26.60.6. Verified patched EXE
SHA256 c71d105e3740f8a373658de042c3ea6ad8709275bc4aeee43f23368f53602873.
All 3,395 staged files and ports/process/world boundary passed preflight.

```text
23:42:57.480 LeviConfig rewrite successfully
23:42:57.528 BedrockInsight will not be loaded because dependency LeviLamina is missing
23:42:57.529 0 mod(s) loaded
23:42:57.794 Version: 1.26.60.6(ProtocolVersion 2225) with LeviLamina-26.60.0
23:42:58.456 Accepting clients on [::]:59472
23:42:59.919 Waiting for Minecraft services...
23:44:05.489 Could not connect to Minecraft services. This is required to accept connections in online mode.
23:44:05.489 Server stop requested.
Quit correctly
```

capture-runtime-evidence.json / capture-stdout.log / capture-stderr.log and
logs/latest.log preserve the FAILED mod load. Exit code 3; stop command sent
to the created child, no forced termination. Native loaded/enabled and DB
flags FALSE. Read-only process inventory afterwards: one external BDS,
zero project-owned BDS. No other server was stopped/accessed or modified.
LL actually migrated the config to complete v42 and retained all disabled
statistics/crash/Sentry/process-check/STUN/forced-cheat/input flags, confirming
the prelaunch configuration fix. One earlier PS5 launch preparation failed
before Start on lazy environment-dictionary initialization; the guarded
initialization was fixed before the above real child execution.

Exact source diagnosis: pinned src/ll/core/mod/ModRegistrar.cpp loadManifest
deliberately excludes manifests whose type is the PreLoader mod manager.
Thus LeviLamina is NOT an ordinary-mod dependency-graph node. Removed that
invalid native dependencies edge, not the actual installation prerequisite:
Tooth still requires LL 26.60.0; native extraInfo records the pin. Runtime::load
now checks the REAL exported ll::getLoaderVersion and ll::isClient before data
or hooks, rejects client/non-26.60.0/prerelease and catches version exceptions.
Shared manifest validation rejects the old unloadable graph edge. Four new
adapter-only guard rejection cases pass; the safety harness remains explicitly
fake SDK, not game evidence. The actual official-header DLL is being rebuilt.
An intermediate rebuild stopped before link on a missing dot-import for the
new output guard (run-c5259589239548fab002a2f3a3991709); fixed the import and
started a clean fresh build. No success evidence or package from that failure.

Minecraft-service reachability is an additional real runtime obstacle, NOT
missing binary input. Online mode remains TRUE; no proxy, firewall, persistent
security setting or active server configuration was changed to bypass it.
Do not reuse the first world's directory for a retry; stage a fresh run.

## Official tools integrated; matching BDS received — 2026-11-05

BDS_ACCEPTANCE: ISOLATED_RUNTIME_PREPARATION
DLL_KIND: OFFICIAL_SERVER_FUNCTION_IMPORT_CANDIDATE_WITH_PRELINK

Read OFFICIAL_INPUTS.md, BDS_LOAD_FIRST.md, REVIEW_BEFORE_BDS.md and AGENTS.md
first. All SIX official tool/runtime archives now independently match their
handoff sizes and SHA256 pins in packaging/official-inputs.json. The matching
BDS ZIP arrived DURING this turn: 88,759,231 bytes, independently rehashed as
21620db4f80d0fe206f21cb08f52f4e5f14252ce865ec49ccfbe0d9f93ce88b0.
Earlier missing-BDS statements below are historical and superseded.

Fresh native build completed **14:18:13 UTC**:

```text
.build/server-candidate/run-99b4ca4bcdab460487a51ae517cc760f/BedrockInsight.dll
size: 1,533,440 bytes
SHA256: 8789be116f1dc3781eff3573f7def258caf1a18d19d88dd3c436147f276d148e

same run / BedrockInsight-server-candidate.zip
size: 1,045,367 bytes
SHA256: ebd1c8ee7e0fb5fa0869151b32d1b70633f8e3e54bff6572284d48ddb1f2d8d4

same run / prelink/output/lib/bedrock_runtime_api.lib
size: 1,392 bytes
SHA256: f664c930a50c9a6ddf3b207cb5e3148bcc302cddd17c491b1b989f53df08cd2b
```

scripts/build_server_candidate.ps1 now freshly stages/inspects official tools,
revalidates all 22,202 pinned header blobs, recompiles the real native entry,
typed event adapter and five capture-core translation units, runs official
prelink 0.8.8 with server.5 data, then links processed object/library COPIES,
the actual runtime API import library and /DELAYLOAD:bedrock_runtime.dll.
No old client library, invented SDK manifest or fabricated MC implementation.
All 22 resulting LL imports resolve against the verified official server DLL.
No direct/delay game import remains in this particular resulting mod DLL.

Important actual CLI diagnosis: prelink --help exits 1 and reports stale
data/dir/object syntax. That syntax was tested on disposable COPIES and failed
"failed to read bedrock_runtime_data". The actual official cached build-rule
syntax works: prelink.exe server-windows-x64 <outdir> <data> <objects/libs>.
It accepted the pinned data and emitted a genuine import archive (three COFF
descriptors plus one named Actor::getDimensionId import; no implementation).
It does NOT generate a runnable bedrock_runtime.dll. The final mod resolves
getDimensionId from LL, so this import record is not retained by its linker.
Original seven objects/two libraries are hash-checked unchanged; prelink never
receives a C:\Dev/cache/earlier-build path as a writable input. Compile/link/
prelink arguments, copied input hashes, header provenance, native sources,
actual generated library and inspections remain in candidate-evidence.json.

scripts/stage_official_inputs.ps1 extracts selected, hash-pinned payloads to
unique .build/runtime-inputs roots without executing installer hooks. ZIP
traversal, ADS, Windows aliases/devices, links, duplicates and size excesses
are rejected. inspect_official_runtime.ps1 verifies AMD64 PE metadata and all
15 actual LL-to-PreLoader named imports (including five DATA exports).
PreLoader 1.16.7 has 22 names: 17 executable-section functions and five data
exports; other immediate imports are Windows/CRT only. Metadata resolution
is not C++ ABI or complete runtime dependency proof. The SDK's public
PreLoader 1.16.2 header pin is intentional official source, not relabelled 1.16.7.

Actual focused checks:

```text
verify_official_inputs.ps1: PS7/PS5 PASS (latest 27 text-only rejections)
verify_server_release.ps1: PS7 PASS (23 rejection cases)
verify_server_function_exports.ps1: PS7 PASS (14 rejection cases)
verify_logged_native.ps1: PS7 PASS (real stderr/exit0/exit7/missing-EXE cases)
build_server_candidate.ps1: Windows PS5.1 PASS, real fresh artifacts above
probe_official_prelink.ps1 -Syntax BuildRule: PS5 + PS7 PASS on real copies
probe -Syntax ReportedUsage: expected exit1; final failure evidence correctly
records generation_verified=false and all original input postchecks passing
verify_bds_isolation.ps1: PS5 + PS7 PASS (30 metadata + 5 process + 2 junction)
verify_capture_isolation.ps1: PS5 + PS7 PASS (12 candidate + 2 config rejections)
```

Isolated capture preparation uses scripts/stage_isolated_capture.ps1,
prepare_capture_executable.ps1 and check_capture_isolation.ps1. This is a
SEPARATE honest candidate policy, not substituted full-SDK/release evidence.
The original release guard retains its SDK requirements; only its existing
physical/configuration boundary was extracted for reuse. Every staged file
is hashed, unknown files/extra mods/reparse traversal rejected, worlds must
not exist, allowlist/permissions empty, ports 59472/59473 unused and distinct,
running process paths read-only inventoried. No live server is accessed.

Pre-launch review found a new HIGH risk in an initially authored partial LL
config labelled current schema v42: missing required fields could trigger
fallback to unsafe telemetry/process/crash defaults. No BDS was executed.
Fixed by deliberately using v41 input to the pinned official v42 updater,
which merges complete default fields with the disabled overrides before
deserialization. The policy now rejects partial v42 input. Crash logger,
Sentry uploads, LL statistics, other-process checking, forced cheats, STUN
patch and console input preservation are disabled in the isolated config.
The first scratch run b89fb2a2a21b4b4f8533fc59f6fa7ebc is retained UNLAUNCHED
for evidence; do not use it. Fresh corrected scratch run:
.build/isolated-bds/run-ce8413c64e7b4c048d14a12736fb22c0.

PeEditor -m only is used on the scratch BDS copy with explicit --exe/-o;
NO -b/--inplace or installer scripts. The original EXE remains unchanged.
The generated EXE must show the exact PreLoader/pl_resolve_symbol import.
Only en.json localization DATA is used from the official tagged source ZIP;
its recorded digest is observed provenance, not an upstream checksum claim.
PS5 preparation bugs (compression assembly loading; valid empty JSON key in
the official locale) were corrected before any BDS launch.

Independent requested Sol/xhigh review is in progress for the new runtime
boundary/launcher; no game launch may precede its severe-risk disposition.
Prior severe native/core fixes remain unchanged; HTTP and rollback disabled.
No Git write was retried (known read-only .git), no unrelated core/dashboard
work, SDK/cache installation, production deploy, service/port change or push.
No real game event-to-DB evidence exists yet. PROJECT_STATUS remains IN_PROGRESS.

## Fresh official-server-derived native candidate — 2026-11-05

BDS_ACCEPTANCE: BLOCKED_BDS_INPUT
DLL_KIND: OFFICIAL_SERVER_FUNCTION_IMPORT_CANDIDATE
MATCHED_SERVER_SDK: PARTIAL_FUNCTION_IMPORTS_ONLY

Read all three priority files first. Implemented the previous checkpoint's
next engineering step rather than repeating the old client-library diagnostic.
No native/core/dashboard C++ source changed. No server was launched or DLL loaded.

The official hash-verified 26.60.0 server DLL has 960 named exports in readable,
non-writable executable sections backed by section raw data; one data export,
ll_memory_operator_overrided, is excluded. Forwarder RVAs, ambiguous/overlapping
sections, unmapped/unbacked exports and unsafe .def names cannot enter the
function library. llvm-dlltool generated genuine function-only COFF import
metadata from this actual DLL, not an implementation/stub server or invented
xmake SDK. The mod still exports its OWN real SDK allocation-override marker.

New build output, built **13:51:34 UTC**:

```text
.build/server-candidate/run-92994354508f400bbcebe2b32ec966bf/BedrockInsight.dll
size: 1,529,344 bytes
SHA256: 32d522f12b177d9a2940dd7d3313268bac426295fb10bb1dec6d521db5dad5b9

.build/server-candidate/run-92994354508f400bbcebe2b32ec966bf/BedrockInsight-server-candidate.zip
size: 1,025,945 bytes
SHA256: 2f09871e40e0c0c5aabc7483efb2547a5a46ff9610915f643cb175c4b81f9ef3

LeviLamina.functions.lib (same run): 960 named import records + 3 descriptors
SHA256: 01db841922709a672d8fe1a00584a6a8820ff157181b9a8ce25e77cafdcf4335
No executable sections. No old client import library was linked.
```

scripts/build_server_candidate.ps1 revalidated all 22,202 pinned header blobs
(missing=0, copied=0), freshly compiled actual mod.cpp/runtime.cpp and five
capture core sources, then linked /O2 /MD x64 with the generated function
library and static SQLite only. All 22 resulting LL imports are named,
included function exports and present on the official server DLL. Required
ll_mod_load and allocation marker exports are present; other imports remain
Windows/MSVC CRT. This is a real native DLL, not core-only or a dummy artifact.
The old SDK root supplies exact recorded third-party HEADER dependency records
only, never old MC/LL headers or its client import library.

The run contains candidate-evidence.json, owned mod.obj/runtime.obj hashes,
snapshot header-compile-evidence.json, pre/post-checked hashes for 23 C++ source/
header files and SQLite, full compiler/link args, .def/import library/LLVM
inspection logs, core-compile.log, link.log and NOT_RUNTIME_VERIFIED.txt.
The manual candidate ZIP has BedrockInsight/{DLL,PDB,manifest.json} and warning
at ZIP root. It has no Tooth installer, runtime binaries or full-SDK facade.
Not a deployment or redistribution-completeness claim.

Actual tests/results:

```text
scripts/verify_server_function_exports.ps1
PS7 + PS5.1: PASS, exit 0 (14 text-only rejection cases)
scripts/verify_logged_native.ps1
PS7 + PS5.1: PASS, exit 0 (stderr+exit0 accepted; exit7/missing EXE rejected)
scripts/verify_server_release.ps1
PS7 + PS5.1: PASS, exit 0 (23 text-only rejection cases)
scripts/build_server_candidate.ps1 (documented LLVM/SQLite/package roots)
Windows PS5.1: PASS, exit 0; fresh native DLL + candidate ZIP above
Artifact DLL/ZIP/import-library hashes rechecked: PASS
Actual candidate evidence rejected by normal isolation release gate: PASS
Normal .build/levilamina/release/build-evidence.json remains absent
```

Two build-only PS5 issues were reproduced and fixed before the successful run:
native stderr warnings became terminating ErrorRecords, then a local
LASTEXITCODE shadowed PowerShell's global native status. Shared
scripts/invoke_logged_native.ps1 now captures stderr under a function-local
Continue preference, resets/reads global:LASTEXITCODE, and rejects null/nonzero
or invocation failures. Its focused regression uses installed PowerShell and
project-local logs, never fake EXE/DLL/SDK fixtures. Initial failed runs emitted
no success evidence. Compiler warnings remain in logs, not silently suppressed.

Independent static review requested with gpt-6.1-sol/xhigh, agent
01a1abef-5e64-7511-ad71-fcd1a51b155f: no NEW high/critical finding after
reviewing the latest export/build/inspector/native-command helpers and fixes.
Requested parameters accepted; backend designation cannot be independently
attested. Reviewer did not execute scripts or binaries. Residual: section-based
classification is not instruction-boundary/physical-file-bounds or C++ ABI
proof; the pinned official binary identity remains essential. Existing native/
core severe fixes are unchanged; those unrelated suites were not rerun.

New runtime inputs arrived during the build. At 13:55:36 UTC server.5 runtime
data ZIP matched its pinned size/SHA256. Also observed preloader v1.16.7,
prelink v0.8.8, PeEditor v3.9.2 and CrashLogger v1.4.0 archives. Their observed
hashes/lengths are saved in this candidate's incoming-runtime-inventory.json;
those four tools still need authenticated release provenance/version checks.
No new runtime/tool archive was extracted for installation or executed.
The BDS 1.26.60.6 archive is still absent. Do not read/copy/use the active
1.26.51.1 server or change proxy/security settings.

Candidate evidence explicitly keeps matched_server_sdk_verified,
runtime_dependencies_verified, normal_release_evidence, launch_authorized and
bds_load_verified FALSE. candidate_packaged is TRUE for the manual ZIP only.
Normal release/isolation guards were not weakened; no normal release evidence,
Git index write, global SDK/cache installation, world/service/port change or
other-project action occurred. There is no loaded/enabled log or real event-to-DB
proof. Keep PROJECT_STATUS IN_PROGRESS.

Next: authenticate/inspect the newly supplied runtime tools, resolve the real
runtime generation/installation chain in project-local staging, and acquire
matching BDS 1.26.60.6. Then prove fresh isolation before load/enable/shutdown
tests. Do not repeat the old client diagnostic or claim runtime success from
this candidate build. Full xmake SDK packaging remains separate/incomplete.

## New official server loader / actual import-resolution proof — 2026-11-05

BDS_ACCEPTANCE: BLOCKED_RUNTIME_INPUTS
SERVER_LOADER_INPUT: VERIFIED_OFFICIAL_26_60_0
MATCHED_SERVER_SDK: NOT_COMPLETE

Read the updated BDS_LOAD_FIRST.md, REVIEW_BEFORE_BDS.md and AGENTS.md first.
The coordinator supplied a genuinely new official server release ZIP; the old
"no server binary available" blocker below is now superseded, not repeated.
No C++/core/dashboard source, release guard or runtime behavior changed.

Verified actual bytes:

```text
.build/bds-downloads/levilamina-v26.60.0-server-release-windows-x64.zip
size=12,353,484
SHA256=392c567b8c86b9e1739939b578e0fefa2a758c6019d195644f868711c44101dc
internal manifest: LeviLamina / LeviLamina.dll / 26.60.0 / preload-native
extracted loader: AMD64 PE DLL, 4,779,520 bytes
SHA256=211a20b6cd988d1b907fc300aca0bdf5387b35f1b5d0f0656d9c9e9270b45bac
961 named, nonzero-RVA exports
```

The ZIP contains runtime DLL/PDB/manifest, README/changelog and licenses, not
development headers or an import library. New scripts/inspect_server_release.ps1
verifies pinned archive size/hash while holding a read-only stream, rejects
duplicate entries, and copies only exact DLL/manifest entries to fixed leaves
in a fresh project-local .build/server-release-inspection/run-* directory.
LLVM reads metadata only; neither DLL is loaded. Validation/tests live in
scripts/validate_server_release.ps1 and scripts/verify_server_release.ps1.

The actual existing full native BedrockInsight DLL at
.build/pinned-sdk-link-probe/run-d370c5fc2a5e4e58b6d3cb3785fe73b4/BedrockInsight.dll
still has SHA256
32d25d9303a3884c1c8cc629e34f26c0ef60f2209af62873ae6b347ba14d1d02.
All **22** named LeviLamina imports resolve against the actual official server
DLL. Both ll_mod_load and ll_memory_operator_overrided are present; other
mod imports are Windows/MSVC CRT only. This is new symbol-resolution evidence
for the existing diagnostic, NOT a fresh build, complete SDK or BDS-load proof.
The preliminary manual count 988 included 27 dependency-module names; the
strict Export-record parser correctly counts 961 exports.

The old client import archive has 1,041 named metadata records; **126 names
are absent** from this official server DLL. Do NOT relabel it as a full matched
server SDK just because the particular 22 used by this mod happen to resolve.
Source-level ABI/layout, allocator execution, event hooks and real shutdown
remain untested. Historical diagnostic flags/evidence were not overwritten.

Actual checks (exit 0 unless explicitly historical below):

```text
scripts/verify_server_release.ps1
PS7 + Windows PS5.1: PASS (23 text-only rejection cases; no DLL load)
positive fixtures include server Winsock ordinals and nested delay metadata

scripts/inspect_server_release.ps1 -ModDllPath <existing diagnostic path above>
PS7 + Windows PS5.1: PASS; 22 imports resolve; NOT a BDS load proof
PS7 evidence: .build/server-release-inspection/run-1155e754dfcc458c82fef7afe918f922/inspection-evidence.json
PS5 evidence: .build/server-release-inspection/run-24b485800ad246b0a1aca314bab532d4/inspection-evidence.json
each directory also retains server-pe.txt and mod-pe.txt

scripts/verify_import_metadata.ps1
Windows PS5.1: PASS (8 existing text-only rejections)

inspect_server_release.ps1 -ModDllPath C:\outside\BedrockInsight.dll
rejected before file access: Mod DLL must be project-local; exit 1 (PS5.1)
Actual evidence assertions: PASS (22/961 and SDK/package/launch/load flags false)
```

The first actual inspector rejected legitimate server Winsock ordinal imports,
then exposed bedrock_runtime.dll delay imports. Fixed by separating module-name
inventory from strict mod named-import validation; the mod still rejects both
ordinal and delay imports. Also explicitly loaded both compression assemblies
for PS5. No failed intermediate run produced success evidence. A transient
PS7 process-create denial was handled with PS5; PS7 checks passed after review
completed. llvm-dlltool --help returns 1 informationally; not a test failure.

Independent static review requested with gpt-6.1-sol/xhigh, agent
01a1abde-ec83-73a2-aa8b-74131868e944: no NEW high/critical finding in the
latest three scripts. Requested parameters were accepted; actual backend model/
effort are not independently attested. Reviewer ran no binary/tests/runtime.
Prior native/core severe fixes remain in docs/SECURITY_REVIEW.md; unchanged
C++ suites were not rerun. No Git write was retried.

Precise remaining relationship/blockers: the official loader imports
**PreLoader.dll** normally and **bedrock_runtime.dll** through a delay-import
table. Module inventory does NOT prove their presence/version or resolve their
symbols. Locked targets remain BDS 1.26.60.6, runtime data server.5,
prelink 0.8.8 and runtime PreLoader 1.16.7. The download directory contains
only the new loader ZIP; the matching BDS archive was not supplied. No sandbox,
world, port, service, other project or existing 1.26.51.1 server was accessed
or changed for these checks. No load/enabled log or event-to-DB proof exists.
At 13:30:27 UTC the expected BDS 1.26.60.6 and server.5 runtime-data archive
paths and normal server release build evidence were still absent.

Next engineering step is now concrete, NOT "wait for a server DLL": examine
the 22 required exports' executable-section RVAs and, if all are real functions
without forwarding/data ambiguities, reconstruct an explicitly limited import
archive from this verified official DLL using installed llvm-dlltool. This
would be genuine import metadata, not a fabricated SDK or implementation.
Then fresh-build a clearly marked native candidate against the already pinned
official headers and recheck all imports. Preserve normal release/isolation
guards; do not invent xmake package manifests or claim a complete SDK. BDS
runtime acceptance still waits for all matching runtime inputs and isolation.

Historical binary-input recheck — **2026-11-05 04:02:15 UTC**: all six pinned
LL26.60.0/server.5/prelink0.8.8 cache directories remain absent; files in
.build/bds-downloads=0; normal server release evidence absent; no archive or
binary dropped into the project root. HTTPS_PROXY still matches the previously
refused loopback proxy and read-only TCP inventory finds no port-9 listener.
No SDK root/library override is configured in this session. The next action
requires new matched binary inputs or restored permitted transfer, not another
unchanged recheck.
Connector-transfer check at 04:01:36 UTC: available MCP resources contain
plugin/skill metadata, not an SDK/archive resource; resource templates are empty.
The GitHub fetch tool explicitly requires UTF-8 and rejects binary downloads.
Its artifact downloader returns a reusable file reference, with no local-output
parameter; no callable connector-reference-to-local-file mechanism was found.
No artifact re-download, private storage search/upload, remote-PC workaround or
proxy/security bypass was attempted. Local materialization remains unresolved.
No transfer/build/test/Git-write retry, source change or BDS launch. Prior Sol
reviews/fixes remain recorded below and in docs/SECURITY_REVIEW.md; no new code
required another review. Runtime acceptance is still NOT performed. A readable
local matched server SDK+BDS/runtime bundle (or permitted working artifact
transfer) remains the concrete prerequisite; do not repeat diagnostic DLL
builds or resume unrelated features while waiting for it.

## Process inventory preflight follow-up — 2026-11-05

BDS_ACCEPTANCE: BLOCKED_BINARY_INPUTS

Read BDS_LOAD_FIRST.md, REVIEW_BEFORE_BDS.md and AGENTS.md first. Addressed
the previous concrete preflight obstacle, not native/core/dashboard features.
The WMI/CIM query was denied, but the standard read-only .NET Process API
works at current permissions. check_bds_isolation.ps1 now uses
GetProcessesByName for bedrock_server/bedrock_server_mod and MainModule.FileName.
It gathers the full result before returning; enumeration and missing/inaccessible/
exited-process metadata errors fail closed. Cached metadata and process changes
after observation are not guaranteed to be detected. Dispose releases only local metadata handles:
there is no launch, stop, signal, wait, bind, file copy or server mutation.
Ambiguous relative, device/UNC, short-name and noncanonical paths are rejected
rather than guessed. This is NOT complete file-identity or ownership proof.

Actual verification:

```text
scripts/verify_bds_isolation.ps1
PS7 7.6.6 + Windows PS5.1.26100.9711: PASS, exit 0
PS5.1 invoked with -NoProfile -ExecutionPolicy Bypass (process-local only)
30 metadata + 5 process + 2 real project-local junction rejections
No EXE/DLL fixtures or BDS launches

Get-BdsRunningExecutablePaths (read-only actual OS metadata)
02:42:47 UTC PS7: count=1, external_to_project=1
02:43:06 UTC PS5.1: count=1, external_to_project=1
Both identify the existing external bedrock_server_mod process; no interaction

scripts/check_bds_isolation.ps1 -SandboxRoot <project>/.build/isolated-bds/run-b8c195e74ee04aca90cd286414cda332
Missing normal server release build evidence; compile probes cannot pass
exit=2 (PS5.1)
```

The process tests inject metadata getter errors/nulls/wrong executable names,
not processes or binary files. The full-candidate CLI still cannot reach its
inventory step without matched release inputs; only the inventory helper was
verified against the real OS. Even positive policy results retain
launch_authorized=false and bds_load_verified=false. File identity/aliases,
renamed processes, ACLs, runtime provenance and process/port races remain
outside this advisory policy; no isolation certification is claimed.

Focused independent static review requested with gpt-6.1-sol/xhigh, agent
01a1a995-d1c7-7781-a348-800283c77b78: no NEW high/critical finding in the
three script changes. Requested parameters were accepted; backend model/effort
not independently attested. Reviewer did not run tests/inventory or compare
against a usable Git HEAD. Tests cover path/getter rejection, not collector-level
late failure/disposal injection or real OS races. See docs/SECURITY_REVIEW.md.

At 02:42:24 UTC all six pinned LL26.60.0/server.5/prelink0.8.8 cache paths
remained absent. At 02:43:06 UTC downloaded files=0 and normal release evidence
was absent. No new download attempt, proxy or persistent security-setting change.
Native/core sources and DLL were not rebuilt; the existing diagnostic DLL
hash was rechecked unchanged:
32d25d9303a3884c1c8cc629e34f26c0ef60f2209af62873ae6b347ba14d1d02.
It remains unverified client-import metadata, not a BDS-loadable package.
No Git write was retried; the known read-only index restriction remains.
The next required input is still a local matched server SDK plus BDS/runtime
bundle; isolated load/event-to-DB proof has NOT occurred.

## Pinned-header full native DLL link diagnostic — 2026-11-05

BDS_ACCEPTANCE: BLOCKED_BINARY_INPUTS
DLL_KIND: UNVERIFIED_CLIENT_IMPORT_DIAGNOSTIC

Read the priority files first. Revalidated all **22,202** official catalog
blobs (zero missing/copied/normalized) and compiled the actual mod.cpp/runtime.cpp
against the pinned common/server headers and exact dependency header records.
Then compiled the five native capture core sources (admin_api, event, rollback,
sqlite_api, storage) and linked the complete native DLL with static SQLite.
No core/dashboard/runtime source or normal release guard changed.

The reference library is **not a matched server SDK**. It is the old client
tree's LeviLamina.lib, SHA256
`cd5e008b847da5da12049cecec0f4bbe2ae104126914f7d5ccebfe4d4c1bacaa`.
LLVM inspection found 1,041 named x64 import records plus three descriptor
records, no executable sections and no linker directives. The diagnostic
restricts its hash and import metadata before use and rechecks the hash after
linking. This does NOT establish current server exports, object ABI, allocator
execution or game-hook compatibility. No LeviLamina SDK library was fabricated
and no external SDK was copied; the linker emits the mod's own import library.

Actual new artifact (built **02:29:12 UTC**, hash independently rechecked):

```text
.build/pinned-sdk-link-probe/run-d370c5fc2a5e4e58b6d3cb3785fe73b4/BedrockInsight.dll
SHA256: 32d25d9303a3884c1c8cc629e34f26c0ef60f2209af62873ae6b347ba14d1d02
size: 1,529,344 bytes
COFF-x86-64 / IMAGE_FILE_MACHINE_AMD64 / IMAGE_FILE_DLL
exports: ll_mod_load, ll_memory_operator_overrided
22 named imports from LeviLamina.dll; remaining imports are Windows/MSVC CRT
no bedrock_runtime.dll or sqlite3.dll import
```

That fresh diagnostic directory contains build-evidence.json, core-compile.log,
link.log, pe-inspection.txt and NOT_A_BDS_PACKAGE.txt. Evidence explicitly has
compile_probe=true, sdk_is_server=false, sdk_version_matches_2660=false,
matched_server_sdk_verified=false, packaged=false, launch_authorized=false,
bds_load_verified=false. Nothing was written to normal release output or dist.
Compiler arguments, dependency identities and actual native object hashes are
retained in the diagnostic evidence and refreshed header compile evidence.
Latest header object SHA256s (older hashes below are historical):
mod.obj `183e07bc850267d3e6cf3262be4930e2f78187e8278dcc693ae867376b776c1f`;
runtime.obj `9c8a774e092b0bd642ea1bd8d5e51dfd82e91387a02098b5be3843099dc852ec`.

Actual commands/checks, with the documented LLVM/SQLite/package roots:

```text
scripts/verify_import_metadata.ps1
PS7 + Windows PS5.1: PASS, exit 0 (8 rejection cases; text fixtures only)

scripts/link_pinned_sdk_probe.ps1
Official header staging: verified=22202 missing=0 copied=0
Official 26.60.0 server header compile: PASS (objects only; no DLL link or BDS load)
PINNED-HEADER NATIVE DLL LINK PASS: reference import metadata remains UNVERIFIED for server/BDS
exit=0

scripts/build_levilamina.ps1 (normal release, same client reference root/library)
SDK mismatch: requires proven LeviLamina 26.60 server build/package ...
exit=2

Actual new diagnostic build-evidence.json passed to isolation policy:
rejected as non-server/probe evidence: PASS
normal server release evidence exists=False; current release ZIP exists=False
```

Focused independent static review requested with gpt-6.1-sol/xhigh, agent
01a1a985-dddb-7b91-87b3-3f0dc7445052: **no NEW high/critical finding** in
the diagnostic helper, import metadata policy/tests or compile_options evidence
addition. Requested parameters were accepted; backend model/effort cannot be
independently attested. Reviewer did not run the build or certify binary ABI.
Native/core/fake-SDK suites were not rerun because native/core sources did not
change. Existing SDK compiler warnings remain in header compile.log.

A read-only Win32_Process/CIM query during progress diagnostics was denied
(HRESULT 0x80041003); no process ownership proof was inferred. Full-candidate
process inventory was then unverified; the later read-only .NET follow-up
above resolves this metadata-access obstacle, not full isolation proof.
No BDS/DLL was loaded, no world/event-to-DB behavior was tested, and no server
package is claimed. At 02:34:26 UTC all six pinned SDK/data/prelink cache paths
were still absent and downloaded files=0. Matching inputs remain unacquired;
the proxy-transfer blocker is detailed below. No Git write, live server, other
project or external SDK/cache mutation
was attempted. Do not repeat this diagnostic as runtime acceptance evidence;
the next required input is the matched server SDK/BDS/runtime bundle.

## Read-only isolated-BDS candidate preflight — 2026-11-05

BDS_ACCEPTANCE: BLOCKED_BINARY_INPUTS

Read the priority files first. Implemented the checklist's isolation preflight
without launching, installing or claiming a BDS environment. New code:
scripts/validate_bds_isolation.ps1, scripts/check_bds_isolation.ps1 and
scripts/verify_bds_isolation.ps1. No native/core/dashboard code changed.

The checker validates project-local run-GUID paths and rejects reparse points
in ancestors or descendants before traversing them. It requires normal server
release evidence, current profile/dependency definitions and the staged mod's
SHA256, never the historical compile probe. The minimal candidate allows only
BedrockInsight/LeviLamina mods, no worlds directory, empty allowlist/permissions,
online authentication, a single visitor, cheats/LAN visibility disabled and
the exact unique world name. It checks distinct high ports against read-only
UDP/TCP listener inventory and rejects a running BDS in the candidate tree.
Those real network/process collection paths have not run against a full bundle;
the current CLI stops earlier because required inputs are missing.

Actual checks:

```text
scripts/verify_bds_isolation.ps1
PowerShell 7: exit=0
Windows PowerShell 5.1 (-File): exit=0
BDS isolation candidate policy: PASS
(20 metadata rejections + 2 junction rejections; no launch/EXE/DLL fixtures)

scripts/check_bds_isolation.ps1 -SandboxRoot .
Isolation root must be a unique project-local ... directory
exit=2

scripts/check_bds_isolation.ps1 -SandboxRoot <missing project-local run directory>
Missing isolation directory
exit=2

scripts/check_bds_isolation.ps1 -SandboxRoot <empty project-local run directory>
Missing normal server release build evidence; compile probes cannot pass
exit=2 (direct Windows PowerShell -File invocation)
```

The empty smoke-test directory is
.build/isolated-bds/run-b8c195e74ee04aca90cd286414cda332; directory/junction
fixtures are retained under .build/isolation-policy-tests. No BDS/LL binary,
world, fake server package or generated SDK header was created. One attempted
PowerShell 7 process failed to spawn before executing any code; the clean
Windows PowerShell invocation above completed the smoke test. An earlier
-EncodedCommand wrapper mapped the same rejected preflight to exit 1; the
canonical direct -File check returned exit 2 as shown.

Passing policy fixtures is **not isolation certification or launch approval**.
The report always sets launch_authorized=false and bds_load_verified=false.
Hardlinks/ACLs, complete runtime binary provenance and port/process races are
explicitly outside these read-only candidate checks. They must be resolved
before an actual isolated launch; no launcher was added.

Independent read-only review requested with gpt-6.1-sol/xhigh, agent
01a1a979-51ce-7431-8783-8ca71ffdcfb0, completed with **no NEW high/critical
finding** in the three scripts. It inspected path/reparse containment,
process/world checks, package/profile consistency and PowerShell control flow.
The tool accepted the requested configuration; the backend model/effort cannot
be independently attested. Main ran the checks above; the reviewer ran no tests
or BDS. See docs/SECURITY_REVIEW.md for the bounded review result and exclusions.
At 02:13:22 UTC, all six pinned LL/server-data/prelink cache-directory checks
were still false, download files=0 and normal release evidence absent. No new
artifact download, DLL link/hash, BDS log or event-to-DB proof exists. No Git
write, live server, other project or external SDK/cache was modified. Missing
matched binary inputs still block the actual DLL/runtime acceptance milestone.

## Host artifact-transfer diagnosis — 2026-11-05

BDS_ACCEPTANCE: BLOCKED_BINARY_INPUTS

Read the three priority files first, then SPEC.md and the current checkpoint.
The native source remains unchanged since the independent focused review;
there was no reason to repeat that audit or unchanged builds/tests.

Checked one additional, proxy-preserving transfer route rather than repeating
the failing shell download. The available Node runtime is on this same Windows
workspace, not a separate cloud download environment. An external undici module
is unavailable. The host's HTTP_PROXY, HTTPS_PROXY and ALL_PROXY each point to
`http://127.0.0.1:9` (no embedded credentials); NO_PROXY contains loopback hosts
only. No environment variable or system/network setting was modified.

Used the built-in HTTPS client's per-request Agent with proxyEnv carrying those
same HTTP/HTTPS/NO_PROXY values, following the
[official proxy API](https://nodejs.org/api/http.html#built-in-proxy-support).
No global client or direct/proxy-bypassing fallback was used. The sole bounded
HEAD request targeted the pinned official LL 26.60.0 server release archive.
Actual result at **02:00:37 UTC**:

```text
ECONNREFUSED
address=127.0.0.1 port=9 elapsedMs=23
transferredArchiveBytes=0
```

Thus this alternate client also cannot reach the configured proxy. It did not
produce an archive/import library, change xmake inputs, link a DLL or start BDS.
Remote connector file references remain distinct from readable local SDK bytes.
No new release artifact/hash or loaded/enabled log is claimed. The verified
header snapshot and earlier object hashes below remain prior build evidence.
No Git write, external SDK/cache mutation, other-project or live-server change
was made. Continue only when a readable matched server SDK/BDS/runtime bundle
is supplied or permitted artifact transfer is restored; do not clear the proxy,
substitute client inputs or add unrelated features to conceal this blocker.

## Independent focused review / binary-input recheck — 2026-11-05

BDS_ACCEPTANCE: BLOCKED_BINARY_INPUTS

Latest availability check: **2026-11-05 02:02:56 UTC**. A recursive project
scan found zero LeviLamina.lib/LeviLamina.dll, bedrock_server*.exe or 26.60
server ZIP candidates. The three pinned LL/server-data/prelink directories
remain absent in both inspected caches. The two verified-header object hashes
below remain prior build evidence; no new build was performed. No native source
changes were made in these local-only rechecks. The configured HTTPS proxy is
still 127.0.0.1:9; the refused connection probe was not repeated.
This local-only recheck did not retry downloads, builds/tests or Git writes,
and did not launch BDS. A readable matched bundle or restored permitted
artifact transfer is still required; no new DLL/load evidence was produced.
An additional read-only check found zero named BDS/LL candidates in Downloads
and no archives in LL2660Transfer. The SDK Update Kit archive inventory contains
26.51 client catalog data and development tools, not a matched server bundle;
none was extracted, executed or modified. The binary-input blocker is unchanged.
A direct request for the readable local matched bundle path is pending. The
latest recheck found zero files in .build/bds-downloads; no network/Git retry,
build, test or server launch was performed for this check.

Read BDS_LOAD_FIRST.md, REVIEW_BEFORE_BDS.md and AGENTS.md first, then SPEC.md,
status and the recorded review. Launched an independent read-only reviewer
with explicit `model=gpt-6.1-sol`, `reasoning_effort=xhigh` (agent
`01a1a962-d342-7602-967c-590090936075`). The tool accepted that configuration;
the reviewer cannot independently attest the backend model/effort.

Result: **no NEW critical/high issue found** in the focused source audit.
The reviewer traced native load/enable/disable/unload and shared callback
lifetime/generation, copied event identity, bounded FIFO admission, durable
event-ID resume, transactional migration/event writes, failure rejection and
worker joining. Rollback still rejects before world callbacks in both journal
apply and API dispatch; the native mod starts no HTTP listener. Existing fixes
were not counted again. See docs/SECURITY_REVIEW.md for scope and limits. No
source patch or unchanged test/build rerun was justified by this inspection.
This does not certify game-object ABI, allocator/hooks, real shutdown, or
production cryptographic hashing. HTTP and world mutation remain disabled.

Binary checks this turn (cache/candidate scan and bounded download attempt
completed by **01:45:17 UTC**; connector/import inspections followed):

- Both inspected package caches still lack LL 26.60.0, server.5 runtime data
  and prelink 0.8.8; only the older versions listed below were found.
- Additional read-only candidates were rejected: Production-LinkAudit's
  library is from a target_type=client / 26.51.9 tree; BootLab and FullHookTrial
  loader manifests report 26.60.5+lab in Minecraft.Windows client labs. These
  are not matched server inputs and were neither copied nor executed.
- One bounded `scripts/acquire_bds_inputs.ps1` attempt returned helper exit 2 /
  curl exit 7 through the configured proxy after 2040 ms. No proxy/security
  setting was changed. The download directory still contains zero files.
- The GitHub connector retrieved server CI artifact 13233696219 again to a
  remote file reference only. No locally transferred/hash-verified SDK archive
  or matching import library is claimed. No temporary signed URL was persisted.
- Inspected the existing pinned-header objects with llvm-nm and the historical
  DLL probe with llvm-readobj. The native adapter's inspected imports use LL
  exports, not direct game symbols; the probe has no sqlite3.dll import (the
  selected SQLite package records shared=false). This is diagnostic evidence,
  not a new DLL link or approval to use the old client library.

No release package, isolated BDS environment or loaded/enabled/event-to-DB log
was produced. No BDS process, live world, other project, external SDK/cache or
Git index was modified; no blocked Git write was retried. The verified-header
object hashes below remain the latest build evidence, not this turn's rebuild.
Next requires a readable matched LL 26.60.0 server import library/loader plus
the locked BDS/runtime bundle, or restored permitted artifact downloads. Stop
here rather than manufacture compatibility evidence or add unrelated features.

## Official header reconstruction — 2026-11-05

BDS_ACCEPTANCE: BLOCKED_BINARY_INPUTS

Read the priority files first. Continued SDK reconstruction instead of adding
application features or rebuilding the old DLL. The earlier missing-header
blocker is superseded here; matching binary/runtime requirements remain.

Fetched the official recursive Git-tree catalog at pinned release commit
`072b9ffe0e9d9e0ec846f11d294880e892fe7770` through the connected GitHub API.
Selected **22,196 public common/server headers**, plus COPYING/COPYING.LESSER,
both EULAs, xmake.lua and tooth.json: **22,202 catalog files**. Reused 21,877
exact local matches from read-only references; fetched the other 325 files
(2,029,432 catalog bytes) at that commit, checking returned blob identity.
No SDK Git command, package install or external write is part of staging.

Snapshot: `.build/pinned-sdk-headers-26.60.0/source/`.
Full local verification: **22,202 matching Git text blobs; zero missing**.
CRLF checkouts compare as canonical LF. One imported final LF was removed only
after the exact expected blob validated the change; an independent raw-byte
check confirmed ItemRegistryPacket.h has Git hash
`3c2b3cbfbe41ce106e89dbfcc4797218dc85a9c3`.
Catalog SHA256:
`1ae4dd1e4f54f7da1c9d8a88e29f0897109f052f91f2ef12d2c1ff9da76c5823`.
catalog.json, acquisition-evidence.json and headers-evidence.json retain the
provenance. This is **headers only**, not a complete source checkout or SDK.
HEADERS_ONLY.txt warns not to execute its metadata xmake.lua as a loader build.

New scripts stage/revalidate exact blobs without Lua execution or cache writes,
test ten path/profile/digest rejections, and compile actual mod.cpp/runtime.cpp with
LL_PLAT_S /O2 /MD /std:c++20. The compile uses 28 exact dependency records and
28 cache include paths matching official source declarations or pinned
transitive manifests. No old generated mc/ll include root is passed. The
source's FMT/EnTT public definitions are retained; no library is linked. Import
requires the full catalog SHA256 pinned in packaging/bds-profile.json; a valid
subset claiming the official commit is rejected before copying any headers.

Actual verification with the existing LLVM/SQLite/package environment roots:

```text
scripts/verify_sdk_header_staging.ps1
SDK header staging path/profile tests: PASS (10 rejection cases; no header copies)

scripts/verify_bds_profile.ps1
BedrockInsight BDS profile tests: PASS (metadata only; no SDK/BDS load)

scripts/verify_pinned_sdk_headers.ps1
Official header staging: verified=22202 missing=0 copied=0
Official 26.60.0 server header compile: PASS (objects only; no DLL link or BDS load)
exit=0

.build/pinned-sdk-header-compile/mod.obj
SHA256: 1295a5f7d850dedca3526fdd4ac9a13937f27390094a31bdeac7583b006ffc89
.build/pinned-sdk-header-compile/runtime.obj
SHA256: 0f4e39a9c1fac175b7003a290ecb8c3ff68413cab4e103b9870ce992ddd94332
llvm-readobj: both COFF-x86-64 / IMAGE_FILE_MACHINE_AMD64
mod.obj directives: /EXPORT:ll_mod_load /EXPORT:ll_memory_operator_overrided,DATA
```

Exact compiler arguments, selected dependency manifests and object hashes are
in `.build/pinned-sdk-header-compile/compile-evidence.json`; compiler output is
retained in the adjacent compile.log. Object hashes and x64/export directives
were rechecked after the final compile. Existing SDK warnings remain (28 per
translation unit); no ABI/load
claim is made. An initial slow staging run was interrupted before copying;
the optimized run and full revalidation completed. Native/core runtime sources
were unchanged; prior core/fake-SDK suites were not rerun. The old client-
library DLL probe/hash below is historical and was not rebuilt or relabeled
using these new objects.

The normal release command with this headers-only root rejects packaging with
SDK mismatch / exit 2. A fresh binary acquisition attempt at 01:33:35 UTC
failed through the configured proxy: curl exit 7 after 2040 ms; helper exit 2.
No binary archive, current release package or BDS loaded/enabled log exists.
Next: acquire/verify the matching 26.60.0 server import library/loader, locked
runtime data/prelink and BDS 1.26.60.6 environment, then build/package and prove
isolated load/shutdown/event-to-DB behavior. HTTP/world mutation stay disabled.
No BDS, other project, external SDK/cache or Git index was modified; no Git
write was retried. PROJECT_STATUS remains IN_PROGRESS.

## SDK acquisition checkpoint — 2026-11-05

BDS_ACCEPTANCE: BLOCKED_INPUTS

Last availability recheck: 2026-11-05 01:11:52 UTC (2026-11-05 JST).
Local-only checks: LL 26.60.0, server.5 data and prelink 0.8.8 are absent in
both inspected package caches. Package inventories and retained probe hash
are unchanged. The download directory contains zero archives; a recursive
project scan found zero bedrock_server*.exe or LeviLamina.lib files.
The last bounded acquisition attempt (01:07:04 UTC) returned helper exit 2 /
curl exit 7 through the configured proxy after 2036 ms; it was not repeated
on this local-only recheck. No source/test change, rebuild, Git write
or BDS launch was performed. Further runtime work requires the external inputs
below; repeating unchanged builds is not load acceptance evidence.

Read the three priority files first, then SPEC.md, current status and the
focused review. No new matched bundle appeared under the project or the
inspected SDK/package roots. Checked the exact release commit
`072b9ffe0e9d9e0ec846f11d294880e892fe7770` in the Assistant, Official-Audit and
NormalHooks SDK Git object stores. All three return `cat-file` exit 128 with
GIT_NO_LAZY_FETCH=1, GIT_OPTIONAL_LOCKS=0 and GIT_TERMINAL_PROMPT=0: the exact
source snapshot is not available from those local objects. Initial cat-file
reads in the partial clones attempted automatic lazy fetch and failed through
the proxy; subsequent checks explicitly disabled it. The existing Assistant
FETCH_HEAD still has its October 30 timestamp. No SDK/index write is claimed
or requested, and no Git add/commit was attempted.

Refreshed package-directory inventory still shows only LeviLamina 26.51.5,
runtime data server.6/client.6/client.12 and prelink 0.8.6/0.8.7. The bounded
`scripts/acquire_bds_inputs.ps1` attempt again failed with curl exit 7 through
the configured 127.0.0.1 proxy (helper exit 2). The local download directory
has no acquired archive. No proxy/security setting was changed.

The existing probe's SHA-256 still matches build-evidence.json:
`916c6a81cef1a8140674e815c5cedc08818748b5130314a46003feecdc63fe62`.
It remains explicitly unverified, with bds_load_verified=false; no current
release ZIP exists. No implementation changed and no unchanged test/build was
rerun or relabeled as new acceptance evidence. No BDS was launched or touched.

Required external input: a readable local matched LL 26.60.0 server SDK
(generated headers and import library), BDS 1.26.60.6 and server.5 runtime-data/
prelink 0.8.8/runtime dependencies, or restored permitted artifact downloads.
The existing request for a bundle path remains pending. Do not resume unrelated
features, bypass the SDK guard or fabricate load logs while this input is
missing. The overall project remains IN_PROGRESS.

## Native shutdown lifetime follow-up — 2026-11-05

Read BDS_LOAD_FIRST.md, REVIEW_BEFORE_BDS.md and AGENTS.md first, then SPEC.md,
current status and native code. This section supersedes earlier statements
that load starts SQLite and that only unload drains the writer.

The official release's
[DisableAllModsHook](https://github.com/LiteLDev/LeviLamina/blob/072b9ffe0e9d9e0ec846f11d294880e892fe7770/src-server/ll/core/main_win.cpp)
calls disableAllMods at ServerInstance::leaveGameSync, not unload. Our
disable previously only toggled capture, leaving a live SQLite worker until
unload/global destruction. A loaded-but-never-enabled mod also opened a
worker, although disableAllMods skips disabled mods. This exposed normal
shutdown to lost pending writes and thread synchronization during DLL detach;
[Microsoft's loader guidance](https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-best-practices)
warns against that cleanup pattern. No actual BDS exit deadlock was executed
or claimed reproduced; the risk comes from tracing the code paths.

Fixed the native lifecycle, without changing adapter-neutral core features:

- Load now records configuration only: no SQLite connection, worker or hooks.
- Enable starts a fresh one-shot writer, then registers the typed listeners.
  A failed startup/registration closes and joins that activation's worker.
- Disable closes admission, drains/joins storage outside the capture mutex,
  and verifies accepted/committed counts and terminal health. Re-enable opens
  the same DB and resumes IDs; prior loss/failure is not reset to healthy.
- Unload stops storage before listener removal, including when failed removal
  must deny logical unload. Disabled/stale callbacks cannot access the writer.

Actual checks using the toolchain/input roots in SECURITY_REVIEW.md:

```text
scripts/verify_native_runtime.ps1
BedrockInsight native adapter safety tests: PASS (fake SDK; no BDS)
exit=0; /W4 /WX

scripts\build_levilamina.cmd -CompileProbe
COMPILE/LINK PROBE PASS: SDK UNVERIFIED; NOT A BDS LOADABLE PACKAGE
exit=0
DLL: .build/compile-probe/BedrockInsight.dll
SHA256: 916c6a81cef1a8140674e815c5cedc08818748b5130314a46003feecdc63fe62
llvm-readobj: COFF-x86-64; ll_mod_load + ll_memory_operator_overrided

scripts\build_levilamina.cmd
SDK mismatch: requires proven LeviLamina 26.60 server build/package ...
exit=2

powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\acquire_bds_inputs.ps1
curl: (7) Failed to connect to github.com:443 over proxy 127.0.0.1 ...
Archive acquisition failed: curl exit 7 (...server-release-windows-x64.zip)
exit=2
```

The expanded adapter test checks no filesystem/database side effects on load,
failed SQLite startup, registration failures at all four listener positions,
and worker connection closure after disable, failed activation/re-activation
and failed unload. Its journal-mode probe fails while the worker is active
and succeeds after lifecycle cleanup, demonstrating connection release.
SQLite readback verifies 136 events with MAX(event_id)=136, 130 attempt audit
rows and two closed sessions across activations, including a 128-event burst
drained on disable. No real DLL detach/BDS stop was tested. Core and metadata
sources were unchanged; their prior suites were not rerun for this change.
Compiler output/evidence are in .build/compile-probe/build.log and
build-evidence.json. Existing SDK warnings remain; older hashes are historical.

Refreshed cache inspection still finds only server.6/client.6/client.12, not
the locked v26.60.6-server.5. The SDK remains a client build advertising
26.51.9. Downloads remain blocked by the configured proxy. A request for a
local matched bundle path is pending; do not bypass network/security settings
or substitute client/old inputs. No release package or BDS loaded/enabled log
exists. No BDS, other project, live server, external SDK/cache or Git index
was modified. No blocked Git write was retried. PROJECT_STATUS: IN_PROGRESS.

Next acceptance requirement is unchanged: obtain LL 26.60.0 server SDK headers/
import library, BDS 1.26.60.6, server.5 runtime data/prelink 0.8.8 and the locked
runtime dependencies, then build/package and prove local world/process/port
isolation before real load, shutdown and event-to-DB verification.

## Official release / native loader follow-up — 2026-11-05

This section supersedes the older server.12 and plugins/ assumptions below.
Read BDS_LOAD_FIRST.md, REVIEW_BEFORE_BDS.md and AGENTS.md first, then reviewed
actual native code against official release sources. No unrelated features
were added and no Git write was retried.

The GitHub connector could read the official
[v26.60.0 release](https://github.com/LiteLDev/LeviLamina/releases/tag/v26.60.0)
and commit `072b9ffe0e9d9e0ec846f11d294880e892fe7770`. Its
[Tooth manifest](https://github.com/LiteLDev/LeviLamina/blob/072b9ffe0e9d9e0ec846f11d294880e892fe7770/tooth.json),
the versioned xmake recipe and BDS v1.26.60 manifest establish the target:

- BDS **1.26.60.6**, LeviLamina **26.60.0**, PreLoader **1.16.7**;
- `bedrockdata v26.60.6-server.5`, requiring `prelink v0.8.8`;
- native mod placement **mods/BedrockInsight/**, not plugins/.

Official URLs, source blob IDs, release commit, archive sizes/digests and CI
artifact ID are in `packaging/bds-profile.json`. Release validation now checks
this profile, exact loader dependency and EnTT page definitions.
`scripts/acquire_bds_inputs.ps1` provides bounded, hash/size-checked downloads
of two official dependency archives into this repository; it never extracts,
installs, launches software or changes proxy/security settings. It is not a
complete SDK/BDS installer.

Fixed two additional native defects before any runtime launch:

- Official NativeModManager refuses DLLs without
  `ll_memory_operator_overrided`. The previous real probe exported only
  `ll_mod_load`. Included the SDK's actual MemoryOperators.h overrides once
  with LL_MEMORY_OPERATORS, not a fake marker. Build acceptance requires both
  exports; import inspection confirms getDefaultAllocator/throwMemoryException.
- Official EventBus removes listeners after mod disable. Re-enable previously
  claimed success without restoring capture. Registration now occurs on
  enable, unwinds/retries partial activation and replaces retained/auto-removed
  handles without duplication. An activation generation rejects delayed
  snapshots from earlier activations, including after re-enable. Load opens
  storage but installs no game hooks.

Actual verification using the input roots in SECURITY_REVIEW.md:

```text
scripts/verify_native_runtime.ps1
BedrockInsight native adapter safety tests: PASS (fake SDK; no BDS)
exit=0; /W4 /WX

scripts/verify_sdk_inputs.ps1
BedrockInsight SDK input resolution tests: PASS

scripts/verify_bds_profile.ps1
BedrockInsight BDS profile tests: PASS (metadata only; no SDK/BDS load)

scripts\build_levilamina.cmd -CompileProbe
COMPILE/LINK PROBE PASS: SDK UNVERIFIED; NOT A BDS LOADABLE PACKAGE
exit=0
DLL: .build/compile-probe/BedrockInsight.dll
SHA256: 887cdf241ca751a8476341dd1d22d362f1f4c2b22c68f3076f1c6eef6d48fd15
llvm-readobj: COFF-x86-64; ll_mod_load + ll_memory_operator_overrided

scripts\build_levilamina.cmd
SDK mismatch: requires proven LeviLamina 26.60 server build/package ...
exit=2

powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\acquire_bds_inputs.ps1
curl: (7) Failed to connect to github.com:443 over proxy 127.0.0.1 ...
Archive acquisition failed: curl exit 7 (...server-release-windows-x64.zip)
exit=2
```

Native tests cover SDK-style automatic removal, partial re-enable failure/
retry, retained handles, idempotent enable and stale snapshots across
activations. Real SQLite readback verifies eight events/two closed sessions
with no stale-snapshot writes. The profile suite covers a positive case and
eight rejections. These are adapter/metadata tests, not game hook/ABI proof.
Core sources were unchanged; prior core results were not rerun.

The final native build used the clean cmd.exe wrapper invocation. An earlier
wrapper call inherited the PowerShell 7 module environment and its Windows
PowerShell child failed to resolve Get-FileHash; a direct PowerShell 7 probe
also passed. Final compiler output is `.build/compile-probe/build.log`, with
existing SDK warnings; arguments, inputs/profile hashes and required exports
are in build-evidence.json. Earlier DLL hashes below are historical.

Acquisition: the GitHub connector retrieved release-run 41641646298 server
artifact 13233696219 to a connector file reference. The local shell could not
transfer it through the unreachable proxy; no browser surface is available.
No official archive is claimed installed or locally hash-verified. Refreshed
caches still have only server.6/client.6/client.12 and prelink 0.8.6/0.8.7.
The source SDK still selects client and reports 26.51.9; its closure correctly
fails the new server.5 profile. There is no current release package or BDS
loaded/enabled log. No SDK tree, other project or live server was modified.

Next: acquire official matched 26.60.0 SDK headers/import library, server.5
data/prelink 0.8.8 and BDS 1.26.60.6 runtime dependencies. Recheck provenance,
build the real package, and prove repository-local process/world/port isolation
before launching. HTTP and world mutation stay disabled. Actual load,
shutdown and event-to-DB acceptance remain unverified; status is IN_PROGRESS.

## SDK dependency matching follow-up — 2026-11-05

Read the three priority files again, checked the native lifecycle/persistence
fixes against the implementation, and continued package-source/cache
diagnosis. The download caches contain only the older client.6/server.6
archives; no server.12 or 26.60 SDK archive was found. The official release
API request still fails with curl exit 7 through the unreachable configured
proxy. A request for another local bundle path is pending.

The focused build audit found another high-impact risk: the manual build
recursively added every package include directory, placing parallel-hashmap
v1.3.12 before the SDK-recorded v2.0.0, and did not carry public EnTT layout
definitions. This could compile against different public header layouts even
with a correct loader. Fixed this in scripts/resolve_sdk_inputs.ps1 and
scripts/build_levilamina.ps1:

- Source builds use only the exact roots in their xmake references record;
  installed SDKs use manifest dependency version/buildhash records. Each
  selected manifest is checked and hashed. No Lua/xmake evaluation, package
  download, or cache write occurs.
- Source public definitions and installed package exported definitions are
  passed to clang-cl. Missing exact dependencies, conflicting definitions,
  duplicate dependency versions, invalid metadata/paths, and generated-header
  shadowing fail closed. Release packaging additionally requires the audited
  v26.51.1-server.12 runtime-data record.
- The actual source closure selects 32 roots, 29 include directories,
  parallel-hashmap v2.0.0, prelink v0.8.7 and public
  FMT_USE_FULL_CACHE_DRAGONBOX=1 / ENTT_PACKED_PAGE=128 /
  ENTT_SPARSE_PAGE=4096. No parallel-hashmap v1.3.12 include is selected.
  The cached server 26.51.5 closure separately resolves 20 exact roots,
  server.6 data and its exported ENTT_NO_MIXIN / LL_PLAT_S flags; it remains
  a version mismatch and is not used for release packaging.

Actual checks for this follow-up:

```text
& ./scripts/verify_sdk_inputs.ps1
BedrockInsight SDK input resolution tests: PASS

scripts\build_levilamina.cmd -CompileProbe
COMPILE/LINK PROBE PASS: SDK UNVERIFIED; NOT A BDS LOADABLE PACKAGE
exit=0
DLL: .build/compile-probe/BedrockInsight.dll
SHA256: 8af1351c5c532d52265acf953c40d9b01699b25e45452a2a2225dd1e881c3c24

scripts\build_levilamina.cmd
SDK mismatch: requires proven LeviLamina 26.60 server build/package ...
exit=2
```

The focused resolver suite covers installed/source selection and eight
rejection scenarios. Actual artifact evidence confirms the recorded versions
and compile definitions. The new compile probe supersedes the earlier hash;
compiler arguments and dependency manifest hashes are in
.build/compile-probe/build-evidence.json and compiler output in build.log.
Runtime/core sources were unchanged in this follow-up; their prior passing
regressions are retained below, not claimed as rerun here.
No current release package or BDS loaded/enabled log exists. No SDK tree,
other project, live server or Git index was modified.

## Current review checkpoint — supersedes previous native-build claims

Read BDS_LOAD_FIRST.md, REVIEW_BEFORE_BDS.md and AGENTS.md first. The focused
implementation audit and fixes are recorded with severity, source locations,
reproducers and limitations in docs/SECURITY_REVIEW.md.

The earlier statement that the DLL was built against a matched 26.60 server
library was incorrect. The selected source build's
`.xmake/windows/x64/xmake.conf` explicitly says `target_type = "client"`;
its `bin/LeviLamina/manifest.json` declares `26.51.9+8b4ef79d`.
The source `tooth.json` declaration of 26.60.0 is not binary provenance.
The other inspected Dev source build is also configured for the client.
The cached server package is 26.51.5 with runtime data server.6, while the
26.60 source declaration requires BDS 1.26.60 and runtime data server.12.
Only client.12 was found in the Dev runtime-data cache.

Fixed before any BDS runtime attempt:

- SQLite startup/worker failure left a joinable thread unjoined and could
  terminate the host during destruction. Lifecycle is serialized, failures
  reject admission, workers are joined, and unexpected worker exceptions
  become terminal health errors.
- Reopening a database restarted event IDs at 1. Startup now resumes from
  durable MAX(event_id) before producers are admitted. Flush predicate
  updates synchronize with waiters and the control queue is bounded.
- Partial registration leaked callbacks capturing raw this; callbacks raced
  disable/unload. Capture now has shared ownership and synchronization,
  owns handles before registration, retains failed removals and refuses
  logical unload. Because the audited SDK snapshots callbacks, the Windows
  callback module is pinned until process exit; hot replacement is unsupported.
- Removed direct generated Player storage/position reads and display-name
  fallback identity. LL-exported getUuid/getRealName preserve all UUID bits;
  an unavailable UUID remains Unknown. Duplicate joins are suppressed.
- SDK block emitters run before origin(). The adapter now records explicit
  attempts with empty before/after columns, not completed-action metrics or
  reversible snapshots. No duplicated metric/audit queue pair is emitted.
- Rollback application is disabled unconditionally before world callbacks.
  The API also rejects admin apply with 501 even if a provider is injected.
  Type-only preflight cannot provide NBT/state/compensation/crash recovery.
- Invalid API roles are rejected; missing/throwing hashers fail closed.
  Socket provider/parser exceptions no longer escape the noexcept worker.
  The native capture build initializes no HTTP server, token store, hasher,
  dashboard or world writer.
- Added the required root manifest.json, corrected server Tooth metadata/
  plugins placement/ZIP roots, and pinned xmake to server LeviLamina 26.60.0
  and levibuildscript 0.6.1. A build guard rejects client/26.51 provenance;
  `-CompileProbe` can only emit explicitly unverified artifacts outside dist.

Actual verification on 2026-11-05:

```text
scripts\verify_core.cmd
BedrockInsight core tests: PASS
BedrockInsightCoreTests exit=0

powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\verify_native_runtime.ps1
BedrockInsight native adapter safety tests: PASS (fake SDK; no BDS)
exit=0; /W4 /WX

scripts\build_levilamina.cmd
SDK mismatch: requires proven LeviLamina 26.60 server build/package ...
exit=2
```

The core suite includes 1,200 events across three reopen cycles with four
producers, injected SQLite transaction failure and failed startup/destruction,
future schema rejection, disabled two-location rollback writes and a real
loopback request after a throwing provider. The adapter harness checks
registration failure at each of four positions, removal failure/retry,
callbacks retained after Runtime destruction, duplicate/cancelled joins,
renamed/unknown identity, and attempt-only audit rows through real SQLite.
This harness does not validate BDS layouts or hook execution.

Exact compiler/input commands are in docs/SECURITY_REVIEW.md and the generated
`.build/compile-probe/build-evidence.json`. Core verification output is in
`build/manual/security-core-tests.log`; native compiler output is in
`.build/compile-probe/build.log`. The existing SQLite `__int64`/CRT
deprecation and SDK generated-header warnings remain on the real-SDK build.

Final actual compile/link probe (LLVM 23.1.4 clang-cl, /O2 /MD /std:c++20,
LL_PLAT_S; minimal native capture/storage sources, real LL_REGISTER_MOD):

```text
scripts\build_levilamina.cmd -CompileProbe
COMPILE/LINK PROBE PASS: SDK UNVERIFIED; NOT A BDS LOADABLE PACKAGE
exit=0
DLL: .build/compile-probe/BedrockInsight.dll
SHA256 (historical, before exact dependency selection): 95e78b93d90e58b2d68133edf7984709024fc1c39f28e6f95b8312ab6a0fac12
llvm-readobj: COFF-x86-64; ll_mod_load export
SDK evidence: sdk_is_server=false; sdk_version_matches_2660=false
```

The standard package command also rejects the cached 26.51.5 server package
with exit 2. Native/Tooth source JSON parsing and loader/server/version/
placement assertions passed. The package-manager install path has not been
executed or accepted by BDS. No current release ZIP/hash is claimed.

Historical unreviewed artifacts were moved intact (recoverable, none deleted):

- `dist/quarantine-pre-review-20261105/BedrockInsight-server/`
- `dist/quarantine-pre-review-20261105/BedrockInsight-server-win-x64.zip`
- `.build/quarantine-pre-review-release-20261105/`

No current release package is emitted from the mismatched inputs.
No BDS process was started, stopped, attached to or modified. Recursive
filename-only inspection of C:\Dev found no bedrock_server*.exe. A bounded
official GitHub release acquisition request failed with curl exit 7:
`Failed to connect to api.github.com:443 over proxy 127.0.0.1 ... Could not connect to server`.
No proxy setting was changed. Existing servers, SDK trees and other projects
were only consulted read-only. Git writes were not retried.

Next acceptance step: obtain the matching 26.60 server loader/import library,
server.12 runtime data and BDS 1.26.60 distribution; prove a new repository-local
world/ports/process path is isolated; build/package and record real
loaded/enabled logs and captured DB rows. HTTP and rollback must remain
disabled. There is no real BDS-load/event-to-database proof yet.

## Completed

The entries below preserve historical checkpoints. Their native adapter,
packaging and rollback-apply claims are superseded by the review above.

M0 SDK/build audit:

- Read `SPEC.md`, `AGENTS.md`, and this status file before changing code.
- Inspected the local BedrockDynamicSDK 26.60 reference and LeviLamina
  26.60 source/API trees read-only. The event/API findings are recorded in
  `docs/SDK_AUDIT.md`.
- Established the xmake project shape and an optional CMake entry point.
- Confirmed the relevant LeviLamina event types and `LL_REGISTER_MOD` entry
  contract, but did not guess at the generated `mc/*` package ABI.

M1 schema/queue/persistence:

- Added immutable `AnalyticsEvent` values with explicit actor kind, including
  `Unknown`; adapters must preserve unknown causation instead of guessing.
- Added a bounded FIFO `EventQueue` with close/drain semantics and explicit
  back-pressure. Queue submission only copies values and never performs DB I/O.
- Added SQLite schema v1 and migration: players, sessions, raw events,
  audit events, daily aggregates, foreign keys, and lookup indexes.
- Added `StorageWriter`, a dedicated single SQLite connection/worker with
  WAL, `synchronous=NORMAL`, foreign keys, prepared statements, transactions,
  and daily/session aggregate updates.
- Added core tests for FIFO/back-pressure/close, WAL mode, schema version,
  event durability, daily aggregates, audit rows, and unknown actor retention.

M2 aggregation core:

- Added a thread-safe, adapter-neutral `MetricsAggregator` that consumes
  immutable analytics events without database I/O.
- Added per-player counters for blocks/items/kills/deaths/distance/sessions,
  including deterministic per-type maps for blocks, items, and kill targets.
- Added explicit server tick samples with average/max MSPT, capped TPS, current
  online count, and peak online count.
- Added deterministic daily, monthly, and all-time rankings with stable
  value-then-player-id ordering and a limit parameter.
- Extended the core test executable with typed counter, session state, server
  sample, TPS, and ranking coverage.

M2 persistence/query core:

- Bumped the SQLite schema to v2 with a tested v1-to-v2 migration.
- Persisted per-player daily type counters and typed server tick samples
  (MSPT and online count) through the existing single writer transaction.
- Added read-only `AnalyticsQuery` access for daily/monthly/all-time rankings,
  bounded server metric summaries, and daily/monthly/all-time active-player
  counts suitable for a future localhost API/dashboard worker.
- Extended tests to cover fresh v2 databases, v1 upgrades, durable type
  counters, server samples, ranking queries, and DAU-style active-player
  queries.

M3 audit/query core:

- Added structured `AuditFilter` predicates for event time ranges, action,
  actor kind/id, world and dimension, coordinate bounds, before/after types,
  and cause, with bounded newest-first or oldest-first results.
- Added `AuditRecord` projection retaining event identity, actor data,
  position, session, causation, raw details, and audit before/after values.
- Added exact `inspect_audit(event_id)` lookup with a clear not-found error,
  while retaining unknown actors instead of inferring a player.
- Extended tests for precise filtered lookup, stable newest/oldest ordering,
  bounds, limits, exact inspection, and not-found behavior over persisted
  audit rows.

M4 rollback control-plane core:

- Added preview-first `RollbackPlanner` support for reversible block changes,
  including multi-event coalescing at one world position and explicit undo or
  restore direction.
- Added current-state conflict detection, audit-chain validation, world-aware
  locations, and rejection of unsupported container/entity operations so the
  planner cannot guess or duplicate inventory state.
- Added callback-based `RollbackJournal` transactions with complete preflight
  before the first write, prepared/applied/conflict/failed states, per-entry
  results, and idempotent re-apply after success.
- Bumped the SQLite schema to v3 and routed rollback snapshots through the
  existing single SQLite writer, including transaction metadata, coalesced
  source event IDs, world-aware entries, status/error fields, and read-only
  journal summaries.
- Extended tests for coalesced undo, world identity, conflict rejection,
  unsupported operations, journal inspection, preflight safety, idempotent
  application, schema migration, single-writer persistence, and journal query
  readback.

M5 API/security boundary core:

- Added a fixed, adapter-neutral `/api/v1` request router for health, metrics,
  audit lookup, rollback preview/apply, and token-management provider hooks.
- Enforced loopback-only requests by default, with explicit 401/403/404/501
  responses and role-based permissions: viewers can read analytics,
  moderators can inspect audit and preview rollback, and admins can apply
  rollback or manage tokens.
- Added in-memory token records that retain only a caller-supplied hash,
  support revocation, and expose principals without returning secrets. The
  production adapter must inject a cryptographic salted hasher.
- Added a thread-safe bounded admin action log that records allowed and denied
  requests without token material, with deterministic IDs and eviction.
- Extended the core test executable with localhost enforcement, invalid and
  disabled token rejection, role checks, route/provider dispatch, safe action
  logging, and bounded-log behavior.

M5 HTTP transport core:

- Added a dependency-free bounded HTTP/1.1 request parser with `Host`
  validation, case-insensitive headers, `Content-Length` framing, explicit
  incomplete-input handling, header/body/request limits, and rejection of
  duplicate headers and unsupported chunked transfer encoding.
- Added deterministic HTTP response serialization with JSON content type,
  content length, and connection-close framing, plus a bridge that extracts a
  Bearer token and dispatches into the authenticated API core.
- Kept the transport socket-free and JSON-parser-free so it remains safe to
  test without a server process; the future listener owns reads, writes, and
  connection lifecycle.
- Extended tests for complete and incomplete requests, case-insensitive
  headers, body dispatch, malformed requests, request-size limits, duplicate
  headers, and transfer-encoding rejection.

M5 JSON and typed payload core:

- Added a dependency-free JSON value/parser implementation with bounded input,
  nesting, value-count, and string limits; duplicate-key rejection; strict
  number grammar; control-character checks; and Unicode escape decoding.
- Added HTTP JSON-object validation for POST/non-empty request bodies, with
  explicit 400/413 responses before authentication providers receive payloads.
- Added typed rollback payload decoding for operation, transaction ID, reason,
  and non-empty event ID lists. The authenticated principal remains the source
  of actor identity; clients cannot supply an operator ID.
- Added typed audit-filter decoding with range checks for coordinates,
  dimensions, limits, timestamps, and ordering, suitable for a future query
  provider.
- Extended tests for JSON escapes, Unicode, duplicate keys, number/type
  validation, parser limits, HTTP body validation, and typed audit/rollback
  payloads.

M5 dashboard data serialization core:

- Added deterministic JSON serializers for server metrics, rankings with rank
  numbers and period metadata, player profiles including typed counters, audit
  records with positions/details, rollback summaries, and admin action logs.
- Escaped all string fields and serialized `details_json` as a string so
  provider output cannot be corrupted by embedded quotes or malformed raw
  details. Serializer output is validated again through the bounded parser in
  tests.
- Kept serializers independent of SQLite and HTTP transport so dashboard/API
  providers can perform their read-only query work off the server tick path.
- Extended tests for escaping, numeric/boolean fields, nested positions/maps,
  ranking metadata, player profiles, audit records, rollback summaries, and
  admin-log output.

M5 durable administration core:

- Bumped the SQLite schema to v4 with `api_tokens` and append-only
  `admin_action_log` tables plus lookup indexes. Token rows retain only the
  caller-supplied hash, actor, role, creation time, and enabled state.
- Routed token changes and admin action entries through the existing SQLite
  single-writer control queue, with flush/health metrics for accepted and
  committed operations. No request path opens SQLite or performs synchronous
  database I/O.
- Added token-store restore/snapshot support and change sinks, plus admin-log
  action sinks, so a future mod bootstrap can restore v4 metadata and connect
  mutations to persistence without coupling the API router to storage.
- Added read-only query access for restoring API tokens and inspecting bounded
  newest-first admin actions, including role validation on database readback.
- Extended tests for schema-v1 upgrade to v4, fresh v4 durable token/log rows,
  hash-only storage, sink routing, token restore authentication, and query
  readback.

M5 localhost listener core:

- Added a socket-backed `HttpServer` that binds only to IPv4 loopback
  (`127.0.0.1`/`localhost`), uses bounded HTTP request limits, passes numeric
  peer addresses to the existing authenticated API bridge, and always closes
  each connection after one response.
- Added timed receive/send operations, select-based stop polling, deterministic
  cleanup, ephemeral-port reporting, and rejection of non-loopback bind
  addresses. The listener owns no SQLite connection and performs no server
  tick/world access.
- Extended tests with an in-process loopback client covering listener startup,
  metrics response serialization, authentication rejection, non-loopback bind
  rejection, and clean shutdown.

M5 dashboard/provider core:

- Added authenticated read-only `/api/v1/rankings`, `/api/v1/player?id=...`,
  and admin-only `/api/v1/admin/actions` provider routes. Player providers
  receive the complete bounded request target so the adapter can validate and
  decode identifiers without the API core guessing at database semantics.
- Added embedded dashboard HTML, CSS, and JavaScript assets with responsive
  metric cards, rankings, player-profile lookup, token input, and safe DOM
  rendering. The listener serves only exact static paths (`/`, `/index.html`,
  `/styles.css`, `/app.js`) and leaves API authentication/roles unchanged.
- Extended API and socket tests for provider routing, role enforcement, asset
  content types, dashboard content, and the rankings/player fetch paths.

M5 packaging shell foundation:

- Added a server-only `LL_REGISTER_MOD` lifecycle shell with load/enable/
  disable/unload guards. It deliberately does not guess at `mc/*` event or
  world symbols; those adapters remain a separate ABI-gated change.
- Added an opt-in `with_levilamina` xmake target using the server
  `levilamina` package, `levibuildscript` link/mod-packer rules, SQLite, and
  Winsock. The default core/test target remains independent of SDK headers.
- Added `scripts/verify_levilamina_entry.cmd`, a read-only compile-only probe
  that consumes the local LeviLamina source and installed xmake package
  include roots without loading a DLL or contacting BDS.

M5 pre-review native compile/link attempt (historical; rejected for BDS):

- Replaced the lifecycle-only shell with a `Runtime` bound to the audited
  server event headers: `PlayerJoinEvent`, `PlayerDisconnectEvent`,
  `PlayerDestroyBlockEvent`, and `PlayerPlacedBlockEvent`. The callbacks
  preserve cancellation, create session IDs, record block break/place
  metrics plus audit rows, capture block position/dimension/type, and submit
  immutable values to `StorageWriter`; callbacks never open SQLite or perform
  synchronous database I/O.
- This attempt used source commit
  `8b4ef79d110f19952696d9ba6a0f5053e8e0689a` and a client import library;
  its prior matched-server claim is retracted. The generated Player identity
  field/name fallback implementation has been removed in the review fixes.
- Added `scripts/build_levilamina.cmd`, a clang-cl x64 DLL
  build that links the supplied LeviLamina import library, SQLite static
  library, and Winsock, then emits a local mod directory and ZIP without
  starting BDS or copying into any server installation. Added a server Tooth
  manifest targeting LeviLamina `26.60.*`. The review found packaging errors;
  those sources were corrected, and this old package was quarantined.
- The historical build produced `.build/levilamina/release/BedrockInsight.dll`
  (COFF x86-64) and `dist/BedrockInsight-server-win-x64.zip`. The DLL exports
  exactly `ll_mod_load`; import inspection shows `LeviLamina.dll` plus system
  libraries and no `bedrock_server.dll` dependency. Historical SHA-256 values:
  DLL `460499e1f7e593d95f9b3a2909094870fffc31c4d59e1ccbda2f01b1312f1ecb`;
  ZIP `60928c99fa5f7ea5ab827321e87f29a59ec816524f870d3e21ff4d2e09050ae0`.
- Added Windows `NOMINMAX` portability guards to the HTTP implementation and
  core test harness; the MSVC-compatible clang regression path now links
  without the test CRT shim on `_MSC_VER`.

## Verification

Passed on 2026-11-05 (M1 persistence, M2 query core, M3 audit query core, M4
durable rollback journal core, M5 API/security/HTTP/JSON/dashboard-data/
durable-admin/loopback-listener/dashboard-provider core, and the matched
26.60 native DLL/package build):

```text
set BEDROCKINSIGHT_CLANG_ROOT=C:\PraxisRemote\toolchain\llvm-mingw-20260519-ucrt-x86_64\bin&&set BEDROCKINSIGHT_SQLITE_ROOT=C:\Users\chihirosub\AppData\Local\.xmake\packages\s\sqlite3\3.54.0+100\b897e7007f3d49769c227e337e14fdef&&scripts\verify_core.cmd
BedrockInsight core tests: PASS
BedrockInsightCoreTests exit=0
```

The command completed without compiler warnings. It covers fresh schema-v4
creation and upgrade from schema-v1, durable typed counters/server samples,
read-only ranking/active-player and journal queries, filtered audit
lookup/inspect, preview-first rollback planning/journaling, in-memory metrics,
and the M5 loopback/auth/role/API action-log tests. It does not start, stop,
attach to, or deploy to BDS.

The M5 API re-run used this PowerShell invocation after adding the API core:

```text
$env:BEDROCKINSIGHT_CLANG_ROOT='C:\PraxisRemote\toolchain\llvm-mingw-20260519-ucrt-x86_64\bin'; $env:BEDROCKINSIGHT_SQLITE_ROOT='C:\Users\chihirosub\AppData\Local\.xmake\packages\s\sqlite3\3.54.0+100\b897e7007f3d49769c227e337e14fdef'; cmd /c scripts\verify_core.cmd
BedrockInsight core tests: PASS
BedrockInsightCoreTests exit=0
```

The first M5 compile attempt exposed a test-only `std::string` plus
`std::string_view` concatenation error in the provider fixture; it was fixed
before this passing run. The final run completed without compiler warnings.

The later M5 HTTP transport verification used the same installed toolchain:

```text
$env:BEDROCKINSIGHT_CLANG_ROOT='C:\PraxisRemote\toolchain\llvm-mingw-20260519-ucrt-x86_64\bin'; $env:BEDROCKINSIGHT_SQLITE_ROOT='C:\Users\chihirosub\AppData\Local\.xmake\packages\s\sqlite3\3.54.0+100\b897e7007f3d49769c227e337e14fdef'; cmd /c scripts\verify_core.cmd
BedrockInsight core tests: PASS
BedrockInsightCoreTests exit=0
```

This run compiled and linked `http_transport.cpp` with `-Wall -Wextra
-Wpedantic` and passed the expanded transport tests without warnings.

The current M5 JSON/payload verification used this same command after adding
`json.cpp` and `api_payload.cpp`:

```text
$env:BEDROCKINSIGHT_CLANG_ROOT='C:\PraxisRemote\toolchain\llvm-mingw-20260519-ucrt-x86_64\bin'; $env:BEDROCKINSIGHT_SQLITE_ROOT='C:\Users\chihirosub\AppData\Local\.xmake\packages\s\sqlite3\3.54.0+100\b897e7007f3d49769c227e337e14fdef'; cmd /c scripts\verify_core.cmd
BedrockInsight core tests: PASS
BedrockInsightCoreTests exit=0
```

This run compiled and linked the JSON and typed-payload sources with
`-Wall -Wextra -Wpedantic` and passed all core, transport, parser, and payload
tests without warnings.

The current M5 dashboard-data verification used this same installed-toolchain
command after adding `api_json.cpp`:

```text
$env:BEDROCKINSIGHT_CLANG_ROOT='C:\PraxisRemote\toolchain\llvm-mingw-20260519-ucrt-x86_64\bin'; $env:BEDROCKINSIGHT_SQLITE_ROOT='C:\Users\chihirosub\AppData\Local\.xmake\packages\s\sqlite3\3.54.0+100\b897e7007f3d49769c227e337e14fdef'; cmd /c scripts\verify_core.cmd
BedrockInsight core tests: PASS
BedrockInsightCoreTests exit=0
```

This run compiled and linked `api_json.cpp` with `-Wall -Wextra -Wpedantic`
and passed all serializer/parser/provider-core tests without warnings.

The current M5 durable-administration verification used this same command after
adding schema v4, storage control operations, and admin query readback:

```text
$env:BEDROCKINSIGHT_CLANG_ROOT='C:\PraxisRemote\toolchain\llvm-mingw-20260519-ucrt-x86_64\bin'; $env:BEDROCKINSIGHT_SQLITE_ROOT='C:\Users\chihirosub\AppData\Local\.xmake\packages\s\sqlite3\3.54.0+100\b897e7007f3d49769c227e337e14fdef'; cmd /c scripts\verify_core.cmd
BedrockInsight core tests: PASS
BedrockInsightCoreTests exit=0
```

This run compiled and linked the v4 storage/query/API administration changes
with `-Wall -Wextra -Wpedantic` and passed all persistence, migration, sink,
and readback tests without warnings.

The current M5 localhost-listener verification used this same installed
toolchain command after adding `http_server.cpp` and the loopback integration
test:

```text
$env:BEDROCKINSIGHT_CLANG_ROOT='C:\PraxisRemote\toolchain\llvm-mingw-20260519-ucrt-x86_64\bin'; $env:BEDROCKINSIGHT_SQLITE_ROOT='C:\Users\chihirosub\AppData\Local\.xmake\packages\s\sqlite3\3.54.0+100\b897e7007f3d49769c227e337e14fdef'; cmd /c scripts\verify_core.cmd
BedrockInsight core tests: PASS
BedrockInsightCoreTests exit=0
```

This run compiled and linked the Windows socket listener with
`-Wall -Wextra -Wpedantic`, linked Winsock locally, and passed real loopback
request/response, authentication, bind-policy, and shutdown tests. It did not
connect to or modify any BDS process.

The current M5 dashboard/provider verification used the same command after
adding `dashboard.cpp`, the three embedded assets, and the provider routes.
The first run reached the executable but failed only on an outdated fixture
count (`API action log count`) after the new route tests were added. After
correcting that test expectation, the rerun completed as follows:

```text
$env:BEDROCKINSIGHT_CLANG_ROOT='C:\PraxisRemote\toolchain\llvm-mingw-20260519-ucrt-x86_64\bin'; $env:BEDROCKINSIGHT_SQLITE_ROOT='C:\Users\chihirosub\AppData\Local\.xmake\packages\s\sqlite3\3.54.0+100\b897e7007f3d49769c227e337e14fdef'; cmd /c scripts\verify_core.cmd
BedrockInsight core tests: PASS
BedrockInsightCoreTests exit=0
```

The final run compiled the dashboard/provider changes, including the admin
actions panel, with `-Wall -Wextra -Wpedantic` and passed route, role,
static-asset, and full core regression tests without compiler warnings.

The packaging-shell probe was also attempted with the locally audited SDK:

```text
set BEDROCKINSIGHT_CLANG_ROOT=C:\PraxisRemote\toolchain\llvm-mingw-20260519-ucrt-x86_64\bin&&set BEDROCKINSIGHT_LEVILAMINA_ROOT=C:\Dev\LeviLamina-26.60-Assistant-20261029&&set BEDROCKINSIGHT_XMAKE_PACKAGES=C:\Users\chihirosub\AppData\Local\.xmake\packages&&scripts\verify_levilamina_entry.cmd
```

It did not produce a passing probe. With installed package include roots, the
reference failed in `src/ll/api/base/Alias.h:98` (`expected a qualified name
after 'typename'` for `TypedStorageImpl`) and then reported missing
`concurrentqueue.h` from `_HeaderOutputPredefine.h:137`. This is a compile
environment/package blocker, not a live-server test; no DLL was loaded or
deployed.

The matched-source native build then used the MSVC-compatible LLVM 23.1.4
toolchain and completed successfully:

```text
set BEDROCKINSIGHT_CLANGCL_ROOT=C:\Dev\LL2660Toolchain-20261029\clang+llvm-23.1.4-x86_64-pc-windows-msvc\bin&&set BEDROCKINSIGHT_LL_ROOT=C:\Dev\LeviLamina-26.60-Assistant-20261029&&set BEDROCKINSIGHT_LL_LIB=C:\Dev\LeviLamina-26.60-Assistant-20261029\build\windows\x64\release\LeviLamina.lib&&set BEDROCKINSIGHT_SQLITE_ROOT=C:\Users\chihirosub\AppData\Local\.xmake\packages\s\sqlite3\3.54.0+100\b897e7007f3d49769c227e337e14fdef&&set BEDROCKINSIGHT_XMAKE_PACKAGES=C:\Dev\LL2660BuildCache\.xmake\packages&&scripts\build_levilamina.cmd
BedrockInsight native DLL build: PASS
SHA256 DLL: 460499e1f7e593d95f9b3a2909094870fffc31c4d59e1ccbda2f01b1312f1ecb
SHA256 ZIP: 60928c99fa5f7ea5ab827321e87f29a59ec816524f870d3e21ff4d2e09050ae0
```

`tar -tf dist\BedrockInsight-server-win-x64.zip` listed `tooth.json`, the
`BedrockInsight.dll`, and its PDB. `llvm-readobj --coff-exports` reported the
single required `ll_mod_load` export. The regression command was also rerun
with the installed MSVC-compatible clang and exited 0:

```text
set BEDROCKINSIGHT_CLANG_ROOT=C:\Dev\LL2660Toolchain-20261029\clang+llvm-23.1.4-x86_64-pc-windows-msvc\bin&&set BEDROCKINSIGHT_SQLITE_ROOT=C:\Users\chihirosub\AppData\Local\.xmake\packages\s\sqlite3\3.54.0+100\b897e7007f3d49769c227e337e14fdef&&scripts\verify_core.cmd
BedrockInsight core tests: PASS
BedrockInsightCoreTests exit=0
```

This MSVC-compatible run emitted only existing SQLite `__int64` and CRT
`gmtime` deprecation warnings; it did not report errors. A narrow read-only
search, `dir /s /b C:\Dev\*.exe | findstr /i bedrock`, returned no BDS
executable, so isolated load proof could not be run. No BDS process, live
server, other project, or production installation was started or modified.

The test links the locally cached SQLite 3.54.0 static library and uses a
test-only MinGW shim for MSVC CRT security symbols. It creates and removes a
temporary database; it does not start, stop, attach to, or deploy to BDS.

Also attempted:

- `xmake f -c -p windows -a x64 -m release -y`: the global xmake/package
  configuration was not usable in this sandbox (default cache permission
  errors; repository-local package configuration then waited indefinitely on
  package resolution and was interrupted). The core was therefore verified
  with the explicit installed package paths above.
- `cmake -S . -B build-cmake -G "Visual Studio 17 2022" -A x64`: the
  generator was present, but the advertised Visual Studio compiler path was
  not accessible to CMake in this environment. This did not affect the
  passing clang/static-SQLite verification.

Local Git commits are blocked: `.git` is exposed read-only by the execution
environment. The M5 HTTP targeted command
`git add README.md STATUS.md docs/SDK_AUDIT.md CMakeLists.txt xmake.lua
scripts/verify_core.cmd src tests/core_tests.cpp .gitignore` (including the
new `http_transport.cpp`, `json.cpp`, `api_payload.cpp`, `api_json.cpp`, and headers under
`src`) could not create
`.git/index.lock` (`Permission denied`). No commit or push is claimed. Earlier
M4 commit attempts and the follow-up `git commit -m
feat-m5-api-security-core` plus the current `git commit -m
feat-m5-http-transport-core` and `git commit -m
feat-m5-json-payload-core` and `git commit -m
feat-m5-dashboard-json-serializers` are blocked by the same read-only index
path; no commit or push was made. The current durable-admin attempt also
covered the schema-v4/storage/query changes and failed at
`git add README.md STATUS.md docs/SDK_AUDIT.md CMakeLists.txt xmake.lua
scripts/verify_core.cmd src tests/core_tests.cpp .gitignore` and
`git commit -m feat-m5-durable-admin-core` with the same
`.git/index.lock` (`Permission denied`) error. The current loopback-listener
attempt also covered `http_server.cpp`, the build/link updates, and the socket
integration test. The exact commands
`git add README.md STATUS.md CMakeLists.txt xmake.lua scripts/verify_core.cmd
src/bedrock_insight/core/http_server.cpp tests/core_tests.cpp` and
`git commit -m feat-m5-loopback-http-listener` both failed to create
`.git/index.lock` (`Permission denied`); no commit or push was made.
The current dashboard/provider attempt likewise ran
`git add README.md STATUS.md CMakeLists.txt xmake.lua scripts/verify_core.cmd
src/bedrock_insight/core/admin_api.cpp
src/bedrock_insight/core/admin_api.hpp
src/bedrock_insight/core/dashboard.cpp
src/bedrock_insight/core/dashboard.hpp
src/bedrock_insight/core/http_server.cpp
src/bedrock_insight/core/http_server.hpp tests/core_tests.cpp` and
`git commit -m feat-m5-dashboard-provider`; both were blocked by the same
`.git/index.lock` (`Permission denied`) error. A final retry after the admin
actions dashboard panel used the same add and commit commands and failed with
the same error; no commit or push was made.

The current packaging-shell attempt ran
`git add README.md STATUS.md docs/SDK_AUDIT.md xmake.lua
scripts/verify_levilamina_entry.cmd src/bedrock_insight/levilamina/mod.cpp`
and `git commit -m feat-m5-levilamina-packaging-shell`; both failed to create
`.git/index.lock` (`Permission denied`). No commit or push was made.
No further Git write was attempted for the native adapter/package checkpoint;
the same read-only index blocker remains and the project files are preserved
in the workspace for the supervisor to commit when repository write access is
available.

## Not implemented / next

- M2 remaining: verify post-operation block completion before restoring native
  break/place counters, add retention cohorts and sample CPU/memory when
  measurable. Current native callbacks cover sessions and block attempts.
- M3 remaining: add typed causation capture/normalization for containers,
  fluids, pistons, explosions, and entities. Those event/state APIs are not
  yet pinned to this matched server package.
- M4 remaining: add a verified LeviLamina world-state adapter. Container
  inventory, fluid, piston, explosion, and entity restoration remain blocked
  until their exact generated `mc/*` state APIs are pinned.
- M5 remaining: obtain matched server LeviLamina 26.60, runtime data server.12
  and BDS 1.26.60; prove isolation and run the package load/log acceptance
  test. UUID now comes from LL accessors; verified world identity and actual
  BDS event capture are still pending. The explicit clang-cl path is verified
  only as an unverified compile/link probe. No SDK mismatch can be bypassed
  to emit a release package.

The project remains `IN_PROGRESS`; it is not a complete analytics/audit mod.
