param([Parameter(Mandatory=$true)][string]$Exe, [Parameter(Mandatory=$true)][string]$Fixture, [Parameter(Mandatory=$true)][string]$Prefix)
$ErrorActionPreference = 'Stop'
$report = Join-Path $env:RUNNER_TEMP "$Prefix-video.txt"
$normalReport = Join-Path $env:RUNNER_TEMP "$Prefix-normal.txt"
Remove-Item $report,$normalReport -ErrorAction SilentlyContinue
# Exercise the built executable with no global VLC installation and a real video.
$process = Start-Process $Exe -ArgumentList "--self-test `"$Fixture`" `"$report`"" -PassThru
if (-not $process.WaitForExit(45000)) { $process.Kill(); throw 'Native video self-test timed out' }
if (-not (Test-Path $report)) { throw "Native EXE produced no report; process exit $($process.ExitCode)" }
$result = Get-Content $report -Raw
Write-Output $result
if ($process.ExitCode -ne 0 -or $result -notmatch '^PASS') { throw "Native video self-test failed: $result" }
# Test the ordinary initialization order and event loop, including automatic update startup.
$process = Start-Process $Exe -ArgumentList "--normal-start-test `"$normalReport`"" -PassThru
if (-not $process.WaitForExit(45000)) { $process.Kill(); throw 'Normal launch test timed out' }
if (-not (Test-Path $normalReport)) { throw "Normal launch produced no report; process exit $($process.ExitCode)" }
$result = Get-Content $normalReport -Raw
Write-Output $result
if ($process.ExitCode -ne 0 -or $result -notmatch '^PASS') { throw "Normal launch failed: $result" }
