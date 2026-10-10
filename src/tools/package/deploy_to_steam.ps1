# Steam variant of deploy_to_epic.ps1: finds Amnesia: A Machine for Pigs in the Steam libraries (app 239200)
# and puts the package in <game>\VR the same way (plan.md Phase 6b). Pass -Game to skip the search.
param([string]$Game = '')
$ErrorActionPreference = 'Stop'

if (-not $Game) {
    $steam = $null
    foreach ($key in 'HKCU:\Software\Valve\Steam', 'HKLM:\SOFTWARE\WOW6432Node\Valve\Steam', 'HKLM:\SOFTWARE\Valve\Steam') {
        $p = Get-ItemProperty $key -ErrorAction SilentlyContinue
        if ($p.SteamPath) { $steam = $p.SteamPath; break }
        if ($p.InstallPath) { $steam = $p.InstallPath; break }
    }
    if (-not $steam) { throw 'Steam not found. Pass the game folder with -Game "<folder>"' }

    # Every Steam library is listed in libraryfolders.vdf as "path" "<folder>"
    $libs = @($steam)
    $vdf = Join-Path $steam 'steamapps\libraryfolders.vdf'
    if (Test-Path $vdf) {
        foreach ($m in [regex]::Matches((Get-Content $vdf -Raw), '"path"\s+"([^"]+)"')) { $libs += $m.Groups[1].Value -replace '\\\\', '\' }
    }
    foreach ($lib in ($libs | Select-Object -Unique)) {
        $acf = Join-Path $lib 'steamapps\appmanifest_239200.acf'
        if (-not (Test-Path $acf)) { continue }
        $dir = [regex]::Match((Get-Content $acf -Raw), '"installdir"\s+"([^"]+)"').Groups[1].Value
        $cand = Join-Path $lib "steamapps\common\$dir"
        if (Test-Path (Join-Path $cand 'resources.cfg')) { $Game = $cand; break }
    }
    if (-not $Game) { throw "Amnesia: A Machine for Pigs not found in the Steam libraries ($($libs -join ', ')). Pass -Game `"<folder>`"" }
}

"Steam game folder: $Game"
& (Join-Path $PSScriptRoot 'deploy_to_epic.ps1') -Game $Game
