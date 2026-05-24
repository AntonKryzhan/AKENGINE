param(
    [int]$MaxPatches = 1,
    [string]$TaskFile = "tasks/NEXT_PATCH.md",
    [string]$ExtraPrompt = "",
    [int]$TimeoutMinutes = 90,
    [switch]$Push
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = (Resolve-Path (Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) "..")).Path
Set-Location $repoRoot

function Invoke-Git {
    param([Parameter(Mandatory = $true)][string[]]$Args)

    & git @Args
    if ($LASTEXITCODE -ne 0) {
        throw "git $($Args -join ' ') failed with exit code $LASTEXITCODE"
    }
}

function Require-CleanTree {
    $status = & git status --short
    if ($status) {
        Write-Host "Working tree is not clean:" -ForegroundColor Yellow
        $status | ForEach-Object { Write-Host $_ }
        throw "Refusing to start automation over dirty tree."
    }
}

function Invoke-CodexPatch {
    param(
        [Parameter(Mandatory = $true)][string]$Prompt,
        [Parameter(Mandatory = $true)][string]$LogPath,
        [Parameter(Mandatory = $true)][string]$FinalPath,
        [Parameter(Mandatory = $true)][int]$TimeoutMinutes
    )

    $help = (& codex exec --help 2>&1) -join "`n"

    $args = @(
        "exec",
        "--cd", $repoRoot,
        "--sandbox", "workspace-write"
    )

    if ($help -match "--color") {
        $args += @("--color", "never")
    }

    if ($help -match "--output-last-message") {
        $args += @("--output-last-message", $FinalPath)
    }

    $args += @("-")

    $job = Start-Job -ScriptBlock {
        param($PromptText, $CodexArgs, $Root)

        Set-Location $Root
        $PromptText | & codex @CodexArgs 2>&1
        $global:LASTEXITCODE
    } -ArgumentList $Prompt, $args, $repoRoot

    $started = Get-Date

    while ($job.State -eq "Running") {
        Start-Sleep -Seconds 30

        $elapsed = (Get-Date) - $started
        Write-Host ("Codex running... {0:n1} min" -f $elapsed.TotalMinutes)

        if ($elapsed.TotalMinutes -ge $TimeoutMinutes) {
            Stop-Job $job -ErrorAction SilentlyContinue
            Remove-Job $job -Force -ErrorAction SilentlyContinue
            throw "Codex timed out after $TimeoutMinutes minutes. Log: $LogPath"
        }
    }

    $output = Receive-Job $job
    Remove-Job $job -Force

    $output | Out-File -LiteralPath $LogPath -Encoding utf8

    $lastLine = $output | Select-Object -Last 1
    if ($lastLine -match "^\d+$" -and [int]$lastLine -ne 0) {
        throw "Codex failed with exit code $lastLine. Log: $LogPath"
    }
}

if (-not (Get-Command codex -ErrorAction SilentlyContinue)) {
    throw "codex not found in PATH"
}

if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    throw "git not found in PATH"
}

if (-not (Test-Path -LiteralPath $TaskFile)) {
    throw "Task file not found: $TaskFile"
}

$runStamp = Get-Date -Format "yyyyMMdd-HHmmss"
$branch = "codex/auto-$runStamp"

Require-CleanTree
Invoke-Git -Args @("checkout", "-b", $branch)

$logDir = Join-Path $repoRoot "logs"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null

for ($i = 1; $i -le $MaxPatches; $i++) {
    Write-Host ""
    Write-Host "===== AUTO CODEX PATCH $i / $MaxPatches =====" -ForegroundColor Magenta

    Require-CleanTree

    $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $logPath = Join-Path $logDir "auto-codex-$stamp.log"
    $finalPath = Join-Path $logDir "auto-codex-$stamp-final.md"

    $prompt = Get-Content -LiteralPath $TaskFile -Raw

    $prompt += @"

Additional automation rules:
- Produce exactly one controlled AK Engine patch.
- Do not commit.
- Do not push.
- Do not modify build/, logs/, .git/, or generated binaries.
- Run or document the checks from QUALITY_GATE.md.
- Keep the patch small enough to review but dense enough to be meaningful.
"@

    if ($ExtraPrompt.Trim().Length -gt 0) {
        $prompt += "`nAdditional operator instruction:`n$($ExtraPrompt.Trim())`n"
    }

    Invoke-CodexPatch -Prompt $prompt -LogPath $logPath -FinalPath $finalPath -TimeoutMinutes $TimeoutMinutes

    Write-Host ""
    Write-Host "Git diff after Codex:" -ForegroundColor Cyan
    git --no-pager diff --stat

    $changed = & git status --short
    if (-not $changed) {
        throw "Codex finished but produced no changes."
    }

    Write-Host ""
    Write-Host "Running quality gate..." -ForegroundColor Cyan
    & powershell -ExecutionPolicy Bypass -File ".\scripts\quality-gate.ps1"
    if ($LASTEXITCODE -ne 0) {
        throw "Quality gate failed. Fix manually before continuing."
    }

    git add -A
    git reset -- build logs .akcache 2>$null

    $statusAfterAdd = & git status --short
    if (-not $statusAfterAdd) {
        throw "No committable changes after filtering generated directories."
    }

    $commitMessage = "Codex auto patch $i"
    if (Test-Path -LiteralPath $finalPath) {
        $firstLine = Get-Content -LiteralPath $finalPath | Where-Object { $_.Trim().Length -gt 0 } | Select-Object -First 1
        if ($firstLine) {
            $safeLine = $firstLine.Trim()
            if ($safeLine.Length -gt 72) {
                $safeLine = $safeLine.Substring(0, 72)
            }
            $commitMessage = $safeLine
        }
    }

    Invoke-Git -Args @("commit", "-m", $commitMessage)

    Write-Host ""
	Write-Host "Committed patch ${i}: $commitMessage" -ForegroundColor Green

    if ($Push) {
        Invoke-Git -Args @("push", "-u", "origin", $branch)
    }
}

Write-Host ""
Write-Host "AUTOMATION FINISHED" -ForegroundColor Green
Write-Host "Branch: $branch"
git --no-pager log --oneline -5