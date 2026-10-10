# Amnesia: A Machine for Pigs VR - installer (started by install.bat in the package folder).
# Finds the game (Epic or Steam, or asks), copies this package into <game>\VR (keeps an existing vr_settings.cfg),
# copies the game's engine DLLs next to the VR exe, converts the animations the game ships only as FBX, and makes
# a desktop shortcut. Never changes the game's own files. Uninstall: delete <game>\VR and the shortcut.
param([string]$Game = '', [switch]$NoPause)
$ErrorActionPreference = 'Stop'
$pkg = $PSScriptRoot
$title = 'Amnesia: A Machine for Pigs VR'

function Done([string]$msg, [int]$code) {
    Write-Host ''
    Write-Host $msg
    if (-not $NoPause) { Write-Host ''; Read-Host 'Press Enter to close' | Out-Null }
    exit $code
}

function Test-GameDir([string]$dir) {
    $dir -and (Test-Path (Join-Path $dir 'resources.cfg')) -and (Test-Path (Join-Path $dir 'maps') -PathType Container)
}

Write-Host $title
Write-Host ('=' * $title.Length)

try {
    # ---- Find the game ----
    $found = @()
    if ($Game) { $found += $Game }
    elseif (Test-GameDir (Split-Path $pkg)) { $found += (Split-Path $pkg) }   # package already inside the game folder
    else {
        # Epic: the launcher's install manifests
        $manifests = Join-Path $env:ProgramData 'Epic\EpicGamesLauncher\Data\Manifests'
        if (Test-Path $manifests) {
            foreach ($item in Get-ChildItem $manifests -Filter *.item) {
                try { $m = Get-Content $item.FullName -Raw | ConvertFrom-Json } catch { continue }
                if ((Test-GameDir $m.InstallLocation) -and (Test-Path (Join-Path $m.InstallLocation 'aamfp.exe'))) { $found += $m.InstallLocation }
            }
        }
        # Steam: app 239200 in any Steam library
        $steam = $null
        foreach ($key in 'HKCU:\Software\Valve\Steam', 'HKLM:\SOFTWARE\WOW6432Node\Valve\Steam', 'HKLM:\SOFTWARE\Valve\Steam') {
            $p = Get-ItemProperty $key -ErrorAction SilentlyContinue
            if ($p.SteamPath) { $steam = $p.SteamPath; break }
            if ($p.InstallPath) { $steam = $p.InstallPath; break }
        }
        if ($steam) {
            $libs = @($steam)
            $vdf = Join-Path $steam 'steamapps\libraryfolders.vdf'
            if (Test-Path $vdf) {
                foreach ($m in [regex]::Matches((Get-Content $vdf -Raw), '"path"\s+"([^"]+)"')) { $libs += $m.Groups[1].Value -replace '\\\\', '\' }
            }
            foreach ($lib in $libs) {
                $acf = Join-Path $lib 'steamapps\appmanifest_239200.acf'
                if (-not (Test-Path $acf)) { continue }
                $dir = [regex]::Match((Get-Content $acf -Raw), '"installdir"\s+"([^"]+)"').Groups[1].Value
                $found += Join-Path $lib "steamapps\common\$dir"
            }
        }
        $found = @($found | Where-Object { Test-GameDir $_ } | ForEach-Object { (Resolve-Path $_).Path } | Select-Object -Unique)
    }

    if ($found.Count -gt 1) {
        Write-Host 'The game was found in more than one place:'
        for ($i = 0; $i -lt $found.Count; $i++) { Write-Host "  $($i + 1). $($found[$i])" }
        $pick = Read-Host "Install into which one? (1-$($found.Count))"
        $Game = $found[[int]$pick - 1]
    } elseif ($found.Count -eq 1) {
        $Game = $found[0]
    } else {
        Write-Host 'The game was not found automatically. Pick the folder where it is installed (the folder with aamfp.exe).'
        Add-Type -AssemblyName System.Windows.Forms
        $dlg = New-Object System.Windows.Forms.FolderBrowserDialog
        $dlg.Description = 'Where is Amnesia: A Machine for Pigs installed? (the folder with aamfp.exe)'
        if ($dlg.ShowDialog() -ne 'OK') { Done 'Cancelled, nothing was installed.' 1 }
        $Game = $dlg.SelectedPath
    }
    if (-not (Test-GameDir $Game)) { Done "This is not the game folder (resources.cfg or maps is missing):`n  $Game" 1 }
    $Game = (Resolve-Path $Game).Path
    Write-Host "Game folder: $Game"

    $dest = Join-Path $Game 'VR'

    # ---- Program Files etc. need administrator rights: ask once and continue there ----
    try {
        New-Item -ItemType Directory -Force $dest | Out-Null
        $probe = Join-Path $dest '.write_test'
        Set-Content $probe 'x'; Remove-Item $probe
    } catch {
        Write-Host 'The game folder is protected; asking Windows for administrator rights...'
        $args2 = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', "`"$PSCommandPath`"", '-Game', "`"$Game`"")
        $p = Start-Process powershell -Verb RunAs -ArgumentList $args2 -Wait -PassThru
        exit $p.ExitCode
    }

    # ---- Copy the package ----
    if ((Resolve-Path $pkg).Path -ne (Resolve-Path $dest).Path) {
        $keep = $null
        if (Test-Path (Join-Path $dest 'vr_settings.cfg')) {
            $keep = Join-Path $env:TEMP 'amfp_vr_settings_keep.cfg'
            Copy-Item (Join-Path $dest 'vr_settings.cfg') $keep -Force
        }
        Get-ChildItem $dest -Force | Where-Object { $_.Name -ne 'data_overlay' } | ForEach-Object { Remove-Item $_.FullName -Recurse -Force }
        Get-ChildItem $pkg -Force | Where-Object { $_.Name -notin 'vr_package.cfg', 'data_overlay' } | Copy-Item -Destination $dest -Recurse -Force
        Set-Content (Join-Path $dest 'vr_package.cfg') "# Amnesia: A Machine for Pigs VR package. The game is the parent folder.`r`nGameFolder=" -Encoding ascii
        if ($keep) { Move-Item $keep (Join-Path $dest 'vr_settings.cfg') -Force; Write-Host 'Kept your existing vr_settings.cfg.' }
        Write-Host "Copied the VR files to $dest"
    } else {
        Write-Host "The VR files are already in $dest"
    }

    # ---- Engine DLLs from the game itself ----
    foreach ($d in 'SDL2','glew32','DevIL','Newton','OpenAL32','wrap_oal','alut','libogg','libvorbis','libvorbisfile','libtheora','zlibwapi') {
        $src = Join-Path $Game "$d.dll"
        if (-not (Test-Path $src)) { Done "The game folder is missing $d.dll. Check or repair the game installation." 1 }
        Copy-Item $src $dest -Force
    }
    Write-Host 'Copied the game engine DLLs.'

    # ---- Animations the game ships only as FBX ----
    $overlay = Join-Path $dest 'data_overlay'
    $jobs = foreach ($rel in (Get-Content (Join-Path $dest 'vr_animations.txt') | Where-Object { $_ -and -not $_.StartsWith('#') })) {
        $in = Join-Path $Game $rel
        $outFile = Join-Path $overlay ([IO.Path]::ChangeExtension($rel, '.anm'))
        if ((Test-Path $in) -and -not (Test-Path $outFile)) {
            New-Item -ItemType Directory -Force (Split-Path $outFile) | Out-Null
            "$in`t$outFile"
        }
    }
    if ($jobs) {
        $list = Join-Path $env:TEMP 'amfp_vr_animations.txt'
        Set-Content $list $jobs -Encoding Default
        $result = & (Join-Path $dest 'fbx2anm.exe') --list $list | Select-Object -Last 1
        Remove-Item $list
        Write-Host "Animations: $result"
    } else { Write-Host 'Animations: all present.' }

    # ---- Desktop shortcut ----
    $lnk = Join-Path ([Environment]::GetFolderPath('Desktop')) 'Amnesia AMFP VR.lnk'
    $sc = (New-Object -ComObject WScript.Shell).CreateShortcut($lnk)
    $sc.TargetPath = Join-Path $dest 'aamfp_vr.exe'
    $sc.WorkingDirectory = $dest
    $sc.IconLocation = (Join-Path $Game 'aamfp.exe') + ',0'
    $sc.Save()
    Write-Host "Desktop shortcut: $lnk"

    Done "Installed. Start SteamVR, then the 'Amnesia AMFP VR' shortcut. To uninstall, delete $dest and the shortcut." 0
} catch {
    Done "Installation failed: $($_.Exception.Message)" 1
}
