Set-StrictMode -Version Latest

function Assert-CourierAutomationReport {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string]$ReportPath, [Parameter(Mandatory)][string]$Filter)
    if (-not (Test-Path -LiteralPath $ReportPath -PathType Leaf)) { throw "Automation report was not produced: $ReportPath" }
    $report = Get-Content -Raw -Encoding UTF8 -LiteralPath $ReportPath | ConvertFrom-Json -ErrorAction Stop
    foreach ($counter in @('failed','notRun','inProcess')) {
        if ($null -eq $report.PSObject.Properties[$counter] -or $report.$counter -ne 0) {
            throw "Automation report has missing or nonzero $counter count."
        }
    }
    if ($null -eq $report.PSObject.Properties['tests'] -or @($report.tests).Count -eq 0) {
        throw 'Automation report contains no executed tests.'
    }
    $matched = 0
    foreach ($test in $report.tests) {
        if ($test.state -ne 'Success') { throw "Automation test did not succeed: $($test.fullTestPath) ($($test.state))" }
        $name = [string]$test.fullTestPath
        if ($name.Equals($Filter, [StringComparison]::OrdinalIgnoreCase) -or
            $name.StartsWith($Filter + '.', [StringComparison]::OrdinalIgnoreCase)) { $matched++ }
    }
    if ($matched -eq 0) { throw "No executed tests matched '$Filter'." }
    return [pscustomobject]@{ Passed = $matched; ReportPath = $ReportPath }
}

function Resolve-CourierEngine {
    [CmdletBinding()]
    param([string]$EngineRoot)
    if (-not $EngineRoot) { $EngineRoot = $env:CITYCOURIER_UE_ROOT }
    if (-not $EngineRoot) {
        $manifestPath = Join-Path $env:ProgramData 'Epic/UnrealEngineLauncher/LauncherInstalled.dat'
        if (Test-Path -LiteralPath $manifestPath) {
            $manifest = Get-Content -Raw -LiteralPath $manifestPath | ConvertFrom-Json
            $installed = @($manifest.InstallationList | Where-Object { $_.AppName -eq 'UE_5.6' })
            if ($installed.Count -gt 0) { $EngineRoot = $installed[0].InstallLocation }
        }
    }
    if (-not $EngineRoot) {
        throw 'UE 5.6 was not found. Install it through Epic Games Launcher, then pass -EngineRoot or set CITYCOURIER_UE_ROOT for this process.'
    }
    $resolved = (Resolve-Path -LiteralPath $EngineRoot -ErrorAction Stop).Path
    $versionPath = Join-Path $resolved 'Engine/Build/Build.version'
    $version = Get-Content -Raw -LiteralPath $versionPath -ErrorAction Stop | ConvertFrom-Json
    if ($version.MajorVersion -ne 5 -or $version.MinorVersion -ne 6) {
        throw "This project requires UE 5.6; found $($version.MajorVersion).$($version.MinorVersion) at $resolved"
    }
    foreach ($relativePath in @('Engine/Binaries/Win64/UnrealEditor-Cmd.exe','Engine/Build/BatchFiles/Build.bat','Engine/Build/BatchFiles/RunUAT.bat')) {
        if (-not (Test-Path -LiteralPath (Join-Path $resolved $relativePath) -PathType Leaf)) {
            throw "Incomplete Unreal installation: missing $relativePath"
        }
    }
    return $resolved
}

function New-CourierRunDirectory {
    param([Parameter(Mandatory)][string]$Name)
    $root = Split-Path -Parent $PSScriptRoot
    $suffix = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0,8)
    $path = Join-Path $root "Saved/Validation/$Name-$suffix"
    New-Item -ItemType Directory -Path $path -Force | Out-Null
    return $path
}

function ConvertTo-CourierProcessArgument {
    param([AllowEmptyString()][string]$Value)
    # Windows CommandLineToArgvW / C-runtime escaping. Never passed to a shell.
    $builder = New-Object System.Text.StringBuilder
    [void]$builder.Append('"')
    $slashes = 0
    foreach ($character in $Value.ToCharArray()) {
        if ($character -eq '\') { $slashes++; continue }
        if ($character -eq '"') {
            [void]$builder.Append('\', (2 * $slashes + 1))
        } else { [void]$builder.Append('\', $slashes) }
        [void]$builder.Append($character)
        $slashes = 0
    }
    [void]$builder.Append('\', (2 * $slashes))
    [void]$builder.Append('"')
    return $builder.ToString()
}

function Invoke-CourierExecutable {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$FilePath,
        [string[]]$Arguments = @(),
        [Parameter(Mandatory)][string]$LogDirectory,
        [ValidateRange(1,86400)][int]$TimeoutSeconds = 900
    )
    $quoted = @($Arguments | ForEach-Object { ConvertTo-CourierProcessArgument $_ }) -join ' '
    $stdout = Join-Path $LogDirectory 'stdout.log'
    $stderr = Join-Path $LogDirectory 'stderr.log'
    $process = Start-Process -FilePath $FilePath -ArgumentList $quoted -WindowStyle Hidden -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    try {
        # Windows PowerShell 5.1 may otherwise discard the native process handle
        # before ExitCode is read, yielding null even for a successful process.
        $null = $process.Handle
        if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
            $process.Kill()
            $process.WaitForExit()
            throw "Process exceeded $TimeoutSeconds seconds. Logs: $LogDirectory"
        }
        $process.WaitForExit()
        $exitCode = $process.ExitCode
        Get-Content -LiteralPath $stdout -Tail 20 -ErrorAction SilentlyContinue | ForEach-Object { Write-Host $_ }
        Get-Content -LiteralPath $stderr -Tail 10 -ErrorAction SilentlyContinue | ForEach-Object { Write-Host $_ }
        if ($exitCode -ne 0) { throw "Process failed with exit code $exitCode. Logs: $LogDirectory" }
    } finally { $process.Dispose() }
}

Export-ModuleMember -Function Assert-CourierAutomationReport,Resolve-CourierEngine,New-CourierRunDirectory,Invoke-CourierExecutable
