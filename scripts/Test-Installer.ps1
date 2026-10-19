[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$Installer,
    [switch]$SkipStartup
)
$ErrorActionPreference='Stop'
Import-Module (Join-Path $PSScriptRoot 'Courier.Tools.psm1') -Force
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskSetup=(Resolve-Path -LiteralPath $Installer).Path
$taskManifest=Get-Content -LiteralPath ([IO.Path]::ChangeExtension($taskSetup,'.json')) -Raw -Encoding UTF8 | ConvertFrom-Json
if((Get-FileHash -LiteralPath $taskSetup -Algorithm SHA256).Hash -ne $taskManifest.installer_sha256){throw 'Installer checksum mismatch.'}
$taskKey='HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\CityCourierPlaytest'
$taskAppKey='HKCU:\Software\CityCourierPlaytest'
$taskDesktop=Join-Path ([Environment]::GetFolderPath('Desktop')) '街头骑手试玩.lnk'
$taskPrograms=Join-Path ([Environment]::GetFolderPath('Programs')) '街头骑手试玩'
foreach($taskExisting in @($taskKey,$taskAppKey,$taskDesktop,$taskPrograms)){
    if(Test-Path -LiteralPath $taskExisting){throw "Test would replace an existing installation or shortcut: $taskExisting"}
}
$taskRun=New-CourierRunDirectory 'Installer-Smoke'
$taskInstall=[IO.Path]::GetFullPath((Join-Path $taskRun 'Installed'))
$taskAllowed=[IO.Path]::GetFullPath((Join-Path $taskRoot 'Saved/Validation'))+[IO.Path]::DirectorySeparatorChar
if(-not $taskInstall.StartsWith($taskAllowed,[StringComparison]::OrdinalIgnoreCase)){throw 'Test installation must stay within this workspace validation directory.'}
$taskSave=Join-Path $env:LOCALAPPDATA 'CityCourier/Saved/SaveGames'
function Get-PlayerSaves {
    if(Test-Path -LiteralPath $taskSave){
        @(Get-ChildItem -LiteralPath $taskSave -File -Recurse | Sort-Object FullName | ForEach-Object {
            $_.FullName+':'+(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
        }) -join "`n"
    }else{''}
}
$taskSavesBefore=Get-PlayerSaves
Write-Host "Testing current-user installer in $taskInstall"
$taskSetupProcess=Start-Process -FilePath $taskSetup -ArgumentList '/S',("/D="+$taskInstall) -WindowStyle Hidden -PassThru -Wait
if($taskSetupProcess.ExitCode -ne 0){throw "Installer exited $($taskSetupProcess.ExitCode)."}
foreach($taskFile in $taskManifest.files){
    $taskPath=[IO.Path]::GetFullPath((Join-Path $taskInstall $taskFile.path))
    if(-not $taskPath.StartsWith($taskInstall+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Invalid manifest path.'}
    if(-not (Test-Path -LiteralPath $taskPath)){throw "Missing installed file: $($taskFile.path)"}
    if((Get-FileHash -LiteralPath $taskPath -Algorithm SHA256).Hash -ne $taskFile.sha256){throw "Installed file hash mismatch: $($taskFile.path)"}
}
if((Get-ItemProperty -LiteralPath $taskKey).InstallLocation -ne $taskInstall){throw 'Uninstall registration points to an unexpected directory.'}
foreach($taskShortcut in @($taskDesktop,(Join-Path $taskPrograms '街头骑手.lnk'),(Join-Path $taskPrograms '卸载.lnk'))){
    if(-not (Test-Path -LiteralPath $taskShortcut)){throw "Missing shortcut: $taskShortcut"}
}
if(-not $SkipStartup){& (Join-Path $PSScriptRoot 'Test-ShippingStartup.ps1') -PackageDirectory $taskInstall -Seconds 30}
$taskNote=Join-Path $taskInstall 'user-note.txt'
[IO.File]::WriteAllText($taskNote,'Preserve player files during uninstall.')
$taskUninstaller=Join-Path $taskInstall 'Uninstall.exe'
# NSIS may start a temporary uninstaller process; wait on removal of its marker.
$taskUninstallProcess=Start-Process -FilePath $taskUninstaller -ArgumentList '/S' -WindowStyle Hidden -PassThru -Wait
if($taskUninstallProcess.ExitCode -ne 0){throw "Uninstaller exited $($taskUninstallProcess.ExitCode)."}
$taskDeadline=[DateTime]::UtcNow.AddSeconds(30)
while((Test-Path -LiteralPath (Join-Path $taskInstall '.citycourier-install.ini')) -and [DateTime]::UtcNow -lt $taskDeadline){Start-Sleep -Milliseconds 250}
foreach($taskFile in $taskManifest.files){if(Test-Path -LiteralPath (Join-Path $taskInstall $taskFile.path)){throw "Uninstaller left shipped file: $($taskFile.path)"}}
foreach($taskRemoved in @($taskKey,$taskAppKey,$taskDesktop,$taskPrograms,(Join-Path $taskInstall '.citycourier-install.ini'))){
    if(Test-Path -LiteralPath $taskRemoved){throw "Uninstaller left registration or shortcut: $taskRemoved"}
}
if(-not (Test-Path -LiteralPath $taskNote)){throw 'Uninstaller removed an extra player file.'}
if((Get-PlayerSaves) -ne $taskSavesBefore){throw 'Installer test changed the real player save files.'}
[ordered]@{
    installer=$taskSetup;installer_sha256=$taskManifest.installer_sha256
    install_directory=$taskInstall;install_scope='current_user';verified_file_count=$taskManifest.files.Count
    startup_tested=(-not [bool]$SkipStartup);uninstalled=$true;extra_file_preserved=$true;player_saves_unchanged=$true
} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $taskRun 'installer-smoke.json') -Encoding UTF8
Write-Host "Installer install/hash/startup/uninstall checks passed: $taskRun"
