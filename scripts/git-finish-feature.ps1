# Finish a feature branch - merge to develop
param(
    [Parameter(Mandatory=$true)]
    [string]$FeatureName,
    
    [switch]$DeleteBranch = $false
)

$branchName = "feature/$FeatureName"

Write-Host "Finishing feature: $branchName" -ForegroundColor Cyan

# Ensure the feature branch exists
$branchExists = git branch --list $branchName
if (-not $branchExists) {
    Write-Host "Error: Branch $branchName does not exist" -ForegroundColor Red
    exit 1
}

# Switch to develop and pull latest
git checkout develop
git pull origin develop

# Merge feature branch
Write-Host "Merging $branchName into develop..." -ForegroundColor Yellow
git merge --no-ff $branchName -m "Merge $branchName into develop"

if ($LASTEXITCODE -ne 0) {
    Write-Host "Merge conflict! Resolve conflicts and commit manually." -ForegroundColor Red
    exit 1
}

# Push develop
git push origin develop

Write-Host "Feature merged to develop successfully!" -ForegroundColor Green

# Optionally delete the feature branch
if ($DeleteBranch) {
    git branch -d $branchName
    git push origin --delete $branchName
    Write-Host "Deleted branch $branchName" -ForegroundColor Yellow
} else {
    Write-Host "Feature branch $branchName kept. Use -DeleteBranch to remove it." -ForegroundColor Yellow
}
