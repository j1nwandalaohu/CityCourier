[CmdletBinding()]
param(
    [string]$EngineRoot,
    [string]$Compiler,
    [string]$PackageDirectory,
    [string]$RuntimeRedistributable,
    [string]$OutputDirectory,
    [switch]$SkipPackage,
    [string]$BuildTag=(Get-Date -Format 'yyyyMMdd-HHmm')
)
$ErrorActionPreference='Stop'
Import-Module (Join-Path $PSScriptRoot 'Courier.Tools.psm1') -Force
$taskProjectRoot=Split-Path -Parent $PSScriptRoot
if(-not $PackageDirectory){$PackageDirectory=Join-Path $taskProjectRoot 'Artifacts/Windows/Shipping'}
if(-not $SkipPackage){& (Join-Path $PSScriptRoot 'Package.ps1') -Configuration Shipping -EngineRoot $EngineRoot}
$taskPackage=(Resolve-Path -LiteralPath $PackageDirectory).Path
if(-not $Compiler){$Compiler=Join-Path $taskProjectRoot '.dependencies/installer-tools/nsis-3.13/makensis.exe'}
if(-not (Test-Path -LiteralPath $Compiler)){throw 'NSIS 3.13 compiler not found. Pass -Compiler or extract the official portable compiler under .dependencies/installer-tools.'}
if($BuildTag -notmatch '^[A-Za-z0-9._-]+$'){throw 'BuildTag must contain only letters, digits, dots, underscores or hyphens.'}
$taskGame=Join-Path $taskPackage 'CityCourier.exe'
$taskRuntime=Join-Path $taskPackage 'Engine/Extras/Redist/en-us/vc_redist.x64.exe'
foreach($taskRequired in @($taskGame,$taskRuntime)){if(-not (Test-Path -LiteralPath $taskRequired)){throw "Required package file missing: $taskRequired"}}
# UE's bundled redistributable can predate the compiler used for this build.
# Prefer the matching local VS runtime when newer; permit an explicit source.
if(-not $RuntimeRedistributable){
    $taskVSRuntime='C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Redist/MSVC/v143/vc_redist.x64.exe'
    if(Test-Path -LiteralPath $taskVSRuntime){$RuntimeRedistributable=$taskVSRuntime}
}
if($RuntimeRedistributable){
    $taskProvidedRuntime=(Resolve-Path -LiteralPath $RuntimeRedistributable).Path
    $taskProvidedVersion=[version][Diagnostics.FileVersionInfo]::GetVersionInfo($taskProvidedRuntime).ProductVersion
    $taskBundledVersion=[version][Diagnostics.FileVersionInfo]::GetVersionInfo($taskRuntime).ProductVersion
    if($taskProvidedVersion -gt $taskBundledVersion){Copy-Item -LiteralPath $taskProvidedRuntime -Destination $taskRuntime -Force}
}
$taskOutput=if($OutputDirectory){[IO.Path]::GetFullPath($OutputDirectory)}else{Join-Path $taskProjectRoot 'Artifacts/Installers'}
New-Item -ItemType Directory -Path $taskOutput -Force | Out-Null
$taskRun=New-CourierRunDirectory 'Installer'
$taskFiles=@(Get-ChildItem -LiteralPath $taskPackage -File -Recurse | Where-Object {
    $_.Extension -ne '.pdb' -and $_.FullName.Substring($taskPackage.Length+1) -notmatch '(^|[\\/])Saved[\\/]' -and $_.Name -ne 'vc_redist.arm64.exe'
} | Sort-Object FullName)
if(-not ($taskFiles | Where-Object FullName -EQ $taskGame) -or -not ($taskFiles | Where-Object FullName -EQ $taskRuntime)){throw 'Payload filtering unexpectedly removed the game or offline runtime.'}
function ConvertTo-NsisString([string]$Value){return $Value.Replace('$','$$').Replace('"','$\"')}
$taskInstall=[Collections.Generic.List[string]]::new()
$taskUninstall=[Collections.Generic.List[string]]::new()
$taskDirectories=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
$taskManifest=@()
foreach($taskFile in $taskFiles){
    $taskRelative=$taskFile.FullName.Substring($taskPackage.Length+1)
    $taskParent=Split-Path -Parent $taskRelative
    $taskDestination=if($taskParent){'$INSTDIR\'+(ConvertTo-NsisString $taskParent)}else{'$INSTDIR'}
    $taskInstall.Add('SetOutPath "'+$taskDestination+'"')
    $taskInstall.Add('File "'+(ConvertTo-NsisString $taskFile.FullName)+'"')
    $taskUninstall.Add('Delete "$INSTDIR\'+(ConvertTo-NsisString $taskRelative)+'"')
    while($taskParent){[void]$taskDirectories.Add($taskParent);$taskParent=Split-Path -Parent $taskParent}
    $taskManifest += [pscustomobject]@{path=$taskRelative;bytes=$taskFile.Length;sha256=(Get-FileHash -LiteralPath $taskFile.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}
}
foreach($taskDirectory in @($taskDirectories | Sort-Object Length -Descending)){$taskUninstall.Add('RMDir "$INSTDIR\'+(ConvertTo-NsisString $taskDirectory)+'"')}
$taskInstallInclude=Join-Path $taskRun 'install-files.nsh'
$taskUninstallInclude=Join-Path $taskRun 'uninstall-files.nsh'
$taskUtf8=[Text.UTF8Encoding]::new($true)
[IO.File]::WriteAllLines($taskInstallInclude,$taskInstall,$taskUtf8)
[IO.File]::WriteAllLines($taskUninstallInclude,$taskUninstall,$taskUtf8)
$taskSetup=Join-Path $taskOutput "CityCourier-Setup-$BuildTag.exe"
$taskMinimumRuntime=[Diagnostics.FileVersionInfo]::GetVersionInfo($taskRuntime).ProductVersion
$taskKB=[math]::Ceiling(($taskFiles | Measure-Object Length -Sum).Sum/1KB)
$taskCompilerArguments=@('/V3','/INPUTCHARSET','UTF8',"/DSETUP_FILE=$taskSetup","/DBUILD_TAG=$BuildTag","/DINSTALLED_KB=$taskKB","/DVC_MIN_VERSION=$taskMinimumRuntime","/DINSTALL_INCLUDE=$taskInstallInclude","/DUNINSTALL_INCLUDE=$taskUninstallInclude",(Join-Path $taskProjectRoot 'installer/CityCourier.nsi'))
Write-Host "Building offline Windows installer: $taskSetup"
& $Compiler @taskCompilerArguments 2>&1 | Tee-Object -FilePath (Join-Path $taskRun 'compiler.log')
if($LASTEXITCODE -ne 0){throw "NSIS failed ($LASTEXITCODE). Logs: $taskRun"}
if(-not (Test-Path -LiteralPath $taskSetup)){throw 'Compiler did not produce the expected installer.'}
$taskMetadata=[ordered]@{
    build=$BuildTag;configuration='Shipping';package_directory=$taskPackage;installer=$taskSetup
    installer_bytes=(Get-Item -LiteralPath $taskSetup).Length;installer_sha256=(Get-FileHash -LiteralPath $taskSetup -Algorithm SHA256).Hash.ToLowerInvariant()
    minimum_vc_runtime=$taskMinimumRuntime;offline_runtime_included=$true;install_scope='current_user';saves_removed_on_uninstall=$false;files=$taskManifest
}
$taskMetadata | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $taskOutput "CityCourier-Setup-$BuildTag.json") -Encoding UTF8
($taskMetadata.installer_sha256+'  '+[IO.Path]::GetFileName($taskSetup)) | Set-Content -LiteralPath (Join-Path $taskOutput "CityCourier-Setup-$BuildTag.sha256") -Encoding ASCII
Write-Host "Installer ready: $taskSetup"
