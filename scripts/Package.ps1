[CmdletBinding()]
param([ValidateSet('Development','Shipping')][string]$Configuration='Development', [string]$EngineRoot)
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'Courier.Tools.psm1') -Force
$engine = Resolve-CourierEngine -EngineRoot $EngineRoot
$projectRoot = Split-Path -Parent $PSScriptRoot
$project = Join-Path $projectRoot 'CityCourier.uproject'
$map = Join-Path $projectRoot 'Content/CityCourier/Maps/L_CourierCity.umap'
if (-not (Test-Path -LiteralPath $map)) { throw 'Startup map is missing. Build the editor and run bootstrap_project.py first.' }
$archive = Join-Path $projectRoot "Artifacts/Windows/$Configuration"
$logDirectory = New-CourierRunDirectory "Package-$Configuration"
$batch = Join-Path $engine 'Engine/Build/BatchFiles/RunUAT.bat'
Write-Host "Packaging to $archive. Log: $logDirectory/package.log"
$previousPreference = $ErrorActionPreference
try {
    $ErrorActionPreference = 'Continue'
    & $batch BuildCookRun "-project=$project" -noP4 -unattended -utf8output -platform=Win64 "-clientconfig=$Configuration" -build -cook -stage -pak -archive -prereqs "-archivedirectory=$archive" 2>&1 | Out-File -LiteralPath (Join-Path $logDirectory 'package.log') -Encoding UTF8
    $code = $LASTEXITCODE
} finally { $ErrorActionPreference = $previousPreference }
Get-Content -LiteralPath (Join-Path $logDirectory 'package.log') -Tail 25
if ($code -ne 0) { throw "UE packaging failed ($code). Full log: $logDirectory" }
$game = Join-Path $archive 'Windows/CityCourier.exe'
if (-not (Test-Path -LiteralPath $game)) { throw "UAT reported success but expected executable is missing: $game" }
Write-Host "Windows package created: $game"
