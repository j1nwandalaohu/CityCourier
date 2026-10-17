[CmdletBinding()]
param([Parameter(Mandatory)][string]$CsvPath)
$ErrorActionPreference='Stop'
$path=(Resolve-Path -LiteralPath $CsvPath).Path
$metadata=Get-Content -LiteralPath (Join-Path (Split-Path $path) 'metadata.json') -Raw | ConvertFrom-Json
if($metadata.capture_source -ne 'PackagedGame'){throw 'Only a rendered packaged game capture is accepted.'}
if($metadata.width -ne 1920 -or $metadata.height -ne 1080 -or $metadata.quality -ne 1 -or $metadata.lumen_gi -ne 1 -or $metadata.hardware_raytracing -ne 0){throw 'Capture is not the agreed 1080p medium software-Lumen configuration.'}
$rows=@(Import-Csv -LiteralPath $path | Where-Object Phase -eq 'performance')
if($rows.Count -lt 100){throw 'Missing performance samples.'}
function Number([object]$Value){
    $n=[double]::Parse([string]$Value,[Globalization.CultureInfo]::InvariantCulture)
    if([double]::IsNaN($n) -or [double]::IsInfinity($n)){throw 'Non-finite performance sample.'}
    return $n
}
$times=[Collections.Generic.List[double]]::new()
$gt=0.0;$rt=0.0;$gpu=0.0;$maxMemory=0.0;$maxTexture=0.0
foreach($row in $rows){
    if($row.CaptureSource -ne 'PackagedGame'){throw 'Mixed editor or synthetic-source capture.'}
    $ms=Number $row.FrameMs;if($ms -le 0){throw 'Nonpositive frame time.'};$times.Add($ms)
    $gt+=Number $row.GameThreadMs;$rt+=Number $row.RenderThreadMs;$gpu+=Number $row.GpuMs
    $maxMemory=[math]::Max($maxMemory,(Number $row.WorkingSetMB));$maxTexture=[math]::Max($maxTexture,(Number $row.TextureMemoryMB))
}
$duration=(Number $rows[-1].ElapsedSeconds)-(Number $rows[0].ElapsedSeconds)+$times[0]/1000
if($duration -lt 179){throw 'Performance route is shorter than three minutes.'}
$sum=($times | Measure-Object -Sum).Sum
$slow=@($times | Sort-Object -Descending | Select-Object -First ([math]::Ceiling($times.Count*.01)))
$avg=1000/($sum/$times.Count);$low=1000/(($slow | Measure-Object -Average).Average)
$summary=[pscustomobject]@{
    capture_source='PackagedGame';metadata=$metadata;frames=$times.Count;duration_seconds=$duration
    average_fps=$avg;one_percent_low_fps=$low;meets_target=($avg -ge 30 -and $low -ge 24)
    game_thread_mean_ms=$gt/$times.Count;render_thread_mean_ms=$rt/$times.Count;gpu_mean_ms=$gpu/$times.Count
    process_peak_working_set_mb=$maxMemory;rhi_peak_texture_memory_mb=$maxTexture
    memory_note='Process working set from FPlatformMemory; RHI texture allocations exclude non-texture VRAM. GPU duration from RHI timestamp queries; zero means unavailable.'
}
$summary | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath ([IO.Path]::ChangeExtension($path,'.summary.json')) -Encoding UTF8
$summary
