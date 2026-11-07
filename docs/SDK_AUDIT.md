# M0 SDK/build audit

Audit date: 2026-11-05

## Pre-BDS review correction

Official release follow-up: v26.60.0 resolves to commit
`072b9ffe0e9d9e0ec846f11d294880e892fe7770`. The release now establishes BDS
1.26.60.6, bedrockdata v26.60.6-server.5, prelink v0.8.8, PreLoader 1.16.7 and
mods/ placement. The old checkout's server.12 pin and plugins/ placement are
not the acceptance target. See packaging/bds-profile.json for official URLs,
blob IDs/archive digests and STATUS.md for acquisition evidence. The official
NativeModManager also requires unified allocation overrides/export; mod.cpp
now includes the SDK implementation and native build inspection requires both
that export and ll_mod_load. Official EventBus/Mod.cpp disable processing
removes listeners, so registration now occurs on each enable with stale
activation snapshots rejected. These fixes are locally tested, not BDS loaded.

The prior matched-server conclusion was incorrect. Both inspected local
source build configurations select `target_type = "client"`; the chosen
source output's `bin/LeviLamina/manifest.json` says `26.51.9+8b4ef79d`,
despite a source Tooth declaration of 26.60.0. The cached server package is
26.51.5 with server.6 data. The source declaration requires BDS 1.26.60 and
server.12 data, which were not found. No Bedrock server executable was found
under C:\Dev. The normal build now rejects these inputs; the actual DLL
compile/link result is an explicitly unverified probe, not a BDS package.

The installed levibuildscript uses prelink.exe to generate bedrock_runtime_api
from runtime data, so the former missing-bedrock_server.lib explanation was
not the exact blocker. The loader/runtime-data/server version mismatch is.
See SECURITY_REVIEW.md for source audit and tests. Historical notes below
do not establish binary compatibility or authorize a runtime launch.

## Local references inspected

Current additional input: project-local
`.build/pinned-sdk-headers-26.60.0/source` contains all 22,196 public common/
server headers at `072b9ffe0e9d9e0ec846f11d294880e892fe7770`. All 22,202
catalog files, including licenses/EULAs/source metadata, passed Git-blob
verification. Reused 21,877 exact local matches and fetched 325 missing files
at the pinned commit. Actual mod.cpp/runtime.cpp compile to COFF-x86-64
objects with source FMT/EnTT definitions and LL_PLAT_S. Exact arguments and
dependency records are in the object compile evidence JSON.

This is not a full source tree/SDK package: no matching server library/loader
or runtime data is included. Its xmake.lua is provenance metadata, not a
runnable loader build. Staging performs no Git fetch, cache mutation or Lua
execution. Binary downloads still fail through the proxy; standard package
validation continues to reject incomplete/mismatched inputs. See STATUS.md
for source/object hashes and remaining acceptance requirements.

| Reference | Observation | Use in this project |
| --- | --- | --- |
| `C:\Dev\BedrockDynamicSDK-2660-reference` | Versioned 1.26.60.5 Bedrock headers, generated profile data, header-only event/game facades, nlohmann JSON and MinHook vendor | Native Bedrock access only after an adapter has a verified symbol/profile contract |
| `C:\Dev\LeviLamina-26.60-Assistant-20261029` | Event/mod/generated headers and client build outputs declaring 26.51.9; source Tooth declares 26.60.0 | Compile/link reference only; source commit `8b4ef79d110f19952696d9ba6a0f5053e8e0689a`; not matched server provenance |
| `C:\Dev\PraxisSmall-2660-Port\companion\xmake.lua` | Working local pattern for `levilamina`, `levibuildscript`, clang-cl and native mod targets | Build-system reference only; no files are modified there |
| `C:\PraxisRemote\toolchain\llvm-mingw-20260519-ucrt-x86_64` | Local clang/clang++ toolchain usable without touching another project | Core test build toolchain |
| `C:\Users\chihirosub\AppData\Local\.xmake\packages\s\sqlite3\3.54.0+100\...` | Cached SQLite 3.54.0 headers, static library and CLI | M1 persistence provider; consumed read-only |

## Event/API findings

The local LeviLamina 26.60 headers expose these relevant event types:

- `ll::event::player::PlayerJoinEvent`
- `ll::event::player::PlayerDisconnectEvent`
- `ll::event::player::PlayerDestroyBlockEvent`
- `ll::event::player::PlayerPlacingBlockEvent`
- `ll::event::player::PlayerPlacedBlockEvent`
- `ll::event::player::PlayerUseItemEvent`
- `ll::event::player::PlayerDieEvent`
- `ll::event::world::BlockChangedEvent`
- `ll::event::world::ServerLevelTickEvent`
- `ll::event::server::ServerStartedEvent` and `ServerStoppingEvent`

`ll::event::EventBus` and `ll::event::Listener<T>` provide typed listener
registration. `ll::mod::LL_REGISTER_MOD` is the native entry macro. The
checked-out source tree itself provides the generated server `mc/*` headers
used for the compile/link checkpoint, while its installed xmake package
variants remain mismatched (the cached package is 26.51.5). The explicit
clang-cl probe therefore consumes reference source headers, the source-tree
client `LeviLamina.lib`, and cached dependency include/library roots. No
runtime hook or BDS process was started during this audit.

## Feature matrix at the current adapter-neutral core checkpoint

| Area | M0 result | M1 result | Status |
| --- | --- | --- | --- |
| Build/toolchain audit | Local references recorded | xmake core targets defined | Verified locally when build command passes |
| Immutable event model | Planned | Implemented | Core-tested |
| Tick-safe event queue | Planned | Bounded FIFO with close/back-pressure | Core-tested |
| SQLite persistence | Planned | WAL, foreign keys, schema v1, migrations | Core-tested against cached SQLite 3.54.0 |
| Single writer | Planned | Dedicated worker owns SQLite connection | Core-tested |
| LeviLamina event adapters | Identified event types | Join/disconnect/place/destroy wired to async storage | Remaining event families and isolated BDS load |
| Player statistics | Matrix only | Daily aggregates, durable per-type counters, and read-only rankings | M2 adapter wiring and retention |
| Server metrics | Matrix only | Durable MSPT/TPS/online samples and read-only summaries | M2 tick adapter and CPU/memory sampling |
| DAU/MAU | Matrix only | Read-only daily/monthly active-player queries | M2 adapter coverage and retention cohorts |
| Audit query/inspect | Matrix only | Filtered lookup/inspect core with bounded read-only queries | M3 adapter coverage and causation normalization |
| Rollback/restore | Matrix only | Preview/coalescing/conflict core plus durable single-writer journal | Bedrock world adapter and non-block restoration |
| Dashboard/API/auth | Matrix only | Loopback request boundary, injected token hashing, roles, fixed routes, bounded admin action log, bounded HTTP/1.1 transport parser, JSON limits, typed audit/rollback payload decoders, deterministic response serializers, SQLite-backed token/admin-log storage, socket listener, embedded dashboard UI/assets, native DLL/package target | Isolated BDS load and browser/server acceptance |

## Build limitation

The exact xmake package remains incomplete for a clean generic probe: its
cached server package is 26.51.5 and the reference probe reports the
generated-header/dependency issues recorded in `STATUS.md`. The matched source
tree plus its client import library is sufficient for an unverified native
compile/link probe. It is not sufficient for BDS loading. The current adapter
leaves `world_id` empty, uses LL-exported UUID/name accessors, and marks block
events as attempts because both audited emitters publish before `origin()`.
