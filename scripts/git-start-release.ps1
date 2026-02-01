# Start a release - merge develop to main with version tag
param(
    [Parameter(Mandatory=$true)]
    [string]$Version
)

Write-Host "Starting release: v$Version" -ForegroundColor Cyan

# Ensure develop is up to date
git checkout develop
git pull origin develop

# Switch to main and pull latest
git checkout main
git pull origin main

# Merge develop into main
Write-Host "Merging develop into main..." -ForegroundColor Yellow
git merge --no-ff develop -m "Release v$Version"

if ($LASTEXITCODE -ne 0) {
    Write-Host "Merge conflict! Resolve conflicts and commit manually." -ForegroundColor Red
    exit 1
}

# Create version tag
git tag -a "v$Version" -m "Release v$Version"

# Push main and tags
git push origin main
git push origin --tags

Write-Host "Release v$Version complete!" -ForegroundColor Green
Write-Host "Main branch updated and tagged." -ForegroundColor Green

# Switch back to develop
git checkout develop
