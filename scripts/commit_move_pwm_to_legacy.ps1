# Commit script for moving pwm_timer to legacy
# Run this from a PowerShell that has git available.

Set-Location 'd:\Nuvoton\M480_RL_Project'
Write-Host "Git status (uncommitted changes):"
git status --porcelain

Write-Host "Staging all changes..."
git add -A

$commitMsg = @'
chore(rl_sport): move pwm_timer to legacy and mark DEPRECATED

Move `pwm_timer.c`/`pwm_timer.h` into `SampleCode/RL_SPORT/legacy/` and annotate header as DEPRECATED.
Update `docs/SampleCode/RL_SPORT/REFACTORING_VALIDATION_PLAN.md`.
'@

Write-Host "Committing with message:"
Write-Host $commitMsg

git commit -m $commitMsg

if ($LASTEXITCODE -eq 0) {
    Write-Host "Commit succeeded. Last commit files:"
    git --no-pager show --name-only --pretty=format:"Committed: %h %s" HEAD
}
else {
    Write-Error "Commit failed. Inspect 'git status' and try again."
}
