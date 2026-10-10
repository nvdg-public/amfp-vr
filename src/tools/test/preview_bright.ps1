# Writes a small, brightened PNG of a (dark) eye capture so details are visible.
param([string]$In, [string]$Out, [int]$Height = 500, [float]$Gain = 4.0)
Add-Type -AssemblyName System.Drawing
$src = [Drawing.Image]::FromFile($In)
$w = [int]($src.Width * $Height / $src.Height)
$bmp = New-Object Drawing.Bitmap $w, $Height
$g = [Drawing.Graphics]::FromImage($bmp)
$cm = New-Object Drawing.Imaging.ColorMatrix
$cm.Matrix00 = $Gain; $cm.Matrix11 = $Gain; $cm.Matrix22 = $Gain
$ia = New-Object Drawing.Imaging.ImageAttributes; $ia.SetColorMatrix($cm)
$g.DrawImage($src, (New-Object Drawing.Rectangle 0, 0, $w, $Height), 0, 0, $src.Width, $src.Height, [Drawing.GraphicsUnit]::Pixel, $ia)
$bmp.Save($Out, [Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose(); $src.Dispose()
$Out
