Amnesia: A Machine for Pigs VR
==============================

A virtual reality version of Amnesia: A Machine for Pigs for SteamVR (tested with a Valve Index and an Oculus Quest 2):
stereo 3D, head tracking, room-scale, and motion controls with hands.

This folder contains no game files. You need your own copy of the game.

DISCLAIMER: Unofficial fan-made VR mod. It is not affiliated with, endorsed by or supported by Frictional
Games or The Chinese Room. "Amnesia" and "A Machine for Pigs" are names of their respective owners; they are
used here only to say which game this mod is for. You need your own copy of the game.


Requirements
------------
- Windows 10 or 11
- SteamVR and a headset
- Amnesia: A Machine for Pigs installed (tested with the Epic Games and Steam versions; other versions are likely to work)
- The Visual C++ 2005 x86 runtime, which the game itself also needs (normally already installed with the game)


Installing
----------
Easiest: double-click install.bat. It finds your Epic or Steam copy of the game (or asks where it is), copies
these files into a VR folder inside the game's folder, prepares everything and puts an "Amnesia AMFP VR"
shortcut on your desktop. Then start SteamVR and the shortcut. Running it again keeps your vr_settings.cfg.

Without the installer: put this folder anywhere, for example inside the game's folder (then it finds the game by
itself), and start aamfp_vr.exe. If the game is not found, you are asked once for the folder where the game is installed
(the folder that contains aamfp.exe and resources.cfg). The answer is saved in vr_package.cfg; empty the
GameFolder line there to be asked again.

On the first start, the game's engine DLLs are copied from your game folder into this folder, and 98 character
animations are converted from your own game files into data_overlay (a few seconds). Always start the game with
aamfp_vr.exe (not aamfp_vr_game.exe). The game folder is only read, never changed. Saves and settings are shared with the normal game
(Documents\Amnesia\Pig).

Start with "aamfp_vr.exe -flat" to play without VR.


Settings
--------
vr_settings.cfg (open with any text editor) explains every VR option: render resolution, menu panel,
turning, comfort options, hands, lantern and more. Controller buttons can be changed in
SteamVR > Settings > Controllers > Manage controller bindings.

Recenter: click the right stick (or press F9).


Uninstalling
------------
Delete this folder (the VR folder inside the game's folder) and the desktop shortcut.


Licence
-------
Full licence texts: LICENSE.txt (GPLv3) and THIRD_PARTY_NOTICES.txt.
Based on the source code Frictional Games released under the GNU General Public License v3
(github.com/FrictionalGames/AmnesiaAMachineForPigs). This VR version is GPLv3 too; its source code must be
available to anyone who receives this package.
Hand models (vr_assets\hands): Godot XR Tools hands by Miodrag Sejic, CC0 (see License.md there).
