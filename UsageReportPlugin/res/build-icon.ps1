# Build multi-size .ico for the usage report plugin.
# Pure GDI+ rendering (no browser needed): each size is drawn with supersampling,
# then downscaled with high quality bicubic resampling.
param(
    [string]$OutIcon = (Join-Path $PSScriptRoot "usage-report.ico"),
    [string]$PreviewDir = ""
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$sizes = @(16, 20, 24, 32, 40, 48, 64, 128, 256)

# design canvas is 256x256; all geometry below uses that coordinate space
$Teal = [System.Drawing.Color]::FromArgb(255, 0x0F, 0x76, 0x6E)
$ScreenFill = [System.Drawing.Color]::FromArgb(26, 0x0F, 0x76, 0x6E)

function New-RoundedPath([single]$x, [single]$y, [single]$w, [single]$h, [single]$r) {
    $p = New-Object System.Drawing.Drawing2D.GraphicsPath
    $d = $r * 2
    $p.AddArc($x, $y, $d, $d, 180, 90)
    $p.AddArc($x + $w - $d, $y, $d, $d, 270, 90)
    $p.AddArc($x + $w - $d, $y + $h - $d, $d, $d, 0, 90)
    $p.AddArc($x, $y + $h - $d, $d, $d, 90, 90)
    $p.CloseFigure()
    return $p
}

function New-IconBitmap([int]$size, [int]$ss) {
    $canvas = $size * $ss
    $k = $canvas / 256.0
    # tiny sizes need bolder geometry, otherwise everything turns into mush
    $tiny = $size -le 20

    $bmp = New-Object System.Drawing.Bitmap($canvas, $canvas, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $g.Clear([System.Drawing.Color]::Transparent)

    $frameW = if ($tiny) { 22 } else { 16 }
    $dialW = if ($tiny) { 18 } else { 13 }

    if (-not $tiny) {
        # screen background
        $screen = New-RoundedPath (30 * $k) (42 * $k) (196 * $k) (122 * $k) (12 * $k)
        $brush = New-Object System.Drawing.SolidBrush($ScreenFill)
        $g.FillPath($brush, $screen)
        $brush.Dispose()
        $screen.Dispose()
    }

    # monitor frame
    $frame = New-RoundedPath (16 * $k) (28 * $k) (224 * $k) (148 * $k) (20 * $k)
    $pen = New-Object System.Drawing.Pen($Teal, ($frameW * $k))
    $pen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
    $g.DrawPath($pen, $frame)
    $frame.Dispose()

    # clock dial
    $dialR = if ($tiny) { 46 } else { 40 }
    $dialCy = if ($tiny) { 101 } else { 103 }
    if ($tiny) {
        # solid dial with knocked out hands reads far better at 16-20 px
        $dialPath = New-Object System.Drawing.Drawing2D.GraphicsPath
        $dialPath.AddEllipse(((128 - $dialR) * $k), (($dialCy - $dialR) * $k), ((2 * $dialR) * $k), ((2 * $dialR) * $k))

        $handPath = New-Object System.Drawing.Drawing2D.GraphicsPath
        $handPath.AddLine((128 * $k), ($dialCy * $k), (128 * $k), (($dialCy - 24) * $k))
        $handPath.AddLine((128 * $k), ($dialCy * $k), (151 * $k), (($dialCy + 13) * $k))

        $widenPen = New-Object System.Drawing.Pen($Teal, (15 * $k))
        $widenPen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
        $widenPen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
        $handPath.Widen($widenPen)
        $widenPen.Dispose()

        $region = New-Object System.Drawing.Region($dialPath)
        $region.Exclude($handPath)
        $dialBrush = New-Object System.Drawing.SolidBrush($Teal)
        $g.FillRegion($dialBrush, $region)
        $dialBrush.Dispose()
        $region.Dispose()
        $handPath.Dispose()
        $dialPath.Dispose()
    } else {
        $clockPen = New-Object System.Drawing.Pen($Teal, ($dialW * $k))
        $clockPen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
        $clockPen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
        $g.DrawEllipse($clockPen, ((128 - $dialR) * $k), (($dialCy - $dialR) * $k), ((2 * $dialR) * $k), ((2 * $dialR) * $k))
        $g.DrawLine($clockPen, (128 * $k), ($dialCy * $k), (128 * $k), (($dialCy - 21) * $k))
        $g.DrawLine($clockPen, (128 * $k), ($dialCy * $k), (150 * $k), (($dialCy + 12) * $k))
        $clockPen.Dispose()
    }
    $pen.Dispose()

    # stand
    $solid = New-Object System.Drawing.SolidBrush($Teal)
    $stemW = if ($tiny) { 34 } else { 24 }
    $g.FillRectangle($solid, ((128 - $stemW / 2) * $k), (174 * $k), ($stemW * $k), (18 * $k))
    $base = New-RoundedPath (74 * $k) (192 * $k) (108 * $k) (18 * $k) (9 * $k)
    $g.FillPath($solid, $base)
    $base.Dispose()
    $solid.Dispose()
    $g.Dispose()

    # downscale to the target size
    $out = New-Object System.Drawing.Bitmap($size, $size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $og = [System.Drawing.Graphics]::FromImage($out)
    $og.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $og.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $og.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
    $og.DrawImage($bmp, (New-Object System.Drawing.Rectangle(0, 0, $size, $size)))
    $og.Dispose()
    $bmp.Dispose()
    return $out
}

function Get-DibBytes([System.Drawing.Bitmap]$bmp) {
    $size = $bmp.Width
    $rect = New-Object System.Drawing.Rectangle(0, 0, $size, $size)
    $data = $bmp.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly,
        [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $stride = [Math]::Abs($data.Stride)
    $pixels = New-Object byte[] ($stride * $size)
    [System.Runtime.InteropServices.Marshal]::Copy($data.Scan0, $pixels, 0, $pixels.Length)
    $bmp.UnlockBits($data)

    $maskRow = [Math]::Floor(($size + 31) / 32) * 4
    $maskSize = $maskRow * $size
    $ms = New-Object System.IO.MemoryStream
    $bw = New-Object System.IO.BinaryWriter($ms)
    $bw.Write([int]40)
    $bw.Write([int]$size)
    $bw.Write([int]($size * 2))
    $bw.Write([int16]1)
    $bw.Write([int16]32)
    $bw.Write([int]0)
    $bw.Write([int]($size * $size * 4))
    $bw.Write([int]0); $bw.Write([int]0); $bw.Write([int]0); $bw.Write([int]0)
    for ($y = $size - 1; $y -ge 0; $y--) {
        $bw.Write($pixels, ($y * $stride), ($size * 4))
    }
    $bw.Write((New-Object byte[] $maskSize))
    $bw.Flush()
    # returning a byte[] through the pipeline would split it into single bytes,
    # so hand it over through a script scoped variable instead
    $script:LastDib = $ms.ToArray()
}

$entries = @()
$pngDir = Join-Path ([System.IO.Path]::GetTempPath()) ("usage-icon-" + [Guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Force -Path $pngDir | Out-Null
foreach ($s in $sizes) {
    $ss = if ($s -le 48) { 16 } else { 4 }
    $bmp = New-IconBitmap $s $ss
    if ($PreviewDir -ne "") {
        New-Item -ItemType Directory -Force -Path $PreviewDir | Out-Null
        $bmp.Save((Join-Path $PreviewDir ("icon-$s.png")), [System.Drawing.Imaging.ImageFormat]::Png)
    }
    # large sizes are stored as PNG inside the ico to keep the file small
    $pngPath = Join-Path $pngDir ("icon-$s.png")
    $bmp.Save($pngPath, [System.Drawing.Imaging.ImageFormat]::Png)
    if ($s -ge 128) {
        $bytes = [System.IO.File]::ReadAllBytes($pngPath)
        $isPng = $true
        $bmp.Dispose()
    } else {
        Get-DibBytes $bmp
        $bytes = $script:LastDib
        $isPng = $false
        $bmp.Dispose()
    }
    $entries += [pscustomobject]@{ Size = $s; Bytes = $bytes; IsPng = $isPng }
}
Remove-Item $pngDir -Recurse -Force -ErrorAction SilentlyContinue

$out = New-Object System.IO.MemoryStream
$bw = New-Object System.IO.BinaryWriter($out)
$bw.Write([int16]0)
$bw.Write([int16]1)
$bw.Write([int16]$entries.Count)

$offset = 6 + 16 * $entries.Count
foreach ($e in $entries) {
    $dim = if ($e.Size -ge 256) { 0 } else { $e.Size }
    $bw.Write([byte]$dim)
    $bw.Write([byte]$dim)
    $bw.Write([byte]0)
    $bw.Write([byte]0)
    $bw.Write([int16]1)
    $bw.Write([int16]32)
    $bw.Write([int]$e.Bytes.Length)
    $bw.Write([int]$offset)
    $offset += $e.Bytes.Length
}
foreach ($e in $entries) { $bw.Write($e.Bytes) }
$bw.Flush()
[System.IO.File]::WriteAllBytes($OutIcon, $out.ToArray())
$bw.Dispose()
$out.Dispose()

Write-Output ("icon written: " + $OutIcon + " (" + (Get-Item $OutIcon).Length + " bytes)")
