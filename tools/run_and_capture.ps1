param(
    [int]$Seconds = 6
)
$ErrorActionPreference = 'Stop'
$bin = 'C:\Users\jercl\Projects\Toybox\Engine\build\tbx-examples-clang\bin\Debug'
$exe = Join-Path $bin 'Launcher.exe'
$outDir = 'C:\Users\jercl\Projects\Toybox\Engine\build\run_and_capture'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$outLog = Join-Path $outDir 'run_stdout.log'
$errLog = Join-Path $outDir 'run_stderr.log'
$shot   = Join-Path $outDir 'run_screenshot.png'

if (Test-Path $outLog) { Remove-Item $outLog }
if (Test-Path $errLog) { Remove-Item $errLog }
if (Test-Path $shot)   { Remove-Item $shot }

$proc = Start-Process -FilePath $exe -WorkingDirectory $bin -PassThru `
    -RedirectStandardOutput $outLog -RedirectStandardError $errLog
Write-Output "Started PID $($proc.Id), waiting $Seconds s..."
Start-Sleep -Seconds $Seconds

# Capture the GL window via PrintWindow(PW_RENDERFULLCONTENT) which grabs hardware/DWM content
# (GDI CopyFromScreen returns black for OpenGL surfaces).
try {
    Add-Type -AssemblyName System.Drawing
    $sig = @'
using System;
using System.Runtime.InteropServices;
public static class WinCap {
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr hwnd, IntPtr hdcBlt, uint nFlags);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr hwnd, out RECT lpRect);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hwnd);
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
}
'@
    Add-Type -TypeDefinition $sig
    $hwnd = $proc.MainWindowHandle
    [WinCap]::SetForegroundWindow($hwnd) | Out-Null
    Start-Sleep -Milliseconds 400
    $r = New-Object WinCap+RECT
    [WinCap]::GetClientRect($hwnd, [ref]$r) | Out-Null
    $w = [Math]::Max($r.Right - $r.Left, 1); $h = [Math]::Max($r.Bottom - $r.Top, 1)
    $bmp = New-Object System.Drawing.Bitmap($w, $h)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $hdc = $g.GetHdc()
    # PW_RENDERFULLCONTENT = 0x00000002
    [WinCap]::PrintWindow($hwnd, $hdc, 2) | Out-Null
    $g.ReleaseHdc($hdc)
    $bmp.Save($shot, [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
    Write-Output "Screenshot saved: $shot ($w x $h)"
} catch {
    Write-Output "Screenshot failed: $($_.Exception.Message)"
}

# Graceful shutdown: send WM_CLOSE so the engine tears down normally (so we can verify clean logs).
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
Write-Output "===== STDOUT TAIL ====="
if (Test-Path $outLog) { Get-Content $outLog -Tail 200 }
Write-Output "===== STDERR TAIL ====="
if (Test-Path $errLog) { Get-Content $errLog -Tail 200 }
