# Small helper to drive the game window for testing: screenshots, mouse and keyboard via SendInput.
# Usage: . .\gamectl.ps1 ; Shot "name" ; Click x y ; Key 0x57 500 ; Look dx dy
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class GameCtl {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
  [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
  [StructLayout(LayoutKind.Sequential)] public struct MOUSEINPUT { public int dx, dy; public uint data, flags, time; public IntPtr extra; }
  [StructLayout(LayoutKind.Sequential)] public struct KEYBDINPUT { public ushort vk, scan; public uint flags, time; public IntPtr extra; }
  [StructLayout(LayoutKind.Explicit)] public struct INPUT { [FieldOffset(0)] public uint type; [FieldOffset(8)] public MOUSEINPUT mi; [FieldOffset(8)] public KEYBDINPUT ki; }
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);
  [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr h);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern uint SendInput(uint n, INPUT[] i, int size);
  [DllImport("user32.dll")] public static extern uint MapVirtualKey(uint code, uint type);
  public static void Mouse(uint flags, int dx, int dy) { var i = new INPUT[1]; i[0].type = 0; i[0].mi.flags = flags; i[0].mi.dx = dx; i[0].mi.dy = dy; SendInput(1, i, Marshal.SizeOf(typeof(INPUT))); }
  public static void Key(ushort vk, bool up) { var i = new INPUT[1]; i[0].type = 1; i[0].ki.scan = (ushort)MapVirtualKey(vk, 0); i[0].ki.flags = 0x0008u | (up ? 0x0002u : 0u); SendInput(1, i, Marshal.SizeOf(typeof(INPUT))); }
}
"@
$Global:ShotDir = Join-Path $PSScriptRoot "..\screens"
function Win { $p = Get-Process aamfp -ErrorAction Stop | ? { $_.Path -like ((Resolve-Path "$PSScriptRoot\..\..").Path + "\*") } | select -First 1; $p.MainWindowHandle }
function Origin { $h = Win; $pt = New-Object GameCtl+POINT; [GameCtl]::ClientToScreen($h, [ref]$pt) | Out-Null; $r = New-Object GameCtl+RECT; [GameCtl]::GetClientRect($h, [ref]$r) | Out-Null; @{ X = $pt.X; Y = $pt.Y; W = $r.R; H = $r.B } }
function IsFg { [GameCtl]::GetForegroundWindow() -eq (Win) }
function Focus { $h = Win; [GameCtl]::ShowWindow($h, 9) | Out-Null; [GameCtl]::BringWindowToTop($h) | Out-Null; [GameCtl]::SetForegroundWindow($h) | Out-Null; Start-Sleep -Milliseconds 400; if (-not (IsFg)) { throw "Game window is not in the foreground; refusing to send input" } }
function Shot($name) { if (-not (IsFg)) { throw "Game not in foreground; screenshot would capture another window" }; $o = Origin; $bmp = New-Object Drawing.Bitmap $o.W, $o.H; $g = [Drawing.Graphics]::FromImage($bmp); $g.CopyFromScreen($o.X, $o.Y, 0, 0, $bmp.Size); $f = Join-Path $ShotDir "$name.png"; $bmp.Save($f, [Drawing.Imaging.ImageFormat]::Png); $g.Dispose(); $bmp.Dispose(); $f }
function Click($x, $y) { Focus; $o = Origin; [GameCtl]::SetCursorPos($o.X + $x, $o.Y + $y) | Out-Null; Start-Sleep -Milliseconds 150; [GameCtl]::Mouse(0x2, 0, 0); Start-Sleep -Milliseconds 80; [GameCtl]::Mouse(0x4, 0, 0); Start-Sleep -Milliseconds 300 }
function Move($x, $y) { $o = Origin; [GameCtl]::SetCursorPos($o.X + $x, $o.Y + $y) | Out-Null }
function Key($vk, $ms = 80) { if (-not (IsFg)) { throw "Game not in foreground" }; [GameCtl]::Key([uint16]$vk, $false); $end = (Get-Date).AddMilliseconds($ms); try { while ((Get-Date) -lt $end) { if (-not (IsFg)) { throw "Lost focus during key hold" }; Start-Sleep -Milliseconds 30 } } finally { [GameCtl]::Key([uint16]$vk, $true) }; Start-Sleep -Milliseconds 100 }
function Look($dx, $dy, $steps = 10) { if (-not (IsFg)) { throw "Game not in foreground" }; for ($i = 0; $i -lt $steps; $i++) { [GameCtl]::Mouse(0x1, [int]($dx / $steps), [int]($dy / $steps)); Start-Sleep -Milliseconds 15 } }
function LeftDown { if (-not (IsFg)) { throw "Game not in foreground" }; [GameCtl]::Mouse(0x2, 0, 0) }
function LeftUp { [GameCtl]::Mouse(0x4, 0, 0) }
