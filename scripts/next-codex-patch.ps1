param(
    [string]$TaskFile = "tasks/NEXT_PATCH.md",
    [string]$BranchPrefix = "codex/patch",
    [string]$ExtraPrompt = "",
    [switch]$NoBranch,
    [switch]$AllowDirty
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Test-CommandAvailable {
    param([Parameter(Mandatory = $true)][string]$Name)
    return $null -ne (Get-Command $Name -ErrorAction SilentlyContinue)
}

function Invoke-GitChecked {
    param([Parameter(Mandatory = $true)][string[]]$Args)

    & git @Args
    if ($LASTEXITCODE -ne 0) {
        throw "git $($Args -join ' ') failed with exit code $LASTEXITCODE"
    }
}

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path (Join-Path $scriptRoot "..")).Path
Set-Location $repoRoot

$requiredFiles = @(
    "AGENTS.md",
    "ENGINE_ROADMAP.md",
    "ENGINE_STATE.md",
    "QUALITY_GATE.md",
    $TaskFile
)

foreach ($file in $requiredFiles) {
    if (-not (Test-Path -LiteralPath $file)) {
        throw "Required file is missing: $file"
    }
}

if (-not (Test-CommandAvailable "codex")) {
    throw "Codex CLI command not found in PATH. Install or expose 'codex' before running this script."
}

$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$logDir = Join-Path $repoRoot "logs"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null

$logPath = Join-Path $logDir "codex-$stamp.log"
$finalMessagePath = Join-Path $logDir "codex-$stamp-final.md"

$hasGit = Test-CommandAvailable "git"
$insideGit = $false

if ($hasGit) {
    $previousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"

    try {
        $gitRootProbe = & git rev-parse --is-inside-work-tree 2>$null
        $gitRootExitCode = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }

    if ($gitRootExitCode -eq 0 -and $gitRootProbe -and $gitRootProbe.Trim() -eq "true") {
        $insideGit = $true
    }
}

if ($insideGit) {
    $statusBefore = & git status --short
    if ($statusBefore -and (-not $AllowDirty)) {
        Write-Host "Repository has uncommitted changes:" -ForegroundColor Yellow
        $statusBefore | ForEach-Object { Write-Host $_ }
        throw "Refusing to run over a dirty tree. Commit/stash first or pass -AllowDirty."
    }

    if (-not $NoBranch) {
        $branchName = "$BranchPrefix-$stamp"
        Invoke-GitChecked -Args @("checkout", "-b", $branchName)
        Write-Host "Created branch: $branchName"
    }
} else {
    Write-Warning "Git repository was not detected. Codex will run without branch/diff safeguards."
}

$prompt = Get-Content -LiteralPath $TaskFile -Raw
if ($ExtraPrompt.Trim().Length -gt 0) {
    $prompt = $prompt + "`n`nAdditional operator instruction:`n" + $ExtraPrompt.Trim() + "`n"
}

$header = @(
    "AK Engine Codex patch log",
    "Timestamp: $stamp",
    "Repository: $repoRoot",
    "Task file: $TaskFile",
    "Final message file: $finalMessagePath",
    ""
) -join "`n"

$header | Out-File -LiteralPath $logPath -Encoding utf8

Write-Host "Running codex exec. Log: $logPath"

$previousErrorActionPreference = $ErrorActionPreference
$ErrorActionPreference = "Continue"

try {
    $codexOutput = $prompt | & codex exec `
        --cd $repoRoot `
        --sandbox workspace-write `
        --color never `
        --output-last-message $finalMessagePath `
        - 2>&1

    $codexExitCode = $LASTEXITCODE
}
finally {
    $ErrorActionPreference = $previousErrorActionPreference
}

$codexOutput | Tee-Object -FilePath $logPath -Append

if ($codexExitCode -ne 0) {
    throw "codex exec failed with exit code $codexExitCode. See log: $logPath"
}

if ($insideGit) {
    Write-Host ""
    Write-Host "Git status:" -ForegroundColor Cyan
    & git status --short

    Write-Host ""
    Write-Host "Git diff --stat:" -ForegroundColor Cyan
    & git diff --stat

    Write-Host ""
    Write-Host "Review the diff before committing. This script does not commit or push."
}

Write-Host ""
Write-Host "Codex final message:" -ForegroundColor Cyan
if (Test-Path -LiteralPath $finalMessagePath) {
    Get-Content -LiteralPath $finalMessagePath
} else {
    Write-Host "Final message file was not created."
}