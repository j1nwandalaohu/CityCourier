[CmdletBinding()]
param([ValidateSet('Editor','Game')][string]$Target='Editor', [string]$EngineRoot)
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'Courier.Tools.psm1') -Force
$engine = Resolve-CourierEngine -EngineRoot $EngineRoot
$project = Join-Path (Split-Path -Parent $PSScriptRoot) 'CityCourier.uproject'
$targetName = if ($Target -eq 'Editor') { 'CityCourierEditor' } else { 'CityCourier' }
$logDirectory = New-CourierRunDirectory "Build-$Target"
$batch = Join-Path $engine 'Engine/Build/BatchFiles/Build.bat'
Write-Host "Building $targetName. Log: $logDirectory/build.log"
$previousPreference = $ErrorActionPreference
try {
    $ErrorActionPreference = 'Continue'
    # Discover new source files on every scripted build; cached UBT makefiles can miss them.
    & $batch $targetName Win64 Development "-Project=$project" -WaitMutex -NoHotReloadFromIDE -NoUBTMakefiles 2>&1 | Out-File -LiteralPath (Join-Path $logDirectory 'build.log') -Encoding UTF8
    $code = $LASTEXITCODE
} finally { $ErrorActionPreference = $previousPreference }
Get-Content -LiteralPath (Join-Path $logDirectory 'build.log') -Tail 25
if ($code -ne 0) { throw "UE build failed ($code). Full log: $logDirectory" }
Write-Host 'UE build completed successfully.'
