Set-Location $PSScriptRoot\..\

# Stage changes
git add .

# Commit using prepared message
$commitMsgFile = ".git-commit-msgs/0002-move-timer-to-drivers.txt"
if (Test-Path $commitMsgFile) {
    git commit -F $commitMsgFile
}
else {
    git commit -m "chore(rl_sport): move timer to drivers/ and update includes"
}

# Show status
git status --porcelain
git log -n 1 --pretty=oneline
