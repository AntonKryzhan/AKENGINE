param(
    [string]$Preset = "windows-vs-debug"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = (Resolve-Path (Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) "..")).Path
Set-Location $repoRoot

function Invoke-Checked {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][scriptblock]$Command
    )

    Write-Host ""
    Write-Host "== $Name ==" -ForegroundColor Cyan
    & $Command

    if ($LASTEXITCODE -ne 0) {
        throw "$Name failed with exit code $LASTEXITCODE"
    }
}

Invoke-Checked "CMake configure" {
    cmake --preset $Preset
}

Invoke-Checked "CMake build" {
    cmake --build --preset $Preset
}

$bin = Join-Path $repoRoot "build\windows-vs-debug\bin\Debug"

$probes = @(
    "ak_editorsceneviewprobe.exe",
    "ak_editortransformgizmoprobe.exe",
    "ak_commandprobe.exe",
    "ak_editormenutoolbarprobe.exe",
    "ak_editorcommandstateprobe.exe"
)

foreach ($probe in $probes) {
    $path = Join-Path $bin $probe

    if (Test-Path -LiteralPath $path) {
        Invoke-Checked $probe {
            & $path
        }
    } else {
        Write-Host "Skipping missing probe: $probe" -ForegroundColor Yellow
    }
}

Write-Host ""
Write-Host "QUALITY GATE PASSED" -ForegroundColor Green