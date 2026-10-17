# Behavioral tests: a broken runner that accepts missing, empty or failed
# automation reports must fail these tests. No Unreal installation is faked.
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot '../scripts/Courier.Tools.psm1') -Force
$testRoot = Join-Path $PSScriptRoot ('../Saved/Tests/Tooling-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testRoot -Force | Out-Null
$script:Passed = 0
$script:Failed = 0

function Check-Case([string]$Name, [scriptblock]$Action) {
    try { & $Action; $script:Passed++; Write-Host "PASS $Name" }
    catch { $script:Failed++; Write-Host "FAIL $Name : $($_.Exception.Message)" }
}
function Expect-Rejection([scriptblock]$Action) {
    $rejected = $false
    try { & $Action | Out-Null } catch { $rejected = $true }
    if (-not $rejected) { throw 'Invalid automation evidence was accepted.' }
}
function Save-Report([string]$Name, [object[]]$Rows, [int]$Failed=0, [int]$NotRun=0) {
    $path = Join-Path $testRoot ($Name + '.json')
    @{ tests=$Rows; succeeded=@($Rows | Where-Object { $_.state -eq 'Success' }).Count;
       succeededWithWarnings=0; failed=$Failed; notRun=$NotRun; inProcess=0 } |
        ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $path -Encoding UTF8
    return $path
}

Check-Case 'Missing report is a failure' {
    Expect-Rejection { Assert-CourierAutomationReport -ReportPath (Join-Path $testRoot 'missing.json') -Filter 'CityCourier.Smoke' }
}
Check-Case 'Zero executed tests cannot pass' {
    $path = Save-Report 'empty' @()
    Expect-Rejection { Assert-CourierAutomationReport -ReportPath $path -Filter 'CityCourier.Smoke' }
}
Check-Case 'Failed test cannot pass even if counters claim success' {
    $path = Save-Report 'failed' @(@{fullTestPath='CityCourier.Smoke.StartupMap';state='Fail'})
    Expect-Rejection { Assert-CourierAutomationReport -ReportPath $path -Filter 'CityCourier.Smoke' }
}
Check-Case 'Not-run test cannot pass' {
    $path = Save-Report 'notrun' @(@{fullTestPath='CityCourier.Smoke.StartupMap';state='NotRun'}) 0 1
    Expect-Rejection { Assert-CourierAutomationReport -ReportPath $path -Filter 'CityCourier.Smoke' }
}
Check-Case 'Unrelated tests are not evidence for requested filter' {
    $path = Save-Report 'unrelated' @(@{fullTestPath='Engine.Smoke';state='Success'})
    Expect-Rejection { Assert-CourierAutomationReport -ReportPath $path -Filter 'CityCourier.Smoke' }
}
Check-Case 'Prefix collision is not a match' {
    $path = Save-Report 'collision' @(@{fullTestPath='CityCourier.SmokeOther';state='Success'})
    Expect-Rejection { Assert-CourierAutomationReport -ReportPath $path -Filter 'CityCourier.Smoke' }
}
Check-Case 'Global report failures cannot be ignored' {
    $path = Save-Report 'globalfailed' @(@{fullTestPath='CityCourier.Smoke.StartupMap';state='Success'}) 1
    Expect-Rejection { Assert-CourierAutomationReport -ReportPath $path -Filter 'CityCourier.Smoke' }
}
Check-Case 'Malformed JSON is a failure' {
    $path = Join-Path $testRoot 'broken.json'
    Set-Content -LiteralPath $path -Value '{ broken' -Encoding UTF8
    Expect-Rejection { Assert-CourierAutomationReport -ReportPath $path -Filter 'CityCourier.Smoke' }
}
Check-Case 'Successful matching tests return actual count' {
    $path = Save-Report 'success' @(@{fullTestPath='CityCourier.Smoke.StartupMap';state='Success'},@{fullTestPath='CityCourier.Smoke.PlayerStart';state='Success'})
    $result = Assert-CourierAutomationReport -ReportPath $path -Filter 'CityCourier.Smoke'
    if ($result.Passed -ne 2) { throw 'Expected 2 verified successful tests.' }
}
Check-Case 'Successful child process retains exit code zero' {
    $logs = Join-Path $testRoot 'process-zero'
    New-Item -ItemType Directory -Path $logs | Out-Null
    Invoke-CourierExecutable -FilePath (Join-Path $PSHOME 'powershell.exe') -Arguments @('-NoProfile','-Command','exit 0') -LogDirectory $logs -TimeoutSeconds 10
}
Check-Case 'Failed child reports its actual nonzero exit code' {
    $logs = Join-Path $testRoot 'process-seven'
    New-Item -ItemType Directory -Path $logs | Out-Null
    $message = ''
    try { Invoke-CourierExecutable -FilePath (Join-Path $PSHOME 'powershell.exe') -Arguments @('-NoProfile','-Command','exit 7') -LogDirectory $logs -TimeoutSeconds 10 }
    catch { $message = $_.Exception.Message }
    if ($message -notmatch 'exit code 7\.') { throw "Actual exit code was lost: $message" }
}
Check-Case 'Process arguments preserve spaces quotes and trailing backslash' {
    $logs = Join-Path $testRoot 'process-arguments'
    New-Item -ItemType Directory -Path $logs | Out-Null
    $value = 'two words "quoted" C:\tail\'
    Invoke-CourierExecutable -FilePath (Join-Path $PSHOME 'powershell.exe') -Arguments @('-NoProfile','-File',(Join-Path $PSScriptRoot 'fixtures/EchoArgument.ps1'),'-Value',$value) -LogDirectory $logs -TimeoutSeconds 10
    if ((Get-Content -Raw -LiteralPath (Join-Path $logs 'stdout.log')) -cne $value) { throw 'Argument content changed.' }
}
Check-Case 'Timed-out child is stopped and cannot report success' {
    $logs = Join-Path $testRoot 'process-timeout'
    New-Item -ItemType Directory -Path $logs | Out-Null
    $message = ''
    try { Invoke-CourierExecutable -FilePath (Join-Path $PSHOME 'powershell.exe') -Arguments @('-NoProfile','-Command','Start-Sleep -Seconds 10') -LogDirectory $logs -TimeoutSeconds 1 }
    catch { $message = $_.Exception.Message }
    if ($message -notmatch 'exceeded 1 seconds') { throw "Expected a timeout failure: $message" }
}
Write-Host "Tooling: $script:Passed passed, $script:Failed failed. Fixtures: $testRoot"
if ($script:Failed -gt 0) { exit 1 }
