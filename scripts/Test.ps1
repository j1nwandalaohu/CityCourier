[CmdletBinding()]
param([ValidatePattern('^[A-Za-z0-9_]+(\.[A-Za-z0-9_]+)*$')][string]$Filter='CityCourier', [switch]$Headless, [string]$EngineRoot, [ValidateRange(1,86400)][int]$TimeoutSeconds=900)
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'Courier.Tools.psm1') -Force
$engine = Resolve-CourierEngine -EngineRoot $EngineRoot
$project = Join-Path (Split-Path -Parent $PSScriptRoot) 'CityCourier.uproject'
$logDirectory = New-CourierRunDirectory 'Automation'
$reportDirectory = Join-Path $logDirectory 'Report'
$arguments = @($project,'-unattended','-nop4','-nosplash','-stdout','-FullStdOutLogOutput',"-ExecCmds=Automation RunTests $Filter",'-TestExit=Automation Test Queue Empty',"-ReportExportPath=$reportDirectory")
if ($Headless) { $arguments += '-NullRHI' }
Invoke-CourierExecutable -FilePath (Join-Path $engine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe') -Arguments $arguments -LogDirectory $logDirectory -TimeoutSeconds $TimeoutSeconds
$result = Assert-CourierAutomationReport -ReportPath (Join-Path $reportDirectory 'index.json') -Filter $Filter
Write-Host "$($result.Passed) verified successful tests. Report: $($result.ReportPath)"
