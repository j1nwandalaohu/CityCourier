[CmdletBinding()]
param([Parameter(Mandatory)][string]$Script, [string]$EngineRoot)
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'Courier.Tools.psm1') -Force
$engine = Resolve-CourierEngine -EngineRoot $EngineRoot
$projectRoot = Split-Path -Parent $PSScriptRoot
$project = Join-Path $projectRoot 'CityCourier.uproject'
$scriptPath = (Resolve-Path -LiteralPath (Join-Path $projectRoot $Script)).Path
$logDirectory = New-CourierRunDirectory 'EditorScript'
Invoke-CourierExecutable -FilePath (Join-Path $engine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe') -Arguments @($project,'-unattended','-nop4','-nosplash','-run=pythonscript',"-script=$scriptPath",'-stdout','-FullStdOutLogOutput') -LogDirectory $logDirectory -TimeoutSeconds 900
Write-Host "Editor script completed. Logs: $logDirectory"
