[CmdletBinding()]
param([ValidateRange(1200,3600)][int]$MinimumSeconds=1200,[switch]$Visible,[ValidateSet('All','Preflight','Play')][string]$Checks='All')
$ErrorActionPreference='Stop'
Import-Module (Join-Path $PSScriptRoot 'Courier.Tools.psm1') -Force
$projectRoot=Split-Path -Parent $PSScriptRoot
$game=Join-Path $projectRoot 'Artifacts/Windows/Development/CityCourier/Binaries/Win64/CityCourier.exe'
if(-not (Test-Path -LiteralPath $game)){throw 'Build the Development package first with scripts/Package.ps1.'}
$root=New-CourierRunDirectory 'Interiors-Acceptance'
$userDirectory=Join-Path $root 'User'
function Invoke-InteriorCheck([string]$Mode,[string]$Name,[int]$Timeout) {
    $output=Join-Path $root $Name
    New-Item -ItemType Directory -Path $output -Force | Out-Null
    Set-Content -LiteralPath (Join-Path $projectRoot 'Saved/Validation/current-interior-run.txt') -Value $output -Encoding UTF8
    Set-Content -LiteralPath (Join-Path $projectRoot 'Saved/Validation/current-interior-mode.txt') -Value $Mode -Encoding UTF8
    $arguments=@('-windowed','-unattended','-nosplash','-CourierValidation',"-CourierValidationMode=$Mode","-CourierValidationSeconds=$MinimumSeconds","-CourierValidationOutput=$output","-UserDir=$userDirectory","-abslog=$output/game.log")
    if(-not $Visible){$arguments+='-RenderOffscreen'}
    Invoke-CourierExecutable -FilePath $game -Arguments $arguments -LogDirectory $output -TimeoutSeconds $Timeout
    $result=Get-Content -LiteralPath (Join-Path $output 'result.json') -Raw | ConvertFrom-Json
    if(-not ($result.PSObject.Properties.Name -contains 'error') -or $result.error){throw "Native validation failed: $($result.error). See $output"}
    if(Select-String -LiteralPath (Join-Path $output 'game.log') -Pattern 'Default Material will be used in game|Fatal error:|Assertion failed:|Virtual Shadow Map Page Pool overflow' -Quiet){throw "Packaged rendering/runtime failure. See $output"}
    return $output
}
if($Checks -ne 'Play'){
    $null=Invoke-InteriorCheck 'InteriorsBikeCheck' 'Bike' 90
    $benchmark=Invoke-InteriorCheck 'InteriorsPerformance' 'Performance' 300
    $performance=& (Join-Path $PSScriptRoot 'Measure-Performance.ps1') -CsvPath (Join-Path $benchmark 'frames.csv')
    if(-not $performance.meets_target){throw 'Packaged performance did not meet 30 mean / 24 FPS 1% low.'}
    $null=Invoke-InteriorCheck 'CharacterActions' 'Actions' 90
}
if($Checks -ne 'Preflight'){
    $play=Invoke-InteriorCheck 'Interiors' 'Play' ($MinimumSeconds+1250)
    $coverage=Get-Content -LiteralPath (Join-Path $play 'interior-play.json') -Raw | ConvertFrom-Json
    if($coverage.elapsed_seconds -lt $MinimumSeconds -or $coverage.layout_classes -ne 10 -or $coverage.building_count -ne 293 -or $coverage.door_count -ne 15697 -or $coverage.completed_indoor_orders -lt 1 -or $coverage.cache_return_cycles -lt 2){throw 'Native playable coverage is incomplete; inspect interior-play.json.'}
    $null=Invoke-InteriorCheck 'InteriorsReload' 'Reload' 90
}
Write-Host "Interior $Checks validation passed. Evidence: $root"
