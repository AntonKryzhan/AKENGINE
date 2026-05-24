param(
    [int]$TinyPatches = 5,
    [int]$TinyTimeoutMinutes = 20,
    [int]$SyncTimeoutMinutes = 10
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = (Resolve-Path (Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) "..")).Path
Set-Location $repoRoot

function Require-CleanTree {
    $status = & git status --short
    if ($status) {
        Write-Host "Working tree is not clean:" -ForegroundColor Yellow
        $status | ForEach-Object { Write-Host $_ }
        throw "Refusing to start over a dirty tree."
    }
}

Require-CleanTree

Write-Host ""
Write-Host "===== CODEX CYCLE: $TinyPatches ONE-FILE PATCHES =====" -ForegroundColor Magenta

& .\scripts\auto-codex-loop.ps1 `
    -MaxPatches $TinyPatches `
    -TaskFile "tasks/ONE_FILE_PATCH.md" `
    -TimeoutMinutes $TinyTimeoutMinutes `
    -ExtraPrompt "Strict one-file mode. Change exactly one existing production source/header file. No docs. No ENGINE_STATE. No ROADMAP. No CMake. No probes. No zip. If more than one production file is required, stop without changes."

if ($LASTEXITCODE -ne 0) {
    throw "One-file patch batch failed."
}

Require-CleanTree

Write-Host ""
Write-Host "===== CODEX CYCLE: ARCHITECTURE SYNC =====" -ForegroundColor Magenta

& .\scripts\auto-codex-loop.ps1 `
    -MaxPatches 1 `
    -TaskFile "tasks/ARCHITECTURE_SYNC.md" `
    -TimeoutMinutes $SyncTimeoutMinutes `
    -ExtraPrompt "Architecture sync only. Edit only ENGINE_STATE.md. Review last 5 commits. No code. No docs. No probes. No CMake. No zip."

if ($LASTEXITCODE -ne 0) {
    throw "Architecture sync failed."
}

Write-Host ""
Write-Host "CODEX 5+1 CYCLE FINISHED" -ForegroundColor Green
git --no-pager log --oneline -8