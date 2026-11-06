# Third-party references

## Pier — tick processing measurement design

The two-priority Level::$tick measurement/counting design was studied from
Maskviva/Pier, commit `f841e43a251444b326557e4e225574d0f65663f3`,
`packages/pier-hooks/src/engine/TickStats.cpp`:
https://github.com/Maskviva/Pier/blob/f841e43a251444b326557e4e225574d0f65663f3/packages/pier-hooks/src/engine/TickStats.cpp

Pier is licensed under Apache-2.0; a copy is in `licenses/Pier-Apache-2.0.txt`.
Credit: Pier contributors / Maskviva (upstream history). No endorsement implied.

BedrockInsight's implementation is separately written: error-checked lifecycle,
owned/weak capture state, RAII exception/recursion/thread/Level guards, disjoint
constant-memory windows, end-aligned wall periods, immutable SQLite queue and
explicit persisted provenance. It does not link Pier or include its host API.
Original reference semantics are preserved: actual inner tick counts, elapsed
busy time around the outer origin, no idle-frame period masquerading as MSPT.

## LL26.60 SymbolProvider v1.3.0

The candidate links the cached SymbolProvider v1.3.0 static library, required by
the pinned official LL26.60 package. Source pin:
https://github.com/LiteLDev/SymbolProvider/blob/6c93ec45c8455992ee726d92df60316c8e731c44/src/SymbolProvider.cpp
Its upstream source header places this mingw-w64-derived implementation in the
public domain, without warranty, and credits BDSLiteLoader developers' changes.
The helper resolves the virtual bedrock_runtime.dll MCAPI imports through
LeviLamina's resolveSymbol. No real bedrock_runtime.dll is bundled or loaded.
The cache's recorded library digest is local build evidence, not an upstream
published binary checksum. No LeafKnife/Stats source is included.
