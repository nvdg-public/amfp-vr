# Compares an eye rendered with the symmetric (enclosing, cropped) projection against the same eye rendered
# with the exact asymmetric projection: crops the symmetric image to the eye's real frustum, scales it to
# the asymmetric image size, and reports the mean/max per-channel difference. Also writes a side-by-side
# PNG (symmetric-cropped | asymmetric | difference x4).
param(
    [string]$Sym, [string]$Asym, [string]$Out,
    [double]$UMin, [double]$UMax, [double]$VMin = 0, [double]$VMax = 1   # crop bounds, image space (v=0 top)
)
Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing @"
using System; using System.Drawing; using System.Drawing.Imaging; using System.Runtime.InteropServices;
public static class ImgDiff {
  public static double[] Diff(Bitmap a, Bitmap b, Bitmap outDiff) {
    var ra = new Rectangle(0,0,a.Width,a.Height);
    var da = a.LockBits(ra, ImageLockMode.ReadOnly, PixelFormat.Format24bppRgb);
    var db = b.LockBits(ra, ImageLockMode.ReadOnly, PixelFormat.Format24bppRgb);
    var dd = outDiff.LockBits(ra, ImageLockMode.WriteOnly, PixelFormat.Format24bppRgb);
    int n = da.Stride * a.Height; byte[] pa = new byte[n], pb = new byte[n], pd = new byte[n];
    Marshal.Copy(da.Scan0, pa, 0, n); Marshal.Copy(db.Scan0, pb, 0, n);
    double sum = 0; int max = 0; long count = 0; long big = 0;
    for (int y = 0; y < a.Height; y++) for (int x = 0; x < a.Width * 3; x++) {
      int i = y * da.Stride + x; int d = Math.Abs(pa[i] - pb[i]);
      sum += d; if (d > max) max = d; if (d > 32) big++; count++; pd[i] = (byte)Math.Min(255, d * 4);
    }
    Marshal.Copy(pd, 0, dd.Scan0, n);
    a.UnlockBits(da); b.UnlockBits(db); outDiff.UnlockBits(dd);
    return new double[] { sum / count, max, 100.0 * big / count };
  }
}
"@
$s = [Drawing.Image]::FromFile($Sym); $a = [Drawing.Bitmap]::FromFile($Asym)
$src = New-Object Drawing.RectangleF ([float]($UMin * $s.Width)), ([float]($VMin * $s.Height)), ([float](($UMax - $UMin) * $s.Width)), ([float](($VMax - $VMin) * $s.Height))
$crop = New-Object Drawing.Bitmap $a.Width, $a.Height, ([Drawing.Imaging.PixelFormat]::Format24bppRgb)
$g = [Drawing.Graphics]::FromImage($crop); $g.InterpolationMode = 'HighQualityBilinear'; $g.PixelOffsetMode = 'Half'
$g.DrawImage($s, (New-Object Drawing.RectangleF 0, 0, $a.Width, $a.Height), $src, [Drawing.GraphicsUnit]::Pixel); $g.Dispose()
$a24 = $a.Clone((New-Object Drawing.Rectangle 0, 0, $a.Width, $a.Height), [Drawing.Imaging.PixelFormat]::Format24bppRgb)
$diff = New-Object Drawing.Bitmap $a.Width, $a.Height, ([Drawing.Imaging.PixelFormat]::Format24bppRgb)
$r = [ImgDiff]::Diff($crop, $a24, $diff)
"mean abs diff {0:N2}/255, max {1}, pixels-channels > 32: {2:N2}%" -f $r[0], $r[1], $r[2]

$h = 600; $w = [int]($a.Width * $h / $a.Height)
$sbs = New-Object Drawing.Bitmap ($w * 3 + 20), $h; $g = [Drawing.Graphics]::FromImage($sbs); $g.Clear([Drawing.Color]::Magenta)
$g.DrawImage($crop, 0, 0, $w, $h); $g.DrawImage($a24, $w + 10, 0, $w, $h); $g.DrawImage($diff, 2 * $w + 20, 0, $w, $h); $g.Dispose()
$sbs.Save($Out, [Drawing.Imaging.ImageFormat]::Png)
$s.Dispose(); $a.Dispose(); $crop.Dispose(); $a24.Dispose(); $diff.Dispose(); $sbs.Dispose()
