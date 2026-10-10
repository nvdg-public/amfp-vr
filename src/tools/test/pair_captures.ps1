# Puts the left and right eye captures (eye0/eye1_frame<n>.bmp from HPL_VR_CAPTURE_*) side by side,
# scaled down, as one PNG per frame for quick viewing.
param([string]$Dir = "$PSScriptRoot\..\..\..\phase2\captures", [int]$Height = 700)
Add-Type -AssemblyName System.Drawing
Get-ChildItem $Dir -Filter "eye0_frame*.bmp" | ForEach-Object {
    $frame = $_.BaseName -replace 'eye0_', ''
    $right = Join-Path $Dir "eye1_$frame.bmp"
    if (-not (Test-Path $right)) { return }
    $l = [Drawing.Image]::FromFile($_.FullName); $r = [Drawing.Image]::FromFile($right)
    $w = [int]($l.Width * $Height / $l.Height)
    $out = New-Object Drawing.Bitmap ($w * 2 + 10), $Height
    $g = [Drawing.Graphics]::FromImage($out)
    $g.Clear([Drawing.Color]::Magenta)
    $g.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBilinear
    $g.DrawImage($l, 0, 0, $w, $Height); $g.DrawImage($r, $w + 10, 0, $w, $Height)
    $png = Join-Path $Dir "pair_$frame.png"
    $out.Save($png, [Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $out.Dispose(); $l.Dispose(); $r.Dispose()
    $png
}
