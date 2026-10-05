# Run against a disposable Windows installation with the optional service
# installed and stopped. The helper requests UAC approval when necessary.
param([Parameter(Mandatory = $true)][string]$InstallDirectory)

$ErrorActionPreference = 'Stop'
$configuration = Join-Path $env:ProgramData 'XPilot Infinity\server\xpilot-infinity-server.conf'
$helper = Join-Path $InstallDirectory 'xpilot-infinity-service-helper.exe'
$service = Get-Service XPilotInfinityServer
if ($service.Status -ne 'Stopped') { throw 'The test requires a stopped service.' }
$original = [IO.File]::ReadAllText($configuration)
$generation = (Get-FileHash -Algorithm SHA256 $configuration).Hash.ToLowerInvariant()
$map = 'circle2.xp2'
$mapHex = -join ([Text.Encoding]::UTF8.GetBytes($map) | ForEach-Object { $_.ToString('x2') })
$process = Start-Process -FilePath $helper -ArgumentList "--map $generation $mapHex" -PassThru -Wait
if ($process.ExitCode -ne 0) { throw "Map save failed with error $($process.ExitCode)." }
$saved = [IO.File]::ReadAllText($configuration)
if ($saved -notmatch '(?m)^map:\s*.*circle2\.xp2\s*$') { throw 'The selected map was not saved.' }
# Map selection also enables strictMap; unrelated options and comments survive.
if ($saved -notmatch '(?m)^strictMap:\s*true\s*$') { throw 'Strict map loading was not enabled.' }
$otherSettings = '(?m)^(?:map|strictMap)\s*:[^\r\n]*(?:\r?\n|$)'
if (($saved -replace $otherSettings, '') -cne ($original -replace $otherSettings, '')) {
    throw 'Selecting a map changed unrelated configuration.'
}
if ((Get-Service XPilotInfinityServer).Status -ne 'Stopped') { throw 'Saving started the service.' }
Write-Output 'PASS: installed service configuration can be saved without starting the service.'
