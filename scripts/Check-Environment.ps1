[CmdletBinding()]
param([string]$EngineRoot)
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'Courier.Tools.psm1') -Force
$root = Resolve-CourierEngine -EngineRoot $EngineRoot
$version = Get-Content -Raw -LiteralPath (Join-Path $root 'Engine/Build/Build.version') | ConvertFrom-Json
Write-Host "Engine: $root"
Write-Host "Version: $($version.MajorVersion).$($version.MinorVersion).$($version.PatchVersion)"
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) { throw 'Visual Studio Installer/vswhere is missing.' }
$vsPath = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsPath) { throw 'Visual Studio C++ x64/x86 tools are missing.' }
Write-Host "Visual Studio: $vsPath"
Get-ChildItem -LiteralPath (Join-Path $vsPath 'VC/Tools/MSVC') -Directory | Select-Object Name
Get-PSDrive -PSProvider FileSystem | Select-Object Name,@{Name='FreeGiB';Expression={[math]::Round($_.Free/1GB,1)}} | Format-Table -AutoSize
Write-Host 'Installation paths exist. Compiler compatibility is established only by an actual UE build.'
