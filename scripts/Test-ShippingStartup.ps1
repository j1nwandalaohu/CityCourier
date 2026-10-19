[CmdletBinding()]
param(
    [ValidateRange(10,120)][int]$Seconds=30,
    [string]$PackageDirectory
)
$ErrorActionPreference='Stop'
Import-Module (Join-Path $PSScriptRoot 'Courier.Tools.psm1') -Force
$projectRoot=Split-Path -Parent $PSScriptRoot
if(-not $PackageDirectory){$PackageDirectory=Join-Path $projectRoot 'Artifacts/Windows/Shipping'}
$binaries=Join-Path $PackageDirectory 'CityCourier/Binaries/Win64'
$game=@(Get-ChildItem -LiteralPath $binaries -Filter '*.exe' | Where-Object Name -Like 'CityCourier*')
if($game.Count -ne 1){throw 'Build the Shipping package before this smoke check.'}
$run=New-CourierRunDirectory 'Interiors-Shipping'
if(-not ('CourierShippingWindow' -as [type])) {
    Add-Type @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class CourierShippingWindow {
    public delegate bool EnumCallback(IntPtr hwnd, IntPtr data);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumCallback callback,IntPtr data);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hwnd,out uint pid);
    [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr hwnd,StringBuilder text,int count);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hwnd,uint msg,IntPtr w,IntPtr l);
}
'@
}
$launchArgs=@('-windowed','-RenderOffscreen','-unattended','-nosplash','-ResX=1920','-ResY=1080',"-UserDir=$run/User",'-CourierValidation','-CourierValidationMode=Interiors',"-CourierValidationOutput=$run")
$taskModule=Get-Module Courier.Tools
$quoted=@($launchArgs | ForEach-Object { & $taskModule {param($value) ConvertTo-CourierProcessArgument $value} $_ }) -join ' '
$process=Start-Process -FilePath $game[0].FullName -ArgumentList $quoted -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $run 'stdout.log') -RedirectStandardError (Join-Path $run 'stderr.log')
$normalExit=$false
try {
    $null=$process.Handle
    if($process.WaitForExit($Seconds*1000)){throw "Shipping exited during startup ($($process.ExitCode))."}
    $process.Refresh()
    $peak=$process.PeakWorkingSet64/1MB
    if($peak -lt 400){throw 'Shipping did not reach the expected loaded-map working set.'}
    if(Test-Path -LiteralPath (Join-Path $run 'progress.json')){throw 'Development validation unexpectedly ran in Shipping.'}
    $windows=[Collections.Generic.List[IntPtr]]::new()
    $classes=[Collections.Generic.List[string]]::new()
    $callback=[CourierShippingWindow+EnumCallback]{param($window,$data)
        [uint32]$windowOwner=0
        [void][CourierShippingWindow]::GetWindowThreadProcessId($window,[ref]$windowOwner)
        if($windowOwner -eq $process.Id){
            $name=[Text.StringBuilder]::new(256)
            [void][CourierShippingWindow]::GetClassName($window,$name,256)
            if($name.ToString() -like 'UnrealWindow*'){$windows.Add($window);$classes.Add($name.ToString())}
        }
        return $true
    }
    [void][CourierShippingWindow]::EnumWindows($callback,[IntPtr]::Zero)
    foreach($window in $windows){[void][CourierShippingWindow]::PostMessage($window,0x0010,[IntPtr]::Zero,[IntPtr]::Zero)}
    $cleanup='native window close'
    if($windows.Count -eq 0){
        # Startup can render without an interactive top-level window. This is
        # a liveness/profile smoke check, not a claim about physical UI input.
        $cleanup='terminated owned headless test process'
        $process.Kill();$process.WaitForExit()
    } else {
        if(-not $process.WaitForExit(15000)){throw 'Shipping did not close after the targeted native close message.'}
        $normalExit=$process.ExitCode -eq 0
        if(-not $normalExit){throw "Shipping close returned $($process.ExitCode)."}
    }
    $profile=@(Get-ChildItem -LiteralPath (Join-Path $run 'User') -Recurse -Filter 'CourierProfile.sav')
    if($profile.Count -ne 1){throw 'Shipping profile initialization was not observed.'}
    [pscustomobject]@{startup_seconds=$Seconds;normal_exit=$normalExit;cleanup=$cleanup;exit_code=$process.ExitCode;peak_working_set_mb=$peak;validation_ignored=$true;profile_initialized=$true;window_classes=@($classes);executable_sha256=(Get-FileHash -LiteralPath $game[0].FullName -Algorithm SHA256).Hash} |
        ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $run 'shipping-startup.json') -Encoding UTF8
    Write-Host "Shipping startup passed: $run; cleanup: $cleanup"
} finally {
    if(-not $process.HasExited){$process.Kill();$process.WaitForExit()}
    $process.Dispose()
}
