# Create or finish a hotfix
param(
    [Parameter(Mandatory=$true)]
    [string]$HotfixName,
    
    [Parameter(Mandatory=$true)]
    [ValidateSet("start", "finish")]
    [string]$Action,
    
    [string]$Version  # Required for finish
)

$branchName = "hotfix/$HotfixName"

if ($Action -eq "start") {
    Write-Host "Starting hotfix: $branchName" -ForegroundColor Cyan
    
    # Hotfixes branch from main
    git checkout main
    git pull origin main
    git checkout -b $branchName
    
    Write-Host "Created $branchName from main" -ForegroundColor Green
    Write-Host "Make your fix, then run: .\scripts\git-hotfix.ps1 -HotfixName $HotfixName -Action finish -Version X.X.X" -ForegroundColor Yellow
}
elseif ($Action -eq "finish") {
    if (-not $Version) {
        Write-Host "Error: -Version is required for finish" -ForegroundColor Red
        exit 1
    }
    
    Write-Host "Finishing hotfix: $branchName (v$Version)" -ForegroundColor Cyan
    
    # Merge to main
    git checkout main
    git pull origin main
    git merge --no-ff $branchName -m "Hotfix v$Version: $HotfixName"
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Merge conflict on main! Resolve and retry." -ForegroundColor Red
        exit 1
    }
    
    # Tag the hotfix
    git tag -a "v$Version" -m "Hotfix v$Version: $HotfixName"
    git push origin main --tags
    
    # Also merge to develop
    git checkout develop
    git pull origin develop
    git merge --no-ff $branchName -m "Merge hotfix/$HotfixName into develop"
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Merge conflict on develop! Resolve manually." -ForegroundColor Red
        exit 1
    }
    
    git push origin develop
    
    # Delete hotfix branch
    git branch -d $branchName
    git push origin --delete $branchName
    
    Write-Host "Hotfix v$Version complete! Merged to main and develop." -ForegroundColor Green
}
