$ErrorActionPreference='Stop'
$root=Join-Path $PSScriptRoot ('../Saved/Tests/Performance-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $root -Force | Out-Null
$csv=Join-Path $root 'frames.csv'
$metadata=@{capture_source='PackagedGame';width=1920;height=1080;quality=1;lumen_gi=1;lumen_reflections=1;hardware_raytracing=0}
$metadata | ConvertTo-Json | Set-Content (Join-Path $root 'metadata.json') -Encoding UTF8
$elapsed=0.0
$rows=for($i=0;$i -lt 20000;$i++){
    $ms=if($i -lt 200){100}else{10};$elapsed+=$ms/1000.0
    [pscustomobject]@{CaptureSource='PackagedGame';ElapsedSeconds=$elapsed;FrameMs=$ms;GameThreadMs=2;RenderThreadMs=3;GpuMs=4;WorkingSetMB=1000;TextureMemoryMB=200;Phase='performance'}
}
$rows | Export-Csv $csv -NoTypeInformation
$measure=Join-Path $PSScriptRoot '../scripts/Measure-Performance.ps1'
$result=& $measure -CsvPath $csv
if([math]::Abs($result.average_fps-(1000/10.9)) -gt .001){throw 'Mean FPS must use mean frame time'}
if($result.one_percent_low_fps -ne 10){throw '1% low must average slowest 1% of frame times'}
if($result.meets_target){throw 'A poor 1% low must reject the target'}
$metadata.capture_source='EditorGame';$metadata | ConvertTo-Json | Set-Content (Join-Path $root 'metadata.json')
$rejected=$false;try{& $measure -CsvPath $csv | Out-Null}catch{$rejected=$true}
if(-not $rejected){throw 'Editor evidence was accepted'}
$metadata.capture_source='PackagedGame';$metadata | ConvertTo-Json | Set-Content (Join-Path $root 'metadata.json')
$rows[0].FrameMs='NaN';$rows | Export-Csv $csv -NoTypeInformation
$rejected=$false;try{& $measure -CsvPath $csv | Out-Null}catch{$rejected=$true}
if(-not $rejected){throw 'Invalid frame time was accepted'}
Write-Host 'PASS: mean frame time, slowest 1%, target rejection, editor rejection, invalid sample rejection'
