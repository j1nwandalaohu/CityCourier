[CmdletBinding()]
param([Parameter(Mandatory)][string]$Script, [string]$EngineRoot, [switch]$Gui)
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'Courier.Tools.psm1') -Force
$engine = Resolve-CourierEngine -EngineRoot $EngineRoot
$projectRoot = Split-Path -Parent $PSScriptRoot
$project = Join-Path $projectRoot 'CityCourier.uproject'
$scriptPath = (Resolve-Path -LiteralPath (Join-Path $projectRoot $Script)).Path
$logDirectory = New-CourierRunDirectory 'EditorScript'
$editorName = if ($Gui) { 'UnrealEditor.exe' } else { 'UnrealEditor-Cmd.exe' }
$scriptArguments = if ($Gui) { @("-ExecutePythonScript=$scriptPath") } else { @('-run=pythonscript',"-script=$scriptPath") }
Invoke-CourierExecutable -FilePath (Join-Path $engine "Engine/Binaries/Win64/$editorName") -Arguments (@($project,'-unattended','-nop4','-nosplash','-stdout','-FullStdOutLogOutput') + $scriptArguments) -LogDirectory $logDirectory -TimeoutSeconds 900
if (Select-String -LiteralPath (Join-Path $logDirectory 'stdout.log') -Pattern 'LogPython: Error:|LogEditorPythonExecuter: Error:' -Quiet) {
    throw "Editor reported a Python error even though the process exited normally. Logs: $logDirectory"
}
Write-Host "Editor script completed. Logs: $logDirectory"
