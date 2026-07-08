# Build + run all host unit tests with gcc directly (no CMake needed).
#
# Usage:  powershell -ExecutionPolicy Bypass -File tests\host\run_tests.ps1
#
# Notes:
#  - Locates the WinLibs/MinGW gcc installed via winget, or any gcc on PATH.
#  - Output exe names get a random suffix so Windows Smart App Control does not
#    block a cached binary hash on repeated runs.

$ErrorActionPreference = "Stop"

function Find-Gcc {
    $c = Get-Command gcc -ErrorAction SilentlyContinue
    if ($c) { return $c.Source }
    $links = "$env:LOCALAPPDATA\Microsoft\WinGet\Links\gcc.exe"
    if (Test-Path $links) { return $links }
    $pkg = Get-ChildItem "$env:LOCALAPPDATA\Microsoft\WinGet\Packages" -Recurse -Filter gcc.exe -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($pkg) { return $pkg.FullName }
    throw "gcc not found. Install: winget install BrechtSanders.WinLibs.POSIX.UCRT"
}

$gcc  = Find-Gcc
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$app  = Resolve-Path "$here\..\..\App"
$bld  = Join-Path $here "build"
New-Item -ItemType Directory -Force -Path $bld | Out-Null

$common = @("-DHK_HOST=1","-std=c11","-Wall","-Wextra","-O1","-I",$app,"-I",$here)

# name -> source files (relative to App, plus the test file in $here)
$tests = [ordered]@{
    "test_crc"     = @("$here\test_crc.c",     "$app\common\crc.c")
    "test_ringbuf" = @("$here\test_ringbuf.c", "$app\common\ringbuf.c")
    "test_sht4x"   = @("$here\test_sht4x.c",   "$app\drivers\sht4x\sht4x.c", "$app\common\crc.c")
    "test_gps"     = @("$here\test_gps.c",     "$app\drivers\gps_ublox\nmea.c")
    "test_actuators" = @("$here\test_actuators.c",
                         "$app\drivers\servo\servo.c",
                         "$app\drivers\buzzer\buzzer.c",
                         "$app\drivers\fan\fan.c",
                         "$app\drivers\battery\battery.c",
                         "$app\drivers\ws2812\ws2812.c")
    "test_services" = @("$here\test_services.c",
                        "$app\common\filters.c",
                        "$app\services\health\health.c")
}

$fail = 0
foreach ($name in $tests.Keys) {
    $sfx = [System.Guid]::NewGuid().ToString("N").Substring(0,6)
    $out = Join-Path $bld "$name-$sfx.exe"
    & $gcc @common @($tests[$name]) "-lm" "-o" $out
    if ($LASTEXITCODE -ne 0) { Write-Host "BUILD FAIL: $name"; $fail = 1; continue }
    try {
        $o = & $out 2>&1
        Write-Host ($o | Out-String).TrimEnd()
        if ($LASTEXITCODE -ne 0) { Write-Host "  -> FAIL ($name)`n"; $fail = 1 }
        else { Write-Host "  -> PASS ($name)`n" }
    } catch {
        Write-Host "  -> BLOCKED/FAILED to run ($name): $($_.Exception.Message.Split([Environment]::NewLine)[0])`n"
        $fail = 1
    }
}

if ($fail -eq 0) { Write-Host "ALL GREEN" } else { Write-Host "SOME FAILED"; exit 1 }
