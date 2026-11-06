# Focused pre-BDS review — 2026-11-05

The review traced the native lifecycle, SDK/build provenance, event admission,
SQLite migrations/transactions/reopen/shutdown, HTTP/auth/browser boundary,
and rollback planning/application. Findings below come from implementation
inspection and failure-injection tests, not prior STATUS.md claims.
No BDS executable or DLL was loaded during this review.

## True processing MSPT focused delta — 2026-11-06

### Closed-run evidence gate and source-sidecar HIGH remediation (08:38 JST)

New post-stop verifier never controls BDS. It requires a normal owned isolated
run, matching candidate hashes, two valid same-activation busy windows/end marker,
closed-copy SQLite integrity/schema/FK and same-source live authenticated API /
Japanese DOM values. It rejects mixed/restarted/invalid/frozen/unsupported data.
It is consistency checking, not independent attestation against edited evidence.

Requested Sol/xhigh reviewer01a1ae0c-b1e5-7632-af2c-b42c9fdbb92d found HIGH:
PowerShell's unparenthesized array expression for DB+'-wal', DB+'-shm' checks one
concatenated path, not the two real paths. FIXED with a suffix loop shared by both
post-stop verifiers. Checks run under the read-only source lock before copying,
after copying and before PASS. Existing real-player verifier now queries its
private copy too, avoiding any reader-created files at the original database.
WAL-only/SHM-only/both markers independently reject; absent sidecars pass only
the precondition, never imply integrity or engine capture. Two wrapper order
checks pass. Final delta review confirms HIGH remediated/no residual severe;
model/effort identity remains unattested, no runtime safety certification.

Live verifier checks remain active under python -O, records real HTTP statuses
and emits success only after its own listener thread stops. No real viewer token
is written. Python40 tests include6 new pure/closed-copy CLI evidence fixtures;
all are explicitly NOT genuine native samples. Original used failed/protected
roots are rejected without DB copy. Native source/DLL unchanged, no new BDS run.

### Host runtime failure and resolver correction (08:20 JST)

The host's new C8F...trial9d84 reached enable/ready then exited C06D007E at the
first5s sample; SQLite has START markers only, integrityok. Unlike earlier
services failures, this is actual enabled-runtime failure evidence, not PASS.
Manual linkage had omitted official LL26.60's SymbolProvider v1.3.0 dependency,
using regular MSVC delayimp instead. A standalone negative control catches the
same C06D007E for the virtual runtime module; the real pinned cached helper
preloads the synthetic import once and two subsequent calls pass with no DLL.
No crash dump/stack was available, so engine-level cause remains an inference.

Corrected build links an owned prelink copy of pinned x64MD SymbolProvider,
`/NODEFAULTLIB:delayimp.lib`, exact LL resolveSymbol export. Cache digest is local
build provenance, not upstream binary authentication. Launch/staging fail closed
on missing/wrong metadata or absent resolver import. New provider/notice metadata
does not pretend complete SDK/release certification.

Requested Sol/xhigh reviewer01a1adfa-f717-7d12-baf0-6c86d946b356 checked the narrow
link/gate delta, all11 copy hashes and corrected DLL218A..., no HIGH/CRITICAL.
Model/effort identity is not independently attestable. It was read-only review;
no server/testing side effects. Native timer/capture C++ sources were unchanged.

Fresh corrected trialfef29 passed3395-file isolation then LOAD, but self-exited3
because Minecraft services failed before enable. No forced termination, no DB,
no proof of corrected runtime sampling. Normal host-route verification still
required. All used roots retained; old DLLB286.../BDS748/companion18767 unchanged.

Latest priority implements actual Level::$tick busy timing, not the earlier
pre-origin interval-only observer. `busy_tick.cpp/.hpp`, core/busy_tick.hpp and
Runtime lifecycle were statically reviewed through requested Sol/xhigh reviewer
`01a1ade4-55c3-75d3-9ce1-5fa7a66f7e19`. The reviewer explicitly does not attest
selectable model/effort identity; this is direct static review, not model identity
or runtime safety certification. No HIGH/CRITICAL reported. Earlier reviewer
`01a1adbb-1483-7a43-a203-3c77992e0afe` hit quota and supplied NO completed audit.

P2 found/fixed: releasing the sampler mutex before dispatch permits invalidation
to overtake an already prepared valid window. Capture now latches busy_invalidated
under its mutex and refuses later valid reports in that generation. Regression
valid->invalid->delayed valid proves one sample/one invalid record;154 accepted
records drain correctly. Final focused review reports the P2 resolved, no new
severe findings. Latest runtime/bridge SHA256s are recorded in build/evidence.

Highest/Lowest manual hooks wrap origin exactly once in our layer. RAII preserves
original exceptions, recursion/multi-Level/thread/clock errors invalidate data.
No ownership/sampler mutex across origin, bounded constant-memory windows, no
SQLite per tick. Error-checked install, DLL pinned before install, successful
hooks remain pass-through after logical disable. Shared in-flight state + weak
Capture/generation prevent use-after-free; no detach-time unhook. Partial install
does not activate a worker/capture. Test-only allocation failure additionally
proves noexcept enable rejects resource exhaustion rather than terminating BDS.

Reader only trusts exact busy provenance/method, typed bounded counts/times,
fresh wall timestamp and lifecycle sequence. No interval/legacy fallback behind
busy end/invalid/unsupported data. Freeze has measured0TPS and undefined MSPT;
warp60 is not clipped20. Mean outer busy scope and peak innermost scope are
explicitly distinguished. No p95 is invented. Existing viewer auth, localhost
binding, read-only SQLite and static web assets are preserved.

Native build/import/package PASS; full official new isolated preflight PASS3395
files. Actual new DLL LOAD confirmed, but the owned BDS self-exited3 due Minecraft
services before enable, no analytics DB or hook execution. No forced kill, auth/
network setting change, or protected BDS/dashboard update. Thus real detour ABI,
cadence, Mod interaction and genuine MSPT acceptance remain UNVERIFIED. See STATUS
top for candidate hashes, failed run732045..., tests and host-side handoff.

## Native interval telemetry Phase 2 — 2026-11-06

Explicit `gpt-6.1-sol` / `xhigh` static review requested for new sampler/runtime,
Python provenance reader and fixtures, reviewer `01a1ada6-f20a-7430-b556-a5757ffec78c`.
Request accepted; backend model identity is not independently attested. Reviewer
reported no NEW HIGH/CRITICAL in the reviewed delta, no edits or execution.
This is NOT runtime certification. It excludes proof of actual `_subTick`
cadence/threading, MCAPI behavior and delivery of an end marker on queue saturation.

Sampler uses `steady_clock`, one Level identity (never retained/dereferenced by
the sampler), O(1) counters and >=5-second immutable submissions, no game-thread
SQLite. Multiple Level identities permanently invalidate the activation. Existing
shared Capture/generation/lifetime protection remains. A clean disable enqueues
an end record before draining; abnormal loss relies on a 20s stale bound. Fresh
persisted data is still a last observation, not proof of process liveness.

No MSPT reuse: interval count/window and method/version live in explicitly named
v4 event provenance; tick_mspt remains zero and legacy server_tick_samples stays
untouched. Reader requires known kind/actor/cause/clock, bounded integer values,
nonfuture wall time and latest durable sequence. It never falls back behind a
new invalid/end record. Stale samples return no current TPS/online.

Initial native link succeeded but the pre-existing static gate rejected its new
bedrock_runtime.dll delay dependency. Added an EXPLICIT OPT-IN whitelist for the
single official `Level::$getActivePlayerCount` MCAPI generated by matched prelink,
not general support for arbitrary delay imports. Unknown module/symbol/ordinal,
duplicate/nested extras/bound-unload tables fail closed. Default parser behavior
and ordinary import-module checks remain strict. Parser tests: 1 valid/10 rejected,
old server release policy 23 rejected, native lifecycle fake-SDK tests pass.

Isolated launcher delta keeps original boundary checks and default ports/3-second
hold. Only explicit high distinct ports and a bounded 3..30s empty-allowlist hold
are added; mixed real-player/hold flags fail before launch. New parameter tests
6 rejected, existing access/boundary/window regression tests pass. Additional
Sol/xhigh delta review `01a1adab-6184-79e0-bab8-46adee550305` reported no NEW
HIGH/CRITICAL in import whitelist / port / hold changes; original physical gates
and owned-child stop semantics preserved. Review requested model identity is not
independently attested; no reviewer tests/launch/file edits. This review completed
before a new runtime test was launched.
Actual candidate hashes, final review result and any real runtime evidence are
recorded in STATUS.md, not inferred from these tests. Both protected servers and
the existing companion process must remain untouched.

## Analytics companion self-review and verification — 2026-11-06

NEW dashboard request implemented as `companion/analytics.py`, `server.py` and
static Japanese assets, not by enabling the dormant native admin/HTTP routes.
This continuation did not request a new model-specific external audit or execute
any BDS test. Do not attribute this companion assessment to earlier Sol reviews.

Source/code review plus regression tests verify: read-only SQLite URI/query_only,
in-memory backup with budgets, no source DML/journal/checkpoint/migration, existing
WAL/SHM requirement for BDS roots, localhost literal bind, exclusive Windows
socket ownership, fresh random viewer-only bearer with expiry and constant-time
digest comparison, no stored token/cookie/URL credential, exact Host/Origin,
no CORS and deny framing, fixed asset routes, no admin/mutation route, bounded
header total time/bytes and close-only serial requests. UI uses textContent and
DOM nodes, never interprets player names as HTML. No player UUID/XUID returned.

Data-integrity regressions cover JST half-open clipping, running-MAX interval
union for overlapping/duplicate sessions, exclusion of invalid/open/future rows,
deterministic positive-only SQL ranking, native attempt-vs-success separation,
and no TPS/online inference from stale/unverified samples or open sessions.
18 Python tests, DOM JS tests, syntax/compile checks and real-source authenticated
loopback HTTP passed. Live source returned Rome6882 136093ms, join=1/leave=1.
Evidence: `.build/analytics-dashboard-evidence/run-db86d40109eb411d99741dc0a25c9d73/`.

Limits: not an adversarial local-OS sandbox, remote HTTPS service, or independent
security certification. Standard SQLite WAL reader locks/shared-memory read marks
are possible; tests assert no fixture main-DB/WAL content writes, not absence of
all OS access metadata. Browser inventory is empty, so no actual browser rendering
or screenshot test is claimed. Tick/proc-MSPT native sampler and extra events are
not wired; older mspt fields never become fabricated TPS. Both active BDS processes
and their files/settings were left unchanged by authored code/operations. New
temporary companion listeners only were stopped, not BDS. No native DLL rebuild.

## Existing one-player timeout evidence inspected — 2026-11-06 04:58 JST

No native or script source change or new BDS execution in this continuation.
The ten reviewed/tested scripts still match the source hashes in
`.build/one-player-mode-tests/run-b987d128ad9a4bb9bac31ca0541db4a2/`.
Prior focused Sol/xhigh static disposition applies to those unchanged bytes;
no replacement review is claimed here.

Saved runtime evidence in `run-9331efac81024a2fb82e87e1bd57f7ab` shows the
one-account window actually ran 04:40:55–04:46:08 JST, reached native load,
enable and ready, then timed out at 300 seconds without a connect/disconnect
pair. Normal own-child stop, exit 0, no force; disabled loader overrides retained.
The FULL player acceptance flag correctly remains false. No observed connection
means this cannot diagnose a client authentication/network problem or certify
event capture. A post-exit read-only SQLite check found integrity OK, schema 4,
no FK rows, events 0 and sessions 0; DB SHA256 unchanged
`46f8656458379eef2e7d944f749c7616f6545d2968501fbd33c6228e12dcc04b`.
Evidence: `.build/one-player-timeout-inspection/run-1a4ce33955bc4e7ca4df354e3dd4c7a4/`.

Fresh preflight rejects the used stage on migrated Config.json; the existing
world also makes it ineligible for a fresh run. No reset, file-hash rewrite,
checkpoint, deletion or reuse was performed. A new disposable stage is required
only when the client is ready. Read-only inventory found zero BDS in that root
and one external BDS, which was not accessed/signalled beyond path metadata.

The computer-use skill was read with guidance, API and confirmation policy.
Supported sky app inventory found the already-running authorized client, but
target-window access returned `Computer Use was not approved to use windows`.
Stopped UI control; no fallback Win32/PowerShell input, alternate protocol,
authentication automation or security change. The earlier saved Character
Selection screen was not newly captured. Account and XUID are already known;
manual client navigation/readiness or approved tool window access is the
current prerequisite, not missing official binaries or a new SDK blocker.

## Explicit one-player test boundary — 2026-11-06 02:46 JST

Implemented the remaining one-account mode without changing native capture
sources or launching/staging BDS. Parent requested `gpt-6.1-sol` / `xhigh`,
reviewer `01a1accb-044a-70a2-a5dd-845dcdf28d8b` (Archimedes); requested
parameters accepted, backend designation not independently attested. Source
review of access policy, shared boundary, stager, checker, executable preparation,
runner, startup classifier and post-exit verifier found **no NEW HIGH/CRITICAL**.
Final follow-up also reviewed the 256-line stdout pump cap, user guide and seven
invalid-input entrypoint tests with the same disposition. No severe patch was
indicated. Reviewer ran no commands that build/stage/execute BDS, no tests,
network or Git writes. Static review does not certify runtime behavior.

Security/data-integrity boundaries:

- Explicit real-player opt-in and identical gamertag + string XUID are required
  at all preparation/launch stages. Exact generated single-entry JSON rejects
  other/missing accounts, duplicates, numeric XUID coercion or limit bypass.
  Staged file hashes still apply; no manual allowlist edit is permitted.
- Default smoke mode remains empty-allowlist / three-second hold. It rejects
  supplied player identities and a custom player window without explicit mode.
  Normal release provenance guard remains distinct and unchanged. The optional
  real-player physical boundary validates the actual nonempty list, not a false
  empty-list assertion, while keeping every other world/port/mod/property check.
- Online authentication true, visitor-only, one player, fresh owned world,
  high explicit NetherNet ports, empty permissions and no LAN advertising remain
  required. This is not OS/network isolation against a local adversary; no
  firewall/proxy/security/service change or public exposure is authorized.
- Startup and ready-player deadlines are separate and finite; output pumping
  returns to deadline checks after at most 256 lines. A single ordered name/XUID
  log pair stops normally; unexpected identity, duplicates, bad order or timeout
  stop with failed acceptance. Only the retained own child receives stop/kill;
  a forced kill can never pass. No gameplay command/event injection is used.
- The read-only verifier now requires name AND XUID plus matching recorded
  staging/preflight/runtime access and roundtrip/window evidence. Strict server
  log parsing excludes chat substrings. Existing transactional persisted-pair,
  closed-session duration, integrity/FK/schema and unchanged DB checks remain.
  A log-only observation or fixture cannot establish real event-to-DB capture.

Main-agent focused tests PASS on PS7 and PS5.1: 28 access + 8 default/physical
boundary + 7 INVALID real-entrypoint rejections, 14 log/window cases; default
boundary 32 metadata + 5 process-metadata + 2 junction rejections; build/config
12 + 3; startup classification 11 + invalid reason; session-row policy 20
negatives. Ten changed scripts parse on both. Evidence with source hashes and
host versions: `.build/one-player-mode-tests/run-b987d128ad9a4bb9bac31ca0541db4a2/`.
These are metadata/parameter tests, NOT actual BDS/client proof. Invalid-entry
tests reject before staging/native/process construction and leave paths absent.

Remaining runtime prerequisite: a consenting exact account and authenticated
compatible client/private route, not supplied yet. Use the updated
`REAL_PLAYER_ACCEPTANCE.md` flow only when the user is ready; no unchanged
baseline rerun. New mode has not admitted a real client or proved SQLite
drain under gameplay. Previous 01:47 clean-exit baseline remains valid with
zero event rows. HTTP and rollback stay disabled.

## Real-player acceptance follow-up — 2026-11-06 02:33 JST

The saved `run-d64ec7bff59e4b6a97c043136f0a2c3b` result supersedes the service/
enable/shutdown blockers in historical sections below: actual load, enable,
ready and normal stop succeeded, exit 0 without forced termination. The
database is intact but has ZERO events/sessions; no player joined. No new
runtime was launched during this follow-up.

Parent-tool review request explicitly selected `gpt-6.1-sol` and `xhigh`,
reviewer `01a1acbd-71aa-7600-98bc-142c495e8577` (Newton); request accepted,
backend model/effort not independently attested. The reviewer initially
mistook its own lack of delegation tools for a missing parent model request;
the parent clarified that no further delegation/model switch was needed.
Actual returned static disposition: no NEW HIGH/CRITICAL in
`runtime.cpp:231` join/disconnect and UUID/session handling,
`storage.cpp:183` transactional persistence, `storage.cpp:826` ID resumption,
or `storage.cpp:669` stop/drain. No severe native change was indicated.
This is a bounded static assessment, not a safety certification.

The same reviewer found no NEW HIGH/CRITICAL in
`scripts/real_player_capture_policy.ps1`,
`scripts/verify_real_player_capture.ps1` and
`docs/REAL_PLAYER_ACCEPTANCE.md` within their post-exit verification scope.
These do not launch BDS, change access controls, insert DB rows or simulate
game events. The checker rejects a live owned BDS or SQLite sidecars; holds
a read-only file handle excluding writers while executing fixed SQL through
the installed `sqlite3 -readonly`; verifies DB/runtime identity and unchanged
hashes; checks integrity/FKs/schema, ordered native session pair and matching
closed session; and requires BDS connect/disconnect and normal disable/quit
logs. Reports go to a separate owned `.build/real-player-evidence` directory.
No attacker-proof provenance or race-free OS isolation claim is made.

Main-agent tests: PS7 + PS5.1 two positive / 20 negative in-memory policy cases
PASS (including UUID UInt64 bounds, missing/open/duplicate sessions and
ordering/window failures). Read-only examination of the actual closed baseline
correctly returns player acceptance FALSE: integrity OK, schema 4, FK issues 0,
events 0, sessions 0, unchanged DB SHA256
`c9b28996d025e2b49d39e37848be4599a88b230a99af8953f79b1b50e747f5da`.
The PS5 process returns expected rejection code 2. No fixture is passed off
as a real capture, and the reviewer did not run these tests.

Remaining pre-runtime gate: the current empty allowlist / three-second smoke
runner cannot admit a human client. No real account identity/client has been
provided. A one-account, bounded private test mode must be separately
implemented/reviewed before an actual join/leave run. Do not silently weaken
the default checker or bypass staged-file hashes; keep online authentication,
visitor-only permissions, fresh disposable world and disabled HTTP/rollback.
The guide supplies user actions, but explicitly does not claim that mode exists.

## Official input / isolated runtime preparation review — 2026-11-05

This follow-up uses the six newly authenticated official archives and the
newly received matching BDS ZIP, not old missing-input assumptions. Independent
read-only review requested with gpt-6.1-sol/xhigh, agent
01a1ac0b-84ee-75e1-854b-88a6005bdf69. Requested parameters were accepted;
the backend designation is not independently attestable. No game execution
preceded the review. Final disposition on the six runtime-boundary/helper
files: **no remaining HIGH/CRITICAL finding**. Static review is not ABI or
runtime certification.

Actual execution follow-up (2026-11-06 JST): the first scratch run exposed a
functional native manifest blocker, not a severe code finding. The pinned
ModRegistrar excludes preload-native managers from the ordinary mod graph;
requiring LeviLamina there rejected BedrockInsight as a missing dependency.
Native dependencies are now empty and exact LL 26.60.0 metadata retained;
the Tooth installation dependency is unchanged. Runtime::load checks real
exported getLoaderVersion/isClient before capture allocation, DB or hooks,
rejecting client, mismatched numeric version, prerelease and exceptions.
Four adapter failure cases and the genuine official-header rebuild pass.
Independent Sol follow-up found no new HIGH/CRITICAL in this change.

The final DLL b286f054ef1ccf9d90cabbbf3943543fea2b6ff78805d2eb7f044ff89d6d8a00
ACTUALLY loaded through LL_REGISTER_MOD on BDS 1.26.60.6 + LL 26.60.0 in root
fb88d04ab9e3428aacb982705fd26f6d. Logs show both the native capture-only loaded
message and LL's "1 mod(s) loaded". Startup then waited for Minecraft services;
the owned test was stopped, exit 3 without forced termination. Enable and
SQLite capture were NOT reached. The runtime evidence explicitly records this
partial result rather than a full PASS. Config v42 retained every disabled
override after actual official migration. Post-test confirms no owned BDS or
bound test port remains. No online-auth/proxy/firewall policy was weakened.
Full allocator/event ABI, worker enable/disable/drain and player-event-to-DB
acceptance remain unverified. Exact evidence and remaining service blocker are
at the top of STATUS.md; original tests/failed manifests remain intact.

New HIGH found and fixed BEFORE any BDS test: initially
scripts/stage_isolated_capture.ps1 emitted a partial loader config labelled
current version 42. The pinned reflection decoder requires nonoptional fields;
missing fields can make ConfigImpl fall back to defaults, including statistics,
crash uploading and other-process checking. The stager now emits version 41
intentionally: the official v42 defaultConfigUpdater serializes complete
defaults, deep-merges the supplied disabled values, then deserializes the
complete document. ConfigImpl saves migrated v42 afterwards. Sol independently
confirmed this trace against the pinned Config API and ConfigImpl source.
The preflight requires integer v41 and exact false booleans, rejecting partial
v42 and string/nonboolean substitutions. Initial root b89fb2a2a21b4b4f8533fc59f6fa7ebc
was NOT launched; it is retained only as preparation evidence. Corrected test
root is ce8413c64e7b4c048d14a12736fb22c0.

Other MODERATE review findings fixed: exact-case archive IDs prevent omission
through case-variant duplicates; JSON snapshot helpers parse and hash identical
bytes then reject changed pins/profile/build evidence; failed prelink probes
now run original-input postchecks and retain final failure evidence; exit zero
without the expected nonempty import archive cannot pass. The observed stale
prelink usage text was resolved by actual execution on disposable real input
copies: the official target/outdir/data build-rule syntax succeeds. No guessed
argument order or fake MC implementation is accepted into the native build.

The normal release SDK gate remains unchanged before its now-shared physical
isolation boundary. A separate capture-candidate checker explicitly retains
normal_release_evidence=false and matched_server_sdk_verified=false, validates
the genuine server-derived build, all staged file hashes, exact two mods,
safe manifests/configs, absent worlds, empty permission/allow lists, high free
ports and process path ownership snapshots. The launcher creates one hidden
child from the checked absolute EXE and working directory, retains its handle,
and issues stop/timeout termination only to that child. It never discovers a
process by name/PID for termination or calls service control. PeEditor uses
only -m with explicit scratch --exe/-o, never -b/--inplace; the original EXE
and tool must remain unchanged. ZIPs are held read-only while hashed/extracted;
no installer hooks run. Localization is JSON-only with explicit observed-hash
provenance, not misrepresented as an upstream authenticated binary checksum.

Focused tests: official input policy 28 text-only rejections; capture build
policy 12 and loader config 3 rejections; existing boundary 30 metadata + 5
process + 2 real junction rejections. Real native build, prelink, preparation
and exact hashes are recorded in STATUS.md. These tests do not provide player
event capture proof. File/port/process checks remain snapshots rather than an
adversarial race-free OS sandbox. No production rollback or HTTP is enabled.

## Severe findings and disposition

| Severity | Location after fix | Evidence and fix |
| --- | --- | --- |
| High | src/bedrock_insight/levilamina/runtime.cpp:65,131,149 | Official server shutdown invokes disableAllMods, not unload. Previously load started a SQLite worker even for never-enabled mods, while disable left it running until unload/global destruction. Pending writes and worker/mutex cleanup could reach DLL process-detach under the loader lock. Load is now configuration-only; enable owns a fresh writer, disable drains/joins it, failed activation closes it, and unload stops it even if listener removal fails. Connection-release regressions and a 128-event shutdown burst pass (136 durable events overall). This is source-traced risk/adapter evidence, not a reproduced BDS exit deadlock or real shutdown acceptance. |
| High | src/bedrock_insight/levilamina/mod.cpp:3; scripts/build_levilamina.ps1:78 | Official 26.60.0 NativeModManager explicitly rejects DLLs without ll_memory_operator_overrided. The prior real probe exported only ll_mod_load; the mod lacked LL's unified allocation overrides despite exchanging C++ objects across DLL boundaries. Included the SDK MemoryOperators.h implementation in exactly one translation unit with its feature macro. The build now requires both exports; the rebuilt x64 probe also imports getDefaultAllocator/throwMemoryException. No marker-only workaround was used. Real loader/allocator runtime acceptance remains pending. |
| High | src/bedrock_insight/levilamina/runtime.cpp:89,178 | Official EventBus.cpp removes mod listeners after disable; the old enable path merely set a boolean, silently losing capture on re-enable. Registration now occurs on activation with cleanup/retry after partial failure. Callbacks retain an activation generation and reject stale snapshots after a new enable. The adapter harness simulates LL automatic removal, retained handles, failed re-enable and delayed snapshots; latest readback includes the shutdown burst above. Load itself installs no game hooks. |
| High | packaging/bds-profile.json; scripts/validate_bds_profile.ps1 | Official release metadata disproved the older checkout's server.12 runtime pin and plugins/ placement as the 26.60.0 acceptance target. The release requires BDS 1.26.60.6, server.5 data, PreLoader 1.16.7 and mods/ placement; the runtime-data recipe requires prelink 0.8.8. Corrected dependency pins/release guard; eight metadata rejection cases pass. Official archive digests/URLs/blob IDs are recorded, not treated as proof that bytes are installed. |
| High | scripts/resolve_sdk_inputs.ps1:1; scripts/build_levilamina.ps1 | Follow-up found the manual cache-wide include sweep selected parallel-hashmap v1.3.12 before the source SDK's recorded v2.0.0 and omitted exported EnTT layout definitions. The build now consumes exact xmake reference/dependency records and public definitions, validates selected package manifests, and refuses missing/ambiguous/shadowed inputs. A focused resolver suite covers both metadata forms and eight rejection cases. Release runtime-data selection now follows the official profile above, not the obsolete checkout's server.12 pin. |
| Critical | src/bedrock_insight/core/rollback.cpp:187; core/admin_api.cpp:321 | Previous apply loop could mutate location 1 and fail on location 2 with no compensation or durable recovery. Type names cannot restore properties/NBT. Application now always fails before invoking any world callback; even an admin with an injected provider receives 501. Preview and prepared journal inspection remain available. No opt-in switch is provided until full state, durable intent/recovery, backup validation and server-thread execution exist. |
| High | src/bedrock_insight/core/storage.cpp:669 | A worker that failed startup or exited on SQLite error set stopped_ before stop(); stop() returned without joining. Destruction of the joinable std::thread terminated the host. Lifecycle operations now serialize and always join an existing worker. Thread startup and worker exceptions enter the failure path. New submissions fail closed after startup failure, worker failure or stop. |
| High | src/bedrock_insight/core/storage.cpp:167; core/event_queue.hpp:33 | Every new queue started event IDs at 1. Reopening a populated database caused an events primary-key failure, stopping persistence. Startup now obtains durable MAX(event_id), initializes sequence before producers are admitted, and rejects exhausted IDs. Three reopen cycles with four producers each preserve 1,200 unique event rows and aggregate increments. |
| High | src/bedrock_insight/levilamina/runtime.cpp:21,145,173 | Handles were stored only after all four registrations succeeded; a later registration exception could leave live callbacks capturing raw this. Callbacks and lifecycle also accessed mutable state without synchronization. Every handle is now owned before registration; callbacks retain shared capture state and serialize admission against disable/unload. Failed removal retains handles and rejects logical unload. The inspected EventBus snapshots ListenerPtr and does not drain on removal; Windows pins this callback module until process exit to prevent execution of unmapped code. Hot DLL replacement is unsupported. |
| High | src/bedrock_insight/levilamina/runtime.cpp:39 | The previous adapter read generated Player storage fields and used mutable names as fallback identities, despite lacking a matched server build. It now uses LL-exported getUuid/getRealName, retains all 128 UUID bits as uuid64:a:b, and preserves Unknown when UUID is absent. Session callbacks do not dereference generated position components. No display-name fallback identity remains. |
| High | src/bedrock_insight/levilamina/runtime.cpp:214,225 | Both SDK block emitters publish before origin(), including PlayerPlacedBlockEvent. Previous code counted completed breaks/placements and implied reversible changes. Capture now writes one explicit attempt audit event with empty before/after columns; no completed-action counter or reversible snapshot is produced. Cancelled destroy/join notifications are skipped and duplicate joins suppressed. |
| High | scripts/build_levilamina.ps1; manifest.json; packaging/tooth.json | The previous DLL was linked to a tree configured target_type=client; its generated loader manifest says 26.51.9+8b4ef79d, although source tooth.json declares 26.60.0. It also omitted native manifest.json, used a nonstandard Tooth UUID, a nonexistent server label and client mods placement, and put an extra directory above ZIP placements. The native build now rejects client/26.51 provenance, supports an explicitly unverified compile probe only outside dist, pins xmake to server 26.60.0, includes native manifest.json, uses the SDK's server plugins placement, and archives placements from ZIP root. Historical artifacts are quarantined intact. Actual package-manager/BDS acceptance remains unverified. |
| High | src/bedrock_insight/core/admin_api.cpp:113,213 | Invalid ApiRole values previously satisfied >= Admin comparisons. Exact role validation now rejects unknown values before insertion and permission checks. Missing, empty or throwing authentication hashers fail closed. No HTTP listener, token store or production hasher is initialized by the native capture adapter. A cryptographic production hasher has not been implemented or certified. |
| High | src/bedrock_insight/core/http_server.cpp:135 | An exception from a provider/parser escaped a noexcept socket worker and could terminate the host. The connection boundary now catches exceptions and closes the client; a real loopback regression verifies a later request succeeds. HTTP remains outside the native capture build. |
| High | src/bedrock_insight/core/storage.cpp:690,753,799 | Flush predicates were changed without the condition-variable mutex, permitting lost wakeups. Commit/failure updates now synchronize with waiters. Control admission is bounded by queue capacity, increments pending only after successful enqueue, and rejects closed/failed workers. Callback rejection is retained in storage metrics; unload reports drops/failures instead of silently claiming healthy capture. |

## Data-integrity and security trace

- Each accepted event has one FIFO sequence; raw row, audit row, player/session
  updates and aggregates commit in one SQLite transaction. Migration steps and
  user_version updates are enclosed in one transaction. Future schemas fail
  closed. Worker failure rolls back the event and rejects further admission.
- Callback storage contains values only. The database worker owns the SQLite
  connection. Load is configuration-only; start/open/migration occur on enable
  and final drain/stop on disable or unload, not event callbacks. Failed
  activation also stops its writer. Partial registration is unwound. The
  writer must be joined before DLL detach; a forced exit/crash does not provide
  this lifecycle guarantee.
- Disabled native capture exposes no HTTP port, token-management endpoint or
  world writer. Rollback is blocked both at journal apply and API dispatch.
  Existing provider payloads cannot enable it. The prior claim that world
  access should happen on a control worker was removed; a future adapter must
  marshal access to the verified server thread.
- The dormant HTTP implementation binds IPv4 loopback and checks the numeric
  peer, bounds parsing and uses bearer authentication and role checks.
  The dashboard uses textContent for server data and does not persist the
  token in browser storage; constant innerHTML fragments contain no server
  data. No CORS permission is emitted. Production activation still requires
  secure hashing, constant-time comparison, durable security mutations,
  Host/Origin validation and browser acceptance; this review does not approve
  enabling it.
- WAL with synchronous=NORMAL is retained. Ordinary reopen and transaction
  failure are tested; abrupt process kill/power-loss durability is not
  proven. Open sessions after crashes/disable are not assigned invented end
  times. Pending queue values can be lost on process crash. This is not a
  crash-proof audit guarantee or a recovery journal for world mutation.

## Verification

Shared input roots for the commands below:

```text
BEDROCKINSIGHT_CLANGCL_ROOT=C:\Dev\LL2660Toolchain-20261029\clang+llvm-23.1.4-x86_64-pc-windows-msvc\bin
BEDROCKINSIGHT_CLANG_ROOT=(same LLVM bin)
BEDROCKINSIGHT_SQLITE_ROOT=C:\Users\chihirosub\AppData\Local\.xmake\packages\s\sqlite3\3.54.0+100\b897e7007f3d49769c227e337e14fdef
BEDROCKINSIGHT_LL_ROOT=C:\Dev\LeviLamina-26.60-Assistant-20261029
BEDROCKINSIGHT_LL_LIB=(LL_ROOT)\build\windows\x64\release\LeviLamina.lib
BEDROCKINSIGHT_XMAKE_PACKAGES=C:\Dev\LL2660BuildCache\.xmake\packages
```

- `scripts\verify_core.cmd`: PASS, exit 0. Reopen/concurrent producer,
  injected SQL failure, failed-start destructor, future schema rejection,
  rollback-write prohibition, invalid-role/unconfigured-hasher and
  provider-exception regressions join the existing core suite.
- `powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\verify_native_runtime.ps1`:
  PASS with /W4 /WX, exit 0. The harness compiles the actual runtime.cpp
  against a fake event surface solely to inject registration/removal failure,
  delayed snapshots, cancellation, duplicate joins, renamed/unknown players,
  disabled capture and attempt semantics, then reads real SQLite rows.
  This test does not verify Bedrock object layout, game hooks or BDS loading.
- `scripts\build_levilamina.cmd -CompileProbe`: actual x64 compile/link
  succeeds against the available reference import library. This deliberately
  emits only `.build/compile-probe/BedrockInsight.dll`, never a release
  package. `llvm-readobj --coff-exports` verifies the real `ll_mod_load`
  export. Compiler/argument/import-library hash evidence is written to
  `.build/compile-probe/build-evidence.json`; latest hash is in STATUS.md.
  SDK warnings remain; the fake-SDK test is warning-free.
- `scripts\build_levilamina.cmd` with the available source tree: correctly
  rejects mismatched SDK before compiling or publishing, exit 2.

## Independent requested-model source review — 2026-11-05

An independent read-only review was launched with explicit
`model=gpt-6.1-sol` and `reasoning_effort=xhigh`, agent
`01a1a962-d342-7602-967c-590090936075`. The agent tool accepted these parameters;
the reviewer cannot independently certify the backend model/effort. No BDS
test preceded this review. Result: **no NEW critical/high issue found**;
previously fixed findings above were not re-counted.

The review traced actual native entry/runtime, queue, storage, SQLite/schema,
rollback, admin/auth and HTTP containment paths, including:

- runtime.cpp:149: disable/unload stop storage outside the capture lock;
  callbacks share owned state and reject stale activation generations.
- storage.cpp:183,669: event writes are transactional; failure paths reject
  new admission and shutdown joins the existing worker. Durable IDs resume.
- rollback.cpp:187 and admin_api.cpp:321: apply is rejected before world
  callbacks. No native HTTP listener or production hasher is initialized.

Evidence is source and regression-source inspection only. No test/build was
rerun and no patch was warranted by this review. It does not establish matched
binary ABI, real SDK allocator/hooks, shutdown execution or event-to-DB
acceptance. Production hashing remains unverified. Keep HTTP and world mutation
disabled; matched inputs and proven isolation are still required before BDS.

## Read-only isolation candidate policy review — 2026-11-05

The new check_bds_isolation.ps1 is a read-only preflight, not a launcher or
complete isolation proof. validate_bds_isolation.ps1 rejects unsafe run paths,
reparse-point traversal, stale/probe/mismatched release metadata, a staged DLL
hash mismatch, existing worlds, additional mods, open access, reused/default
ports and running candidate BDS paths. It never authorizes a launch, including
when all candidate checks pass. Its network/process facts are snapshots only.

An independent reviewer was requested with explicit gpt-6.1-sol/xhigh, agent
01a1a979-51ce-7431-8783-8ca71ffdcfb0. Result: **no NEW high/critical finding**
in the three scripts after static review of containment, profile consistency,
side effects and PowerShell control flow. The requested parameters were accepted;
actual backend model/effort are not independently attested. No audit patch was
required. Main verified the policy suite under PS7 and Windows PS5.1: twenty
in-memory rejection cases, real descendant/ancestor junction rejections and a
positive candidate that still reports launch_authorized=false. CLI smoke checks
reject the project root, missing sandbox and missing normal release evidence
with exit 2. No fake EXE/DLL or release evidence was written; fixture writes
are confined to project-local ignored directories. No real BDS was run.

Hardlinks/ACLs, complete BDS/LL/runtime binary provenance, port reservation,
ownership races and actual server behavior are excluded, not certified. Any
future launcher needs separate validation for those concerns, fresh input/
process/port checks and explicit proof of isolation. The matched DLL/runtime
acceptance gate below remains blocked; this policy suite does not satisfy it.

## Remaining blocker for the BDS acceptance gate

### Genuine server-derived candidate build — 2026-11-05

New server_function_exports.ps1 classifies 960 function exports and excludes
the server's data marker ll_memory_operator_overrided. It rejects forwarded,
unmapped, unbacked, WX/non-code, overlapping-section and unsafe-name cases
before generating a restricted .def. llvm-dlltool emits named COFF import
metadata only (960 records, three descriptors, no implementation sections).
No old client library or fake SDK manifest enters build_server_candidate.ps1.
The actual native entry/runtime and five capture core sources freshly compile
against revalidated pinned common/server headers; post-link all 22 LL imports
match included code exports on the official DLL. This removes the client
import-library dependency for this native candidate, not the BDS/runtime gate.

The PS5 build exposed stderr/error-preference and local/global LASTEXITCODE
bugs. invoke_logged_native.ps1 keeps Continue function-local, uses fresh global
exit status and rejects null/nonzero/start failures. Focused tests pass on PS5
and PS7 for stderr+exit0, exit7 and missing executable. Function classification
passes 14 text-only rejections; existing release policy passes 23. Final native
build exits 0. Hashes, source/object/evidence snapshots and exact paths are in
STATUS.md and candidate-evidence.json. Failed intermediate runs produced no
success evidence. No DLL/BDS was loaded during any build/test.

Independent requested gpt-6.1-sol/xhigh review, agent
01a1abef-5e64-7511-ad71-fcd1a51b155f, found no NEW high/critical issue in
the latest helpers/guards after fixes. Parameters accepted, not independently
backend-attested; reviewer executed no scripts/binaries. Residuals include
metadata-only section classification (not instruction/physical-file-boundary
or object ABI proof) and non-security error-message wording. Official hash
identity, matching runtime inputs and runtime tests remain essential.

Normal release and isolation guards remain intact and reject actual candidate
evidence. The local manual ZIP is explicitly not runtime-verified, full SDK or
deployment approval. Pre/post C++ source/header and SQLite fingerprints, copied
native-object hashes and candidate-local header evidence preserve provenance.
No unrelated C++ functionality changed. Newly observed runtime tools have not
been executed; only runtime-data archive identity currently matches a profile
digest. BDS 1.26.60.6 and actual fresh isolation/load/shutdown/event capture are
still unverified. Production HTTP and world mutation remain disabled.

### New official server loader: static import resolution, not runtime acceptance

The coordinator supplied the official server ZIP pinned in bds-profile.json.
Main verified size/hash, internal 26.60.0 preload-native manifest and AMD64 DLL
metadata. Loader SHA256 is
211a20b6cd988d1b907fc300aca0bdf5387b35f1b5d0f0656d9c9e9270b45bac.
Strict parsing finds 961 named nonzero-RVA exports; all 22 imports of the
existing native diagnostic resolve. Preliminary manual 988 included module
names and is superseded. The existing diagnostic hash remains unchanged.

The client reference import archive is NOT a full server SDK: 126 of its
1,041 metadata names are absent from this official server DLL. Only the
actually used subset was checked. No release guard, historical evidence or
SDK provenance flag was relaxed. The new archive contains no headers/.lib.
The loader imports PreLoader.dll and delay-imports bedrock_runtime.dll, whose
availability/version and symbol resolution remain unverified. Static name
resolution does not prove C++ layout/ABI, allocator/hook behavior or shutdown.

New inspect_server_release.ps1 holds pinned archive bytes read-only while
hashing/reading ZIP entries, uses unique project-local paths and fixed bounded
DLL/manifest leaves, never arbitrary extraction. validate_server_release.ps1
keeps MOD imports strict named/no-delay; separate loader module inventory
handles legitimate Winsock ordinals/nested delay metadata without certifying
dependencies. Actual-run fixes also load both compression assemblies for PS5.
Main's final PS7/PS5 policy suites pass 23 text-only rejections and both actual
inspectors pass. Logs/paths/hashes are in STATUS.md. No DLL was loaded.

Independent static review requested with gpt-6.1-sol/xhigh, agent
01a1abde-ec83-73a2-aa8b-74131868e944: **no NEW high/critical finding** in
the latest three scripts. Requested parameters accepted; backend model/effort
not independently attested. Reviewer did not execute tests or inspect binaries.
No core/native source changed. matched_server_sdk_verified, packaged,
launch_authorized and bds_load_verified all remain false in new evidence.
Fresh loader-derived import metadata requires function-RVA/section/forwarder
checks before any link; a complete BDS bundle and separate isolation proof
are still required before runtime tests. Existing servers remain untouched.

### Process inventory follow-up (advisory checks only)

The earlier WMI/CIM query was denied (0x80041003), correctly blocking rather
than proving ownership. This was an availability obstacle, not an established
high-severity bypass. scripts/validate_bds_isolation.ps1:56 now rejects
unresolved/ambiguous executable paths; its read-only .NET process collector
gathers both standard BDS name queries before returning any result and disposes
only local handles. check_bds_isolation.ps1 uses this instead of CIM. It neither
launches nor signals/stops a server. Enumeration/MainModule errors reject the
candidate; cached metadata and subsequent process changes remain possible.

Independent static review requested with gpt-6.1-sol/xhigh, agent
01a1a995-d1c7-7781-a348-800283c77b78: **no NEW high/critical finding** in
the three changed isolation scripts. Parameters were accepted, not independently
backend-attested. Reviewer ran no inventory/tests/BDS actions and had no usable
Git HEAD for historical comparison. Main verified PS7 and PS5.1: 30 metadata,
five process-getter and two real project-local junction rejections pass; both
real read-only collectors resolve one external bedrock_server_mod. Missing
normal release evidence still rejects the CLI with exit 2. No EXE/DLL fixture
or server mutation was used. Native sources and diagnostic DLL are unchanged.

Coverage does not include collector-level late failure/disposal injection or
OS races. Name-limited enumeration and lexical path comparison do not prove
file identity/aliases, working-directory/world ownership, ACLs or full runtime
provenance. launch_authorized and bds_load_verified remain false even on a
positive candidate. No full isolation or BDS compatibility claim follows.

### Pinned-header full DLL diagnostic (not server compatibility)

The actual native entry/runtime and five capture core sources now compile/link
into a full x64 DLL using the official 26.60.0 headers, not an old generated
include root. It still uses a hash-pinned old client import library. Main
inspected its 1,041 named import records and three non-executable descriptor
records; that establishes import metadata shape, not current server provenance.
The policy rejects executable/directive sections, ordinal imports, foreign DLL
records and changed reference bytes. Eight text-fixture rejections pass on
PS7/PS5.1. No fake SDK library/header or dummy plugin was generated.

The new helper emits only a fresh .build/pinned-sdk-link-probe/run-* diagnostic,
warning file and evidence denying server match, packaging, launch and BDS load.
Normal release still rejects the reference SDK (exit 2), and the isolation
policy rejects the actual new diagnostic evidence. No release ZIP was emitted.
Latest diagnostic SHA256:
`32d25d9303a3884c1c8cc629e34f26c0ef60f2209af62873ae6b347ba14d1d02`.
Its exact path/logs/arguments are in STATUS.md; older probe hashes are historical.

Independent static review requested with explicit gpt-6.1-sol/xhigh, agent
01a1a985-dddb-7b91-87b3-3f0dc7445052, found no NEW high/critical issue in
these four script changes. Backend model/effort are not independently attested.
It did not inspect/execute the DLL or certify ABI. The DLL was never loaded.
Matched loader/runtime inputs, process ownership/isolation and actual BDS
initialization/shutdown/event-to-DB behavior remain unproven. A generic local
CIM process query was denied (0x80041003); no ownership conclusion was drawn.
The later .NET inventory above resolves the metadata-access obstacle only.

### Official header reconstruction checkpoint

The generated-header acquisition portion is now resolved for the pinned
26.60.0 source commit. `.build/pinned-sdk-headers-26.60.0` contains 22,196
official common/server public headers and six license/metadata files: 22,202
canonical Git blobs verified, none missing. Read-only staging reused 21,877
matching local files; the connector supplied 325 missing files at the pinned
commit. No stub or old-header fallback was used. The no-final-newline header
was restored only when its exact blob hash validated; a raw-byte check passed.
Ten path/profile/digest rejection cases pass without creating fake SDK headers.
Normal import requires the complete catalog SHA256 pinned in the BDS profile,
not just a self-reported release commit; a partial catalog fails before copies.

Actual entry/runtime sources compile to x64 objects against those include
roots and matching declared/transitive third-party header records. Hashes and
arguments are in .build/pinned-sdk-header-compile/compile-evidence.json;
mod.obj has both required export directives. This verifies the source API
surface only, not installed-package ABI, allocator execution, DLL linking,
game hooks or shutdown. No full SDK/loader was reconstructed. Normal release
packaging rejects the headers-only root with exit 2.

Matching server library/loader and BDS/runtime binaries remain unavailable or
unverified. Binary downloads still fail through the shell proxy, although
textual SDK inputs can be acquired through the connector. The old DLL probe
does not use these new objects and does not become loadable through this work.

### Binary/runtime acceptance still required

Current target is the official 26.60.0 release at
`072b9ffe0e9d9e0ec846f11d294880e892fe7770`, not the old local source declaration.
Its [Tooth metadata](https://github.com/LiteLDev/LeviLamina/blob/072b9ffe0e9d9e0ec846f11d294880e892fe7770/tooth.json)
and [versioned SDK recipe](https://github.com/LiteLDev/xmake-repo/blob/main/packages/l/levilamina/versions/26_60_0.lua)
require BDS 1.26.60.6 / server.5 runtime data / PreLoader 1.16.7 and mods/
placement. The runtime-data recipe selects prelink v0.8.8. Exact source blob
IDs, archive digests and release artifact identity are recorded in
packaging/bds-profile.json. No corresponding local SDK/runtime installation
has been verified.

The new loader/lifecycle findings were traced directly in official
[NativeModManager.cpp](https://github.com/LiteLDev/LeviLamina/blob/072b9ffe0e9d9e0ec846f11d294880e892fe7770/src/ll/core/mod/NativeModManager.cpp),
[MemoryOperators.h](https://github.com/LiteLDev/LeviLamina/blob/072b9ffe0e9d9e0ec846f11d294880e892fe7770/src/ll/api/memory/MemoryOperators.h),
[EventBus.cpp](https://github.com/LiteLDev/LeviLamina/blob/072b9ffe0e9d9e0ec846f11d294880e892fe7770/src/ll/api/event/EventBus.cpp)
and Mod.cpp. The official
[server shutdown hook](https://github.com/LiteLDev/LeviLamina/blob/072b9ffe0e9d9e0ec846f11d294880e892fe7770/src-server/ll/core/main_win.cpp)
disables mods, which is why storage cleanup now occurs on disable rather than
depending on unload. The loader-lock risk is an inference from those code
paths and [Microsoft's DLL guidance](https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-best-practices),
not an observed game-server crash. Block emitters still publish attempts before origin();
the capture-only semantics are unchanged. The focused native adapter suite
and SDK/profile suites passed after the fixes. The actual x64 probe now
exports ll_mod_load and ll_memory_operator_overrided and imports the LL
allocator functions. Historical reference-header probe SHA-256:
`916c6a81cef1a8140674e815c5cedc08818748b5130314a46003feecdc63fe62`.
Build log/arguments/profile evidence remain under .build/compile-probe.
No actual SDK allocator, loader, hook or shutdown execution was tested.

The connector retrieved the official server CI archive to a file reference,
but the local shell could not transfer it or download official release ZIPs:
curl exit 7 through the unreachable configured proxy. Browser inventory is
empty. The bounded acquisition helper neither extracts nor launches anything.
No network/security settings, external SDK/cache or Git index was modified.

Historical dependency-selection evidence (the hash below is superseded):

The dependency-selection follow-up resolves 32 exact source-reference package
roots and 29 include paths, using parallel-hashmap v2.0.0 and public
FMT_USE_FULL_CACHE_DRAGONBOX=1, ENTT_PACKED_PAGE=128 and ENTT_SPARSE_PAGE=4096.
The installed older server SDK separately resolves 20 pinned dependencies
and its exported ENTT_NO_MIXIN/LL_PLAT_S definitions. The parser reads a
restricted metadata subset without evaluating Lua; unsupported data and
attempted code in references are rejected. No other cached version is used
as fallback when an exact dependency is missing.

`& ./scripts/verify_sdk_inputs.ps1` passed. The actual native DLL rebuilt with
these inputs has SHA-256
`8af1351c5c532d52265acf953c40d9b01699b25e45452a2a2225dd1e881c3c24`;
its real mod entry remains a compile/link probe because the selected loader
and runtime-data variant are still not matched to the server requirement.
No runtime test was performed. The additional archive-cache scan found only
the older client.6/server.6 downloads; the bounded official release request
still fails through the configured proxy.

The only cached server LeviLamina package found is 26.51.5, with
v26.51.1-server.6 runtime data. The 26.60 source Tooth declaration requires
BDS 1.26.60 and runtime data 26.51.1-server.12; only client.12 is present
in the inspected Dev cache. Both local source build configurations inspected
are client builds. No bedrock_server*.exe was found under C:\Dev.

The old checkout's server.12 requirement above is historical and must not be
used for current release acceptance. The installed levibuildscript 0.6.1 link rule synthesizes bedrock_runtime_api
via prelink.exe and runtime data. Therefore a missing bedrock_server.lib was
not the precise SDK blocker claimed previously. The blocker is the absent
matching server loader/runtime-data/BDS combination. The current capture code
avoids direct MCAPI imports and does not require that invented import library.

Artifact acquisition was attempted using a bounded curl request to the
official LiteLDev GitHub release API. It failed with curl exit 7 because the
configured proxy at 127.0.0.1 was unreachable. No external SDK/cache was
modified and no proxy setting was changed.

No isolated runtime launch is approved by this evidence: there is neither a
matched server package nor an isolated BDS installation to launch. There is
no BDS log or real player-to-database proof yet. Once these inputs are
available, hash them, prove paths/world/ports/process ownership stay inside
this repository, and use only a fresh isolated world with capture enabled.
Keep HTTP and world mutation disabled for that first runtime acceptance test.

## 2026-11-06 player-drop result slice (focused implementation self-review)

MSPT's genuine host gate has passed. This review concerns only the new drop
boundary/lifecycle/provider/UI, not a repeat of MSPT or an independent Sol/xhigh
identity attestation. No new BDS was started by this review. Coordinator's95FE
empty smoke enables normally but has NOT exercised a player drop/accessor call.

Checked: exact pinned MCAPI signature and LL getTypeName accessor; no MCNAPI,
guessed offsets, NBT access or writes. Pre-origin snapshot bounds names/types and
uchar count; no item reference read after origin. Every control path calls origin
once, preserves its bool/exception, and excludes randomized/failed observations.
Tests cover snapshot/sink exceptions and mutation of the fake item after origin.
Nested calls are conservatively excluded. No mutex spans origin or the sink.
Shared in-flight state, pinned pass-through hooks, weak capture plus generation
and disabled admission prevent old callbacks from reaching a stopped/replaced
writer. Drop install failure stops the busy bridge and drains/closes storage.
Fake-SDK regression verified155 durable events and unchanged success aggregates.

Reader/API source is exact-version gated, validates integer1..255/type syntax,
rejects unmapped actors, aggregates positive observations only, preserves null
on missing coverage and uses only enum SQL metrics. Names/types are textContent,
raw IDs never exposed. Authentication, loopback binding, limits, read-only SQLite
and original static assets are unchanged; old18767 process is not hot-reloaded.
New actions DOM tests use real fixture HTTP401/200, not real gameplay/screenshot.

No HIGH/CRITICAL issue identified in this focused self-review; NOT a general
safety certification. Remaining risk is genuine engine result/quantity behavior
and interaction with other mods. Display says true-return entry-stack observation,
not inventory-delta proof. Require a fresh normal-host player test before claiming
runtime item success. Do not repeat empty smoke or weaken auth/network to test it.

## 2026-11-06 C1 dead-flag transition (focused self-review, runtime pending)

Reviewed the narrow Player::$die bridge, weak resolution, writer/admission and
new reader/UI. This is self-review, not independent Sol/xhigh model attestation.
Exact official MCAPI die signature and StackResultStorageEntity constructor
compiled; LL isDead/getEntityContext/getWeakRef/tryGetFromEntity exports resolve.
isDead is the pinned IsDeadFlagComponent accessor. No isAlive/absorption MCNAPI
or health/attacker offsets. The full candidate links SymbolProvider as before.

Old Player is not dereferenced after origin: a pre-copied official WeakEntityRef
re-resolves the generation/nonremoved actor and checks identity before isDead.
Failure/expired/replaced/already-dead/reentrant/disabled paths conservatively omit
data, not invent a death or killer. Origin exactly once, RAII recursion state
clears on engine exception, and observer failures cannot change the engine call.
No bridge mutex spans origin/sink. Active tokens, weak capture and generation
reject in-flight callbacks after disable/re-enable. New install failure closes
the previously installed bridges and drains the writer (fake SDK regression).

Reader exact-source/actor/type/amount and strict bounded JSON checks prevent
legacy/pre-notification/wrong bool/unknown version rows from inflating the new
confirmed_deaths metric. Damage codes use a fixed pinned range; unknown means
unknown, never guessed attribution. Only enum SQL identifiers, parameter-bound
selectors, positive-only ranks and textContent. Auth/loopback/read-only budgets
unchanged.50 Python tests plus32 C++ boundary combinations and writer156-event
drain PASS, including actual fixture HTTP -> Japanese DOM; not gameplay proof.

No HIGH/CRITICAL issue identified in this scoped self-review. Remaining risks:
actual die path must set the component when the same actor still resolves;
other mods may alter the observed scope; omitted transitions are possible;
new MCAPI weak resolution has not executed in BDS yet. Require NEW normal-host
real-player action/DB/UI/normal-stop verification before acceptance. Candidate
80BD is BUILD/LINK ONLY, not the older95FE enable smoke or218A MSPT acceptance.
No BDS launch, invasive instrumentation, security changes or live deployment
was performed by this review.

## 2026-11-06 delayed sprint/sneak flag observations (focused self-review)

Typed LL26.60 request events are pre-origin, not proof of success. This adapter
stores a bounded pending baseline and reads the same actor's real server flag
250ms..2s later via official LL getStatusFlag/inline wrappers. Cancellable sneak
requests already cancelled at this listener are ignored. No MCNAPI setters,
packet hook, guessed field offsets, health, distance, or duration inference.
WeakEntityRef defaults IncludeRemoved=false; identity and Level are checked.
Expired/removed/replaced/mismatched requests omit data. Short/opposite toggles
coalesce and duplicates do not extend TTL; the UI explicitly describes omissions.

Capture::mutex serializes the bounded256-entry tracker, synchronous sink and
disable/disconnect flush. The sink is never retained and reacquires no mutex;
there is no delayed task holding a raw Capture/Player. No engine pointers are
dereferenced on stop. At most4Hz flag inspection and five-second aggregate
submissions use the existing single writer, not SQLite per input/tick. JST
boundary flushes retain actual observation dates, not the later flush date.
Only already confirmed counters flush at disconnect/disable; pending input is
not promoted to success. Queue rejection/crash can lose observations and is
not retried into duplicates. Mode history is not an exact total or online mode.

Reader checks exact source/version, mapped player, enum type/edge, positive
bounded integer amount, strict bounded JSON booleans/cadence/time/day bucket.
SQL uses whitelisted metrics/parameters and positive-only deterministic ranks;
missing collection stays null. New/actions textContent renders hostile names
literally; original static files,18767 auth/process, read-only SQLite limits,
loopback/Host/Origin constraints remain unchanged.55 Python tests, fake-SDK
157-row drain and boundary tests pass; six official native objects link and
34 LL imports resolve. These are NOT engine execution proof.

The pinned craft reference was inspected factually, not copied. No observer
calls a get-or-init container mutator or assumes a transfer Success proves a
whole request commit. That feature remains unmeasured pending an independently
verified supported nonmutating completion path.

No HIGH/CRITICAL issue identified in this focused self-review, not independent
model-identity attestation or runtime certification. Candidate01FC has never
been staged/loaded in BDS. Actual event firing/flags/weak resolution, interplay
with other mods and live action->DB->API->Japanese UI require a NEW normal-host
real-player trial, not borrowed95FE smoke or218A MSPT evidence. No BDS launch,
security/auth change, host-route approval retry, deployment or push performed.

## 2026-11-06 pickup main-inventory delta (focused self-review, runtime pending)

Independent Player::take Lowest wrapper uses the pinned MCAPI signature, checked
LL ItemActor conversion/item getter, LL Player main-inventory getter and official
const Container/FillingContainer virtual declarations. No guessed slot numbers,
MCNAPI thunks, packet interception, init-container mutators, NBT/world/item writes.
At most128 slots per scan; total/count arithmetic bounded before subtraction.
Only true return AND positive net type-total gain<=entry stack units passes.
Changed container/size, invalid/removed/replaced player or ambiguous delta omits
data. Nested take invalidates outer as well as skipping inner, avoiding double
credit. This observes main-inventory net gain, not all pickup or durable transfer.

All item/player/inventory pre-call data is value copied or a comparison-only
identity. After origin only an official weak-reacquired same nonremoved Player
is read; no old ItemActor/ItemStack is dereferenced. Origin exactly once, errors
in snapshot/post/sink cannot change the engine bool/exception. Shared in-flight
token, pinned pass-through disable, weak Capture and activation generation reject
late/stale writes. No mutex spans origin, virtual inventory reads or the sink.
Install failure closes earlier bridges/listeners and SQLite; disable drains the
existing single writer. Synthetic lifecycle tests include old callbacks after
disable/re-enable and the pickup install-failure path.

Exact-source/mapped actor/type/positive-int and bounded JSON checks verify stored
before/after arithmetic, slots, entry units and explicit false uncertainty flags.
Whitelisted SQL metrics, parameter binding, positive ranks, null on no coverage,
type top100 and textContent remain; auth/loopback/read-only budgets untouched.
60 Python tests incl HTTP401/200->Japanese DOM and C++ delta/boundary tests PASS.
Static PE whitelist adds only the compiler-observed exact Player::take import;
wrong signature or missing weak constructor is rejected (5 valid/17 rejected).

Self-review found no HIGH/CRITICAL issue in this slice; not independent reviewer
identity/runtime certification. LL getInventory assumes initialized inventory;
the official virtual container ABI and weak Player reacquisition must execute
in a fresh normal-host player test. Other mods can change type totals in the
observed interval; NBT variations/equipment/cursor/other containers are excluded.
Do not claim a specific dropped entity was transferred or infer missing zeros.
No live BDS/viewer change, host-route approval retry or security bypass performed.

## 2026-11-06 ground-jump upward impulse (focused self-review, runtime pending)

Independent Mob::jumpFromGround Lowest bridge, not AuthInput/packet interception.
Checked LL Player conversion excludes nonplayers. Pre-state requires grounded,
alive/nonriding player; post-state weak-reacquires same nonremoved Player and
rejects dimension/mount/death changes. Direct official inline getPosDelta is
used, NOT LL getVelocity (post-tick/vehicle movement) or MCNAPI isJumping. Ground
test is official OnGroundFlagComponent accessor. No mutation/teleport/position
write or guessed offset/virtual slot. New exact MCAPI jump/getVehicle names were
observed in the actual official-header object, not assumed from source spelling.

Finite absolute Y<=16 only, quantized x1e6 with safe bounded rounding; positive
post-state greater than baseline required. Equal repeated calls/no-op do not
count, nested calls invalidate outer as well as omitting inner. Origin exactly
once; observer failures do not replace engine exceptions. No old Mob/Player is
dereferenced after origin. In-flight token, DLL pin, weak Capture/generation and
logical disable protect late callbacks. Install failure closes earlier bridges,
listeners and worker; fake-SDK lifecycle covers159-row drain and stale rejection.
No high-frequency input allocation/polling or SQLite in this hook.

This is an upward direct-state observation during one method, NOT flight/physical
jump completion, a key count, distance or proof of the method's causal ownership
under arbitrary mods. Ground flag may still be set at immediate return. Unfired
routes, invalid/large/tiny-rounded changes and other-mod interference can omit
observations. Such absence remains null; never infer measured zero or online.

Reader exact-source/type/mapped actor/integer amount=1 and bounded strict JSON
flags/measurement/quantized delta/dimension guards, whitelisted SQL/JST positive
rankings, selected scope and literal textContent preserved. Original18767 and
base static assets/auth/loopback/read-only bounds untouched.65 Python tests incl
actual fixture HTTP401/200->Japanese DOM and C++ state/exception tests pass.
Static runtime whitelist6 valid/21 rejected incl wrong jump/vehicle signature
and missing vehicle/weak dependencies; no arbitrary-symbol policy relaxation.

No HIGH/CRITICAL issue identified in this focused self-review, not independent
reviewer identity or engine certification. Actual player execution of this BDS
path, supported state-vector/weak API contracts and gameplay->SQLite->API/UI
remain unverified. Require fresh normal-host real-player actions and normal-stop
integrity, not synthetic proof/old smoke/MSPT evidence. No BDS launch/deployment,
security/auth setting change, host-route denial bypass or GitHub push performed.

## 2026-11-06 direct-attack dead transition attribution (focused self-review)

Single Mob::$die Lowest hook, not pre-origin MobDie notification counting.
Exact EntityAttack=2 has a compile-time pinned-enum assertion. Entity/nonchild/
nonblock source only; nonzero matching source/damaging IDs must resolve through
the victim's Level to the same checked nonremoved Player, not self, with UUID. No last
attacker, display-name identity, getOwner/projectile attribution or unknown credit.
Source const virtuals/Level lookup use exact official declarations, not MCNAPI
thunks/guessed slots. No health/absorption/damage arithmetic/offset guessing.

Pre-origin copies credited identity/name and victim type/class; actor/type mismatch
is excluded. After origin no DamageSource/attacker/old-victim dereference: official
weak ref must resolve same nonremoved Actor, identity check BEFORE dead flag read.
False->true only; prior-dead/replaced/removed/not-dead/nested paths omit. Nested
invalidates outer too; origin exactly once and observer exceptions preserve engine
exception. In-flight shared state, DLL pin, weak Capture/generation and logical
disable as prior adapters. Install failure closes earlier bridges and writer.
Fake lifecycle verifies161-row drain and old/disabled rejection, not engine ABI.

Append-only own event kind, no inflation of legacy full kills or victim deaths.
No victim raw IDs stored or exposed. Reader exact-source/type/class/mapped actor/
integer amount=1 and bounded strict JSON flags; positive SQL ranks, null on missing
opposite category/coverage, top100 types, literal textContent. Auth/loopback/
read-only budgets/original18767 assets unchanged.70 Python tests incl actual
fixture HTTP401/200->Japanese DOM,32 C++ transition combinations/source exclusions/
repeat/respawn/exception tests pass. Static gate7 valid/23 rejected incl wrong
Mob die owner and missing weak API; new import is compiler-observed exact Mob die.

No HIGH/CRITICAL identified in this scoped self-review, not independent reviewer
identity or engine certification. Direct source reporting is NOT all kills/true
causal attribution or durable death under arbitrary mods; Player deaths bypassing
Mob path and quickly removed victims can be missed. Source/Level virtual dispatch,
new MCAPI and weak dead-state must execute in NEW normal-host real-player trial.
Player-vs-player trial needs explicit second-participant and isolated-profile
authorization, not edits to the protected BDS or single-player test allowlist.
No BDS launch/deployment/security change/approval bypass/GitHub push performed.

## 使用true応答・主所持品照合の追加レビュー（自己レビュー、実機未検証）

`GameMode::$useItem`の正規MCAPI宣言と型付きPlayer参照のみ使用。
PlayerTransactionSubjectのunknown storageやPlayer::useItemInHandSlot(MCNAPI)は使用しない。
checked変換・UUID既知を入口で確認、後段は同じ未削除weak Player→同じInventoryを再取得。
借用GameMode/Player/ItemStackへの処理後アクセスなし。最大128枠を前後2走査、
種類別総個数のみでNBTやコンテナ書込なし。origin1回、bool/例外保持、入れ子除外、
世代/disable admissionを使用。hook内DB接続なし、既存bounded asynchronous writerのみ。

新規kind/厳密バージョンsource/amount=1/手/名前空間種類/整数差分/false検証フラグを
companion SQL UDFで再検証。不正・未来・未マップactorは除外。旧schema1〜4を変更しない。
正の減少なしは不明、true応答を完了/消費成功へ格上げしない。opaque Playerキーと
textContent、loopback/auth/Host/Origin/read-only制限は継続。18767と旧静的アセット非変更。
これは一般リリース認証・独立レビュアーの審査ではなく、GameMode typed reference、
Inventory virtual、weak再取得、hook実行の本物のABI/行動→DB→API/UIは通常ホスト実機確認待ち。

## 行動受け入れチェックの追加自己レビュー（実機未実行）

`companion.action_acceptance` CLIは`.build/real-player-evidence`の既存私有レポートと
同じ親内の閉じたcopyだけをSQLiteで開く。protected元worldを拒否し、path traversal・
symlink/junction・巨大JSON/DB・WAL/SHM/journalを拒否する。runtime/staging/stdoutと
候補DLLのハッシュを照合するが、元DB・BDSプロセス・認証・firewallは操作しない。
copyは256MiB/SQL2秒/1000events、メモは64KiB・対応15指標の正整数のみ。
observer noteをDBから自動生成しない。正常stop・一人用設定・生session・厳密source・
起動世代・開始/終了マーカーを照合。未測定を0や成功へ補完せず、指定外を合格にしない。

HTTPは新規ephemeral127.0.0.1のみ、既存18767を参照/再起動しない。独自トークンは
メモリ内、Nodeへは認証済みデータだけをstdinで渡し、実トークンは渡さない。
Nodeは固定JSをVM DOMで評価、textContentのみ許し、ネットワークfetchをstub化する。
HTTP本文最大2MiB、Node8秒、thread終了を確認。結果は個数・ソース・hash・日付とし、
氏名・UUID/XUIDやsubprocessの生診断は出さない。HTML/JSの取得／評価対象hashも照合。
実機自動attestationやブラウザー描画の証明ではなく、edited evidenceへの限界を常に明記。

停止後検証器の窓上限だけを、既存runnerの30〜3600秒へ整合した。runnerの承認範囲、
一人用allowlist、online-mode、強制停止禁止・既存world保護は変更しない。
`verify_craft_api_probe.ps1`は既存compile evidenceのオプションで自作probeを1 object化するだけ。
MCNAPIへ移行せず、候補のruntime import whitelistやSymbolProviderは変更しない。
これは自己レビューであり、独立レビュアーや実機のcapture成立を主張しない。

