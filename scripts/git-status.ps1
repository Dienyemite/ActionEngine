# Show Git Flow status overview
Write-Host "`n=== ActionEngine Git Flow Status ===" -ForegroundColor Cyan

# Current branch
$currentBranch = git branch --show-current
Write-Host "`nCurrent branch: " -NoNewline
Write-Host $currentBranch -ForegroundColor Green

# Show all branches organized by type
Write-Host "`n--- Branches ---" -ForegroundColor Yellow

Write-Host "`nMain branches:" -ForegroundColor White
git branch --list "main" "develop" | ForEach-Object { Write-Host "  $_" }

$features = git branch --list "feature/*"
if ($features) {
    Write-Host "`nFeature branches:" -ForegroundColor White
    $features | ForEach-Object { Write-Host "  $_" }
}

$hotfixes = git branch --list "hotfix/*"
if ($hotfixes) {
    Write-Host "`nHotfix branches:" -ForegroundColor White
    $hotfixes | ForEach-Object { Write-Host "  $_" }
}

$releases = git branch --list "release/*"
if ($releases) {
    Write-Host "`nRelease branches:" -ForegroundColor White
    $releases | ForEach-Object { Write-Host "  $_" }
}

# Recent commits on current branch
Write-Host "`n--- Recent Commits ($currentBranch) ---" -ForegroundColor Yellow
git log --oneline -5

# Uncommitted changes
$status = git status --porcelain
if ($status) {
    Write-Host "`n--- Uncommitted Changes ---" -ForegroundColor Yellow
    git status --short
}

Write-Host "`n" 
