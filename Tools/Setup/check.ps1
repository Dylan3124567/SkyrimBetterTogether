param([string]$OutputDirectory = 'A:\SkyrimMultiplayerBuild\Setup', [string]$Package, [switch]$Friend, [switch]$Modded, [switch]$Update, [string]$KeyFile = 'A:\SkyrimMultiplayerBuild\signing\update-key.xml')
$ErrorActionPreference = 'Stop'
& "$PSScriptRoot\build.ps1" -OutputDirectory $OutputDirectory -Package $Package -Friend:$Friend -Modded:$Modded -Update:$Update
$name = if ($Update) { "SkyrimBetterTogetherModdedUpdate.exe" } elseif ($Modded) { "SkyrimBetterTogetherModdedFriendSetup.exe" } elseif ($Friend) { "SkyrimBetterTogetherFriendSetup.exe" } else { "SkyrimBetterTogetherSetup.exe" }
& "$env:WINDIR\Microsoft.NET\Framework64\v4.0.30319\csc.exe" /nologo /target:exe /platform:x64 /r:System.IO.Compression.dll /r:System.IO.Compression.FileSystem.dll /r:System.Web.Extensions.dll "/r:$OutputDirectory\$name" "/out:$OutputDirectory\SetupCheck.exe" "$PSScriptRoot\Check.cs"
if ($LASTEXITCODE -ne 0) { throw 'Check compilation failed' }
$fixture = Join-Path $OutputDirectory ('check-' + [guid]::NewGuid().ToString('N'))
# An update only installs over a full install, which the fixture is not
if ($Package -and -not $Update) { & "$OutputDirectory\SetupCheck.exe" $fixture $Package } else { & "$OutputDirectory\SetupCheck.exe" $fixture }
if ($LASTEXITCODE -ne 0) { throw "Setup check failed; evidence preserved at $fixture" }
# The checks above load setup as a library, which works for files Windows refuses to start. Start it for real, with no visible window.
$render = Join-Path $fixture 'render.png'
$started = Start-Process "$OutputDirectory\$name" -ArgumentList '--render-test', "`"$render`"" -PassThru -Wait
if ($started.ExitCode -ne 0 -or -not (Test-Path $render)) { throw 'Setup does not start' }
Write-Output "PASS setup starts as a program"
# The updater against a stand-in GitHub feed. Its fixture packages are signed with the release key, read where it is.
# IdleUpdater.exe is the same updater with running programs simulated, for the chain the checks run as real processes.
& "$env:WINDIR\Microsoft.NET\Framework64\v4.0.30319\csc.exe" /nologo /define:IDLE_UPDATER /target:winexe /platform:x64 /main:IdleUpdater /r:System.Windows.Forms.dll /r:System.Drawing.dll /r:System.IO.Compression.dll /r:System.IO.Compression.FileSystem.dll /r:System.Web.Extensions.dll "/out:$OutputDirectory\IdleUpdater.exe" "$PSScriptRoot\Setup.cs" "$PSScriptRoot\Updater.cs" "$PSScriptRoot\UpdaterCheck.cs"
if ($LASTEXITCODE -ne 0) { throw 'Idle updater compilation failed' }
& "$env:WINDIR\Microsoft.NET\Framework64\v4.0.30319\csc.exe" /nologo /target:exe /platform:x64 /r:System.IO.Compression.dll /r:System.IO.Compression.FileSystem.dll /r:System.Web.Extensions.dll "/r:$OutputDirectory\BetterTogetherUpdater.exe" "/out:$OutputDirectory\UpdaterCheck.exe" "$PSScriptRoot\UpdaterCheck.cs"
if ($LASTEXITCODE -ne 0) { throw 'Updater check compilation failed' }
& "$OutputDirectory\UpdaterCheck.exe" (Join-Path $fixture 'updater') "$OutputDirectory\BetterTogetherUpdater.exe" $KeyFile "$OutputDirectory\IdleUpdater.exe"
if ($LASTEXITCODE -ne 0) { throw "Updater check failed; evidence preserved at $fixture\updater" }

