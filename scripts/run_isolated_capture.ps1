# Bounded real smoke test. Only launches/stops the child created by THIS call.
param([Parameter(Mandatory)][string]$SandboxRoot,
      [ValidateRange(15,300)][int]$StartupTimeoutSeconds=75,
      [switch]$RealPlayer, [string]$PlayerName, [string]$PlayerXuid,
      [ValidateRange(30,3600)][int]$PlayerWindowSeconds=120,
      [ValidateRange(3,30)][int]$SmokeHoldSeconds=3)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'validate_capture_isolation.ps1')
. (Join-Path $PSScriptRoot 'capture_startup_outcome.ps1')
if (-not $RealPlayer -and $PSBoundParameters.ContainsKey('PlayerWindowSeconds')) { throw 'PlayerWindowSeconds requires explicit -RealPlayer mode' }
if ($RealPlayer -and $PSBoundParameters.ContainsKey('SmokeHoldSeconds')) { throw 'SmokeHoldSeconds is only for empty-allowlist smoke tests' }
$check=& (Join-Path $PSScriptRoot 'check_capture_isolation.ps1') -SandboxRoot $SandboxRoot -RequirePatched -PassThru `
    -RealPlayer:$RealPlayer -PlayerName $PlayerName -PlayerXuid $PlayerXuid
if (-not $check.candidate_policy_passed) { throw 'Capture isolation failed' }
$access=$check.capture_access
$root=$check.sandbox_root
$exe=Join-Path $root 'bedrock_server_mod.exe'
$exeHash=(Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash
$start=[Diagnostics.ProcessStartInfo]::new()
$start.FileName=$exe; $start.WorkingDirectory=$root
$start.UseShellExecute=$false; $start.CreateNoWindow=$true
$start.RedirectStandardInput=$true; $start.RedirectStandardOutput=$true; $start.RedirectStandardError=$true
# Windows PS5/.NET may emit a UTF-8 BOM on the first redirected stdin command.
# BDS interprets that BOM as command characters ('ﾃｯﾂｻﾂｿstop'); disable it explicitly.
# Use raw BOM-less UTF-8 bytes for the stop command below. PS5/.NET Framework
# does not expose ProcessStartInfo.StandardInputEncoding.
$start.StandardOutputEncoding=[Text.Encoding]::UTF8; $start.StandardErrorEncoding=[Text.Encoding]::UTF8
$temp=Join-Path $root 'test-tmp'
New-Item -ItemType Directory -Path $temp -Force | Out-Null
# Some PS5 hosts return null on the first lazy environment-dictionary access.
# Materialize/recheck it before indexing, and fail before Start if unavailable.
$childEnvironment=$start.EnvironmentVariables
if ($null -eq $childEnvironment) { $childEnvironment=$start.EnvironmentVariables }
if ($null -eq $childEnvironment) { throw 'Cannot initialize child environment; no server started' }
$childEnvironment['TEMP']=$temp; $childEnvironment['TMP']=$temp
$process=[Diagnostics.Process]::new(); $process.StartInfo=$start
$started=$false; $stopIssued=$false; $forced=$false; $failure=$null
$stdoutLineTask=$null; $stderrTask=$null; $pidOwned=$null; $exitCode=$null
$stdoutLines=[Collections.Generic.List[string]]::new()
$startTime=[DateTime]::UtcNow
$startupTimer=[Diagnostics.Stopwatch]::new()
$stopReason='runner_error'; $stopRequestedUtc=$null; $exitObservedUtc=$null
$observations=@(); $observed=@{}
$windowDecision=$null; $readySince=$null
try {
    if (-not $process.Start()) { throw 'Could not start owned capture test' }
    $started=$true; $pidOwned=$process.Id
    # Retain the native handle for this child. No PID-based discovery/kill,
    # process-name stopping, service control or interaction with other BDS.
    $null=$process.Handle
    $stdoutLineTask=$process.StandardOutput.ReadLineAsync()
    $stderrTask=$process.StandardError.ReadToEndAsync()
    $startupTimer.Start()
    $stopReason='startup_deadline'
    while ($true) {
        if ($process.WaitForExit(250)) { $stopReason='process_exited'; break }
        # Drain stdout incrementally: logs/latest.log may be truncated/buffered
        # while the server is alive, even when stdout contains ready/enable.
        $pumpLines=0
        # Return to the deadline/stop check even if output arrives continuously.
        while ($stdoutLineTask -and $stdoutLineTask.IsCompleted -and $pumpLines -lt 256) {
            $line=$stdoutLineTask.GetAwaiter().GetResult()
            if ($null -eq $line) { $stdoutLineTask=$null; break }
            $stdoutLines.Add($line)
            ++$pumpLines
            $stdoutLineTask=$process.StandardOutput.ReadLineAsync()
        }
        $log=Join-Path $root 'logs/latest.log'
        if ([IO.File]::Exists($log)) {
            $tail=($stdoutLines -join "`n") + "`n" + ((Get-Content -LiteralPath $log -Tail 150 -ErrorAction SilentlyContinue) -join "`n")
            foreach ($entry in @(
                @{name='native_loaded';pattern='BedrockInsight loaded: capture only; HTTP and rollback disabled'},
                @{name='native_enabled';pattern='BedrockInsight enabled: join/leave'},
                @{name='server_ready';pattern='Server started'},
                @{name='waiting_minecraft_services';pattern='Waiting for Minecraft services'},
                @{name='service_error';pattern='Could not connect to Minecraft services'}
            )) {
                if (-not $observed.ContainsKey($entry.name) -and $tail -match $entry.pattern) {
                    $observed[$entry.name]=$true
                    $observations+=[pscustomobject]@{state=$entry.name;observed_utc=[DateTime]::UtcNow.ToString('o');
                        elapsed_ms=$startupTimer.ElapsedMilliseconds}
                }
            }
        }
        if ($observed.ContainsKey('native_enabled') -and $observed.ContainsKey('server_ready')) {
            if ($null -eq $readySince) {
                $readySince=$startupTimer.ElapsedMilliseconds
                if ($RealPlayer) {
                    Write-Host "AUTHENTICATED ONE-PLAYER WINDOW OPEN: '$($access.player_name)', port $($check.checked_ports[0]), maximum $PlayerWindowSeconds seconds. Join once, stand still, disconnect."
                }
            }
            $windowDecision=Get-BdsCaptureWindowDecision $access ($startupTimer.ElapsedMilliseconds-$readySince) $PlayerWindowSeconds $stdoutLines.ToArray()
            if ($windowDecision.stop) {
                # Preserve the original default three-second policy. An explicit
                # bounded longer hold allows real 5s telemetry windows, never
                # changing allowlist/visitor/world/port checks or player mode.
                $elapsedReady=$startupTimer.ElapsedMilliseconds-$readySince
                if ($RealPlayer -or $windowDecision.stop_reason -cne 'ready_hold_completed' -or
                    $elapsedReady -ge $SmokeHoldSeconds*1000) {
                    $stopReason=$windowDecision.stop_reason; break
                }
            }
        } elseif ($startupTimer.Elapsed.TotalSeconds -ge $StartupTimeoutSeconds) { $stopReason='startup_deadline'; break }
    }
} catch { $failure=$_.Exception.Message; $stopReason='runner_error'
} finally {
    $startupTimer.Stop()
    if ($started) {
        if (-not $process.HasExited) {
            try {
                $stopRequestedUtc=[DateTime]::UtcNow.ToString('o')
                # BDS on this host prefixes its first parsed stdin command with extra chars
                # even though an independent redirected-pipe test receives raw ASCII cleanly.
                # Consume any first-line prefix with a blank line; stop is on the second line.
                $commandBytes=[Text.Encoding]::UTF8.GetBytes("`r`nstop`r`n")
                $process.StandardInput.BaseStream.Write($commandBytes,0,$commandBytes.Length)
                $process.StandardInput.BaseStream.Flush(); $stopIssued=$true
            }
            catch { if (-not $failure) { $failure=$_.Exception.Message } }
            if (-not $process.WaitForExit(15000)) {
                # Only our retained Process/handle, never an unrelated server.
                $process.Kill(); $forced=$true
                if (-not $process.WaitForExit(10000)) { throw 'Owned capture child did not exit after termination' }
            }
        }
        elseif ($stopReason -cne 'runner_error') { $stopReason='process_exited' }
        $exitCode=$process.ExitCode
        $exitObservedUtc=[DateTime]::UtcNow.ToString('o')
    }
}
$stdout=''; $stderr=''
try {
    # After child exit, drain any remaining output including the last line.
    while ($stdoutLineTask) {
        if (-not $stdoutLineTask.Wait(3000)) { throw 'Timed out draining owned BDS stdout' }
        $line=$stdoutLineTask.Result
        if ($null -eq $line) { $stdoutLineTask=$null; break }
        $stdoutLines.Add($line)
        $stdoutLineTask=$process.StandardOutput.ReadLineAsync()
    }
    $stdout=($stdoutLines -join [Environment]::NewLine)
    if ($stderrTask -and $stderrTask.Wait(3000)) { $stderr=$stderrTask.Result }
} finally { $process.Dispose() }
$stdout | Set-Content -LiteralPath (Join-Path $root 'capture-stdout.log') -Encoding UTF8
$stderr | Set-Content -LiteralPath (Join-Path $root 'capture-stderr.log') -Encoding UTF8
$logs=$stdout+"`n"+$stderr
$log=Join-Path $root 'logs/latest.log'
if ([IO.File]::Exists($log)) { $logs+="`n"+(Get-Content -Raw -LiteralPath $log) }
$loaded=$logs -match 'BedrockInsight loaded: capture only; HTTP and rollback disabled'
$enabled=$logs -match 'BedrockInsight enabled: join/leave'
$serverReady=$logs -match 'Server started'
$db=Join-Path $root 'mods/BedrockInsight/data/bedrock_insight.sqlite3'
$configRetained=$false
try {
    $migrated=Read-BdsJsonSnapshot (Join-Path $root 'mods/LeviLamina/config/Config.json')
    Assert-BdsCaptureLoaderConfig $migrated.data 42
    $configRetained=$true
} catch { if (-not $failure) { $failure=$_.Exception.Message } }
$outcome=Get-BdsCaptureStartupOutcome $stopReason ($logs -match 'Could not connect to Minecraft services') $stopIssued
$expectedStopReason='ready_hold_completed'
if ($RealPlayer) { $expectedStopReason='player_roundtrip_observed' }
$passed=($loaded -and $enabled -and $serverReady -and $configRetained -and $exitCode -eq 0 -and $stopIssued -and
    $stopReason -ceq $expectedStopReason -and -not $forced -and -not $failure)
[pscustomobject]@{started_utc=$startTime.ToString('o');finished_utc=[DateTime]::UtcNow.ToString('o');
    capture_access=$access;player_window_seconds=$(if ($RealPlayer) { $PlayerWindowSeconds } else { $null });
    ready_observed_elapsed_ms=$readySince;player_window_decision=$windowDecision;smoke_hold_seconds=$SmokeHoldSeconds;
    sandbox_root=$root;pid_created_by_test=$pidOwned;executable_sha256=$exeHash;
    candidate_dll_sha256=$check.candidate_dll_sha256;preflight=$check;
    startup_timeout_seconds=$StartupTimeoutSeconds;startup_observation_elapsed_ms=$startupTimer.ElapsedMilliseconds;
    startup_outcome=$outcome;startup_observations=$observations;
    stop_requested_utc=$stopRequestedUtc;process_exit_observed_utc=$exitObservedUtc;
    exit_code=$exitCode;stop_command_issued=$stopIssued;forced_termination=$forced;error=$failure;
    native_loaded=$loaded;native_enabled=$enabled;server_started=$serverReady;
    database_exists=[IO.File]::Exists($db);event_to_db_verified=$false;
    loader_config_migrated_with_disabled_overrides=$configRetained;
    bds_load_verified=$passed;normal_release_evidence=$false;matched_server_sdk_verified=$false;
    limits='Capture-only owned-child test. Access mode and player log observations are explicit, never synthetic game events. Real event-to-DB/durability require the separate read-only verifier; log observation alone is not DB proof. No complete SDK/feature acceptance.'
} | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $root 'capture-runtime-evidence.json') -Encoding UTF8
if (-not $passed) { throw "Isolated capture acceptance NOT passed. Inspect $root/capture-runtime-evidence.json and logs" }
"ISOLATED CAPTURE LOAD PASS; owned child exited normally. Evidence: $root/capture-runtime-evidence.json"
