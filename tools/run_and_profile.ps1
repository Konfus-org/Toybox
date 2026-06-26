# Run the example app under the Visual Studio command-line profiler (VSDiagnostics.exe)
# and collect a CPU-sampling trace. Companion to run_and_capture.ps1 — same launch
# convention, but instead of a screenshot it produces a .diagsession plus an expanded
# (raw) copy of the trace for inspection.
#
# Strategy: we launch the Launcher ourselves (redirected stdout/stderr, graceful WM_CLOSE
# shutdown), then ATTACH VSDiagnostics to that PID. This keeps full parity with
# run_and_capture's process handling while the profiler samples the live window.
#
# Output (under build/run_and_profile):
#   engine_<timestamp>.diagsession  - the raw VS trace (open in Visual Studio for call trees)
#   engine_<timestamp>/             - expandDiagSession output (ETL + metadata, CLI-readable)
#   run_stdout.log / run_stderr.log - the app's console output
param(
    [int]$Seconds = 8,
    # Seconds to let the app warm up (window + first frames) before sampling starts.
    [int]$WarmupSeconds = 2,
    [string]$Bin = (Join-Path $PSScriptRoot '..\..\ExampleProject\build\bin\Debug'),
    [string]$AppArgs = ('--app=ExampleProject --settings="' `
        + [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\ExampleProject\AppSettings.json')) `
        + '"'),
    # CPU sampling config that ships with VS. Swap for CpuUsageHigh.json (higher sample
    # rate) or CpuUsageWithCallCounts.json if you need denser data.
    [string]$Config = 'CpuUsageBase.json',
    # Auto-detected below if left empty.
    [string]$VsDiagnostics = '',
    # Expand the .diagsession into raw files after collection.
    [switch]$Expand = $true
)
$ErrorActionPreference = 'Stop'

# --- Locate VSDiagnostics.exe -------------------------------------------------
function Find-VsDiagnostics {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    $roots = @()
    if (Test-Path $vswhere) {
        $roots += & $vswhere -all -prerelease -property installationPath 2>$null
    }
    $roots += @(
        'C:\Program Files\Microsoft Visual Studio',
        'C:\Program Files (x86)\Microsoft Visual Studio'
    )
    foreach ($r in $roots) {
        if (-not $r -or -not (Test-Path $r)) { continue }
        $hit = Get-ChildItem -Path $r -Recurse -Filter 'VSDiagnostics.exe' -ErrorAction SilentlyContinue |
               Select-Object -First 1 -ExpandProperty FullName
        if ($hit) { return $hit }
    }
    return $null
}

if (-not $VsDiagnostics) { $VsDiagnostics = Find-VsDiagnostics }
if (-not $VsDiagnostics -or -not (Test-Path $VsDiagnostics)) {
    throw "VSDiagnostics.exe not found. Pass -VsDiagnostics <path> (Team Tools\DiagnosticsHub\Collector\VSDiagnostics.exe)."
}
$collectorDir = Split-Path $VsDiagnostics -Parent
$configPath = Join-Path $collectorDir (Join-Path 'AgentConfigs' $Config)
if (-not (Test-Path $configPath)) {
    throw "Profiler config not found: $configPath"
}
Write-Output "VSDiagnostics: $VsDiagnostics"
Write-Output "Config:        $configPath"

# --- Paths --------------------------------------------------------------------
$bin = $Bin
$exe = Join-Path $bin 'Launcher.exe'
if (-not (Test-Path $exe)) { throw "Launcher.exe not found at $exe" }

$outDir = Join-Path $PSScriptRoot '..\build\run_and_profile'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$stamp   = Get-Date -Format 'yyyyMMdd_HHmmss'
$session = Join-Path $outDir "engine_$stamp.diagsession"
$outLog  = Join-Path $outDir 'run_stdout.log'
$errLog  = Join-Path $outDir 'run_stderr.log'
if (Test-Path $outLog) { Remove-Item $outLog }
if (Test-Path $errLog) { Remove-Item $errLog }

# Session id must be an integer in [0, 255] (or a GUID); pick a random one to avoid
# colliding with a concurrent collection.
$sessionId = Get-Random -Minimum 1 -Maximum 254

# --- Launch the app (redirected, like run_and_capture) ------------------------
$proc = Start-Process -FilePath $exe -ArgumentList $AppArgs -WorkingDirectory $bin -PassThru `
    -RedirectStandardOutput $outLog -RedirectStandardError $errLog
Write-Output "Started PID $($proc.Id); warming up $WarmupSeconds s..."
Start-Sleep -Seconds $WarmupSeconds
if ($proc.HasExited) {
    Write-Output "App exited during warmup (code $($proc.ExitCode)). See logs below."
    Get-Content $errLog -Tail 50 -ErrorAction SilentlyContinue
    throw "App did not stay alive long enough to profile."
}

# --- Attach profiler and sample ----------------------------------------------
Write-Output "Attaching profiler (session $sessionId) and sampling $Seconds s..."
& $VsDiagnostics start $sessionId "/attach:$($proc.Id)" "/loadConfig:$configPath" "/scratchLocation:$outDir"
if ($LASTEXITCODE -ne 0) {
    if (-not $proc.HasExited) { Stop-Process -Id $proc.Id -Force }
    throw "VSDiagnostics start failed (exit $LASTEXITCODE)."
}
Start-Sleep -Seconds $Seconds

Write-Output "Stopping collection -> $session"
& $VsDiagnostics stop $sessionId "/output:$session"
if ($LASTEXITCODE -ne 0) { Write-Output "WARNING: VSDiagnostics stop returned $LASTEXITCODE" }

# --- Graceful shutdown (same as run_and_capture) ------------------------------
Write-Output "Process exited on its own: $($proc.HasExited)"
if (-not $proc.HasExited) {
    $proc.CloseMainWindow() | Out-Null
    if (-not $proc.WaitForExit(8000)) {
        Write-Output "Graceful close timed out; force killing."
        Stop-Process -Id $proc.Id -Force
    } else {
        Write-Output "Closed gracefully with exit code $($proc.ExitCode)."
    }
    Start-Sleep -Milliseconds 300
}

# --- Expand the trace into raw, CLI-readable files ----------------------------
if ($Expand -and (Test-Path $session)) {
    Write-Output "Expanding diagsession..."
    & $VsDiagnostics expandDiagSession $session | Out-Null
    $expanded = Join-Path $outDir "engine_$stamp"
    if (Test-Path $expanded) {
        Write-Output "Expanded raw trace: $expanded"
        Get-ChildItem $expanded -Recurse -File | Select-Object FullName, Length | Format-Table -AutoSize
    }
}

Write-Output ""
Write-Output "===== ARTIFACTS ====="
Write-Output "DiagSession: $session"
Write-Output "Stdout:      $outLog"
Write-Output "Stderr:      $errLog"
Write-Output ""
Write-Output "Open the .diagsession in Visual Studio for the CPU call tree / hot path,"
Write-Output "or inspect the expanded ETL with xperf/wpaexporter/TraceProcessing for raw counts."
