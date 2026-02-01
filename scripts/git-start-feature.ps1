# Start a new feature branch from develop
param(
    [Parameter(Mandatory=$true)]
    [string]$FeatureName
)

$branchName = "feature/$FeatureName"

Write-Host "Starting feature: $branchName" -ForegroundColor Cyan

# Ensure we're on develop and up to date
git checkout develop
git pull origin develop

# Create and switch to feature branch
git checkout -b $branchName

Write-Host "Created and switched to $branchName" -ForegroundColor Green
Write-Host "When done, run: .\scripts\git-finish-feature.ps1 -FeatureName $FeatureName" -ForegroundColor Yellow
