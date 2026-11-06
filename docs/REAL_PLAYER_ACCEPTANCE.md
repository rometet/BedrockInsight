# One real player: isolated capture acceptance

The native DLL already loaded, enabled and stopped normally in the saved
`run-d64ec7bff59e4b6a97c043136f0a2c3b` test. Do not restart that world or repeat
the baseline. Its database has zero events; it is not player-capture proof.

## What the user must provide

Provide the Xbox gamertag and XUID of ONE consenting test account, and confirm
access to a signed-in Bedrock 26.60 client on this PC or its private LAN.
Gamertag/XUID are allowlist identifiers, not passwords. Never send passwords,
login tokens or account recovery information. A C++ UUID stored as
`uuid64:a:b` is NOT the Xbox XUID.

No client/account is available to the agent. Do not simulate a login, insert
SQLite rows, use an unauthenticated bot, or turn online-mode off to pass.

## Arrange the private test window before connecting

The DEFAULT smoke-test checker still requires an EMPTY allowlist and its
runner still stops after three seconds of readiness. A separate explicit
`-RealPlayer -PlayerName ... -PlayerXuid ...` mode is implemented for the
coordinated client test. It requires the SAME exact account at staging,
preparation, checking and launch; it does not accept arbitrary allowlist JSON.
The single generated entry includes an XUID and `ignoresPlayerLimit=false`.
Do NOT edit staged files, skip the checker or extend the timer ad hoc.
Before any execution, require the current focused Sol/xhigh review disposition
in STATUS.md and resolve HIGH/CRITICAL findings. The new mode's real-client
runtime behavior is NOT yet verified; metadata tests are not game evidence.

After the user confirms the consenting account and readiness, stage a NEW disposable
root under THIS project's `.build/isolated-bds/run-<32 hex>`, using the already
verified candidate and official inputs. Recheck fresh-world, exact files and
hashes, process ownership and free high ports. Keep `online-mode=true`,
`allow-list=true`, `max-players=1`, visitor permissions, cheats false,
LAN advertising false, HTTP/rollback disabled and NetherNet's explicit UDP
port tied to the validated signaling port. No production worlds or files.
Do not change firewall, router, proxy, service or public-exposure settings.
If the approved private route cannot reach the test, stop and report the
client error; do not weaken authentication or networking policy.

From THIS project, after replacing the account placeholders with the real
consenting account (not an example identity), the coordinator can prepare:

```powershell
$testAccount = @{
    RealPlayer = $true
    PlayerName = '<exact Xbox gamertag>'
    PlayerXuid = '<exact nonzero decimal Xbox XUID>'
}
$prepared = & ./scripts/stage_isolated_capture.ps1 -PassThru @testAccount `
    -CandidateRoot '.build/server-candidate/run-8041186e70984bd6b8ce7e73eed55e48'
& ./scripts/prepare_capture_executable.ps1 -SandboxRoot $prepared.sandbox_root @testAccount
& ./scripts/check_capture_isolation.ps1 -SandboxRoot $prepared.sandbox_root -RequirePatched @testAccount
```

Preparation revalidates the official archives/profile and existing candidate;
it does NOT rebuild the unchanged DLL or start BDS. PeEditor acts only on the
new scratch EXE copy. `BEDROCKINSIGHT_CLANGCL_ROOT` must already point to the
read-only tool directory containing `llvm-readobj.exe`. Do not install tools
into or change the SDK cache. Passing the checker is not a claim of runtime
acceptance or an OS sandbox.

Only when the client user is waiting, launch via the previously permitted host
execution path; do not clear or bypass restricted proxy/security settings:

```powershell
& ./scripts/run_isolated_capture.ps1 -SandboxRoot $prepared.sandbox_root @testAccount `
    -StartupTimeoutSeconds 90 -PlayerWindowSeconds 120
```

The runner prints `AUTHENTICATED ONE-PLAYER WINDOW OPEN` after BOTH ready and
enable. The 120-second window starts THEN, separately from the startup deadline.
It requests normal stop once one ordered name+XUID connect/disconnect pair is
observed; timeout, unexpected identity or duplicate/out-of-order observations
also stop the owned test, but FAIL acceptance. The default smoke mode cannot
be lengthened with `-PlayerWindowSeconds`. Maximum permitted player window is
300 seconds. Normal stop has 15 seconds grace, then termination of ONLY the
retained owned child as a failure fallback (never a clean-exit PASS).

## Exact client actions during the coordinated window

1. Wait for the coordinator to confirm BOTH `Server started` and
   `BedrockInsight enabled` and give the private address and validated port.
   The existing test profile uses port **59472**, NOT production/default 19132.
   Never connect to the active server for this test.
2. In the signed-in compatible Bedrock client, choose Play -> Servers ->
   Add Server. Use a distinctive name such as `BedrockInsight disposable`,
   the confirmed private address and the confirmed high port. Join ONCE with
   the allowlisted account. Do not share the address publicly.
3. Stand still for 5-10 seconds. Do not break/place blocks, run commands,
   interact with containers or invite another player. Save & Quit to leave.
4. Tell the coordinator the exact gamertag and that the client disconnected.
   Do not reconnect. The coordinator observes the disconnect and requests
   normal stop ONLY through its owned child handle/stdin. The existing blank
   first-line + `stop` workaround is already proven; do not kill other BDS.
5. After exit, require exit code 0, no forced termination, normal mod disable
   and `Quit correctly`. Preserve logs, runtime evidence and the closed DB.
   Do not checkpoint or delete SQLite sidecars to make acceptance look clean.

## Read-only acceptance after normal exit

Set `BEDROCKINSIGHT_SQLITE_ROOT` to the already installed SQLite package root
(the directory containing `bin/sqlite3.exe`; no new download). From this
project run, using the same prepared root and exact account:

```powershell
& ./scripts/verify_real_player_capture.ps1 `
    -SandboxRoot $prepared.sandbox_root `
    -PlayerName $testAccount.PlayerName -PlayerXuid $testAccount.PlayerXuid
```

This opens SQLite with `-readonly`, excludes writers while inspecting it,
checks unchanged database/runtime-evidence hashes, and writes a separate
`.build/real-player-evidence/run-.../real-player-evidence.json` report. It
requires native load/enable/ready and clean-stop evidence, `integrity_check=ok`,
schema v4, no FK violations, matching staged/preflight/runtime one-account
policy, bounded roundtrip stop reason, the real BDS connect/disconnect log
messages with the exact name AND XUID and expected LL26.60 server prefix,
exactly one matching `session_start`/`session_end`, one nonzero native UUID,
ordered event IDs/times inside the runtime window, and the corresponding
closed session with matching duration. At most 1,000 rows are inspected;
this is a fresh one-player test, not a production audit tool.

Nonzero exit / `event_to_db_verified=false` is NOT success. Database/log
consistency cannot independently attest that files were not fabricated;
an actual authenticated client connection is an essential prerequisite.
In-memory policy tests are labelled as such and never count as game proof.
Persisting the pair after normal disable establishes durability of that pair,
not complete event coverage, crash recovery or full project acceptance.
