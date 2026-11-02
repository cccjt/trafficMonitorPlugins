param(
    [Parameter(Mandatory = $true)][string]$ReportDir
)

$ErrorActionPreference = 'Stop'

function Add-Event($list, $time, $kind, $src) {
    [void]$list.Add([pscustomobject]@{ t = $time; kind = $kind; src = $src })
}

$events = New-Object System.Collections.ArrayList

Get-WinEvent -FilterHashtable @{LogName='System'; ProviderName='Microsoft-Windows-Kernel-Power'; Id=42} -ErrorAction SilentlyContinue | ForEach-Object {
    Add-Event $events $_.TimeCreated 'off' 'sleep42'
}
Get-WinEvent -FilterHashtable @{LogName='System'; ProviderName='Microsoft-Windows-Kernel-General'; Id=13} -ErrorAction SilentlyContinue | ForEach-Object {
    Add-Event $events $_.TimeCreated 'off' 'shutdown13'
}
Get-WinEvent -FilterHashtable @{LogName='System'; ProviderName='Microsoft-Windows-Kernel-General'; Id=12} -ErrorAction SilentlyContinue | ForEach-Object {
    Add-Event $events $_.TimeCreated 'on' 'boot12'
}
Get-WinEvent -FilterHashtable @{LogName='System'; ProviderName='Microsoft-Windows-Kernel-Power'; Id=131} -ErrorAction SilentlyContinue | ForEach-Object {
    Add-Event $events $_.TimeCreated 'on' 'resume131'
}
Get-WinEvent -FilterHashtable @{LogName='Microsoft-Windows-Winlogon/Operational'; Id=811} -ErrorAction SilentlyContinue | ForEach-Object {
    $x = [xml]$_.ToXml()
    $code = ($x.Event.EventData.Data | Where-Object { $_.Name -eq 'Event' }).'#text'
    if ($code -eq '4') { Add-Event $events $_.TimeCreated 'lock' 'lock4' }
    elseif ($code -eq '5') { Add-Event $events $_.TimeCreated 'unlock' 'unlock5' }
}

$sorted = @($events | Sort-Object t)
if ($sorted.Count -eq 0) { throw 'no power or session events found' }

$now = Get-Date

# ---- online intervals (boot/resume -> sleep/shutdown) ----
$online = New-Object System.Collections.ArrayList
$onSince = $null
foreach ($e in $sorted) {
    if ($e.kind -eq 'on') {
        if ($null -eq $onSince) { $onSince = $e.t }
    }
    elseif ($e.kind -eq 'off') {
        if ($null -ne $onSince -and $e.t -gt $onSince) {
            [void]$online.Add([pscustomobject]@{ start = $onSince; end = $e.t })
            $onSince = $null
        }
    }
}
if ($null -ne $onSince) { [void]$online.Add([pscustomobject]@{ start = $onSince; end = $now }) }

# ---- locked intervals ----
# Auto-logon at boot: Winlogon logs no unlock event, so pair lock/unlock only.
$locked = New-Object System.Collections.ArrayList
$lockSince = $null
foreach ($e in $sorted) {
    if ($e.kind -eq 'lock') {
        if ($null -eq $lockSince) { $lockSince = $e.t }
    }
    elseif ($e.kind -eq 'unlock') {
        if ($null -ne $lockSince -and $e.t -gt $lockSince) {
            [void]$locked.Add([pscustomobject]@{ start = $lockSince; end = $e.t })
            $lockSince = $null
        }
    }
}
if ($null -ne $lockSince) { [void]$locked.Add([pscustomobject]@{ start = $lockSince; end = $now }) }

# ---- display-off intervals recorded by the plugin (real screen state) ----
$displayOff = New-Object System.Collections.ArrayList
$displayLog = Join-Path $ReportDir 'display-events.csv'
if (Test-Path $displayLog) {
    $offSince = $null
    foreach ($line in (Get-Content $displayLog -ErrorAction SilentlyContinue)) {
        if ([string]::IsNullOrWhiteSpace($line)) { continue }
        $parts = $line.Split(',')
        if ($parts.Length -lt 2) { continue }
        if ($parts[0] -eq 'time') { continue }
        $t = [datetime]::ParseExact($parts[0].Trim(), 'yyyy-MM-dd HH:mm:ss', $null)
        $state = $parts[1].Trim()
        if ($state -eq 'off') {
            if ($null -eq $offSince) { $offSince = $t }
        }
        elseif ($state -eq 'on') {
            if ($null -ne $offSince -and $t -gt $offSince) {
                [void]$displayOff.Add([pscustomobject]@{ start = $offSince; end = $t })
                $offSince = $null
            }
        }
    }
    if ($null -ne $offSince) {
        [void]$displayOff.Add([pscustomobject]@{ start = $offSince; end = $now })
    }
}

# ---- usage = online minus (locked or screen-off) ----
# Boundary strategy:
#   end   : display-off if recorded, else lock, else sleep, else shutdown
#   start : unlock if recorded, else wake, else display-on, else boot
# cuts are the union of all non-usage signals, which equals earliest end / latest start.
$usage = New-Object System.Collections.ArrayList
foreach ($o in $online) {
    $cuts = @(@($locked) + @($displayOff) | Where-Object { $_.end -gt $o.start -and $_.start -lt $o.end } | Sort-Object start)
    $cursor = $o.start
    foreach ($c in $cuts) {
        $cs = if ($c.start -lt $o.start) { $o.start } else { $c.start }
        $ce = if ($c.end -gt $o.end) { $o.end } else { $c.end }
        if ($cs -gt $cursor) { [void]$usage.Add([pscustomobject]@{ start = $cursor; end = $cs }) }
        if ($ce -gt $cursor) { $cursor = $ce }
    }
    if ($cursor -lt $o.end) { [void]$usage.Add([pscustomobject]@{ start = $cursor; end = $o.end }) }
}
$usage = @($usage | Where-Object { ($_.end - $_.start).TotalSeconds -ge 60 } | Sort-Object start)

# ---- sleep intervals ----
$sleepIv = New-Object System.Collections.ArrayList
$sleepSince = $null
foreach ($e in $sorted) {
    if ($e.src -eq 'sleep42') {
        if ($null -eq $sleepSince) { $sleepSince = $e.t }
    }
    elseif ($e.src -eq 'resume131' -or $e.src -eq 'boot12') {
        if ($null -ne $sleepSince -and $e.t -gt $sleepSince) {
            [void]$sleepIv.Add([pscustomobject]@{ start = $sleepSince; end = $e.t })
            $sleepSince = $null
        }
    }
}

# ---- per-day usage spans (start / end only) ----
$spanRows = @{}
foreach ($u in $usage) {
    $dk = $u.start.ToString('yyyy-MM-dd')
    if (-not $spanRows.ContainsKey($dk)) { $spanRows[$dk] = New-Object System.Collections.ArrayList }
    $ek = $u.end.ToString('yyyy-MM-dd')
    if (-not $spanRows.ContainsKey($ek)) { $spanRows[$ek] = New-Object System.Collections.ArrayList }
    [void]$spanRows[$dk].Add([pscustomobject]@{ d = $dk; s = $u.start.ToString('HH:mm:ss'); e = $(if ($u.end.Date -eq $u.start.Date) { $u.end.ToString('HH:mm:ss') } else { 'NEXT' }) })
    if ($u.end.Date -ne $u.start.Date) {
        [void]$spanRows[$ek].Add([pscustomobject]@{ d = $ek; s = 'PREV'; e = $u.end.ToString('HH:mm:ss') })
    }
}

# ---- analysis range: later of the first power event and the first lock event ----
$minPower = ($sorted | Where-Object { $_.kind -eq 'on' -or $_.kind -eq 'off' } | Select-Object -First 1).t
$minLock = ($sorted | Where-Object { $_.kind -eq 'lock' -or $_.kind -eq 'unlock' } | Select-Object -First 1).t
if ($minPower -and $minLock) {
    $from = if ($minPower -gt $minLock) { $minPower.Date } else { $minLock.Date }
}
elseif ($minPower) { $from = $minPower.Date }
elseif ($minLock) { $from = $minLock.Date }
else { $from = $now.Date.AddDays(-7) }

$today = $now.Date
$dayList = New-Object System.Collections.ArrayList

for ($d = $from; $d -le $today; $d = $d.AddDays(1)) {
    $ds = $d
    $de = $d.AddDays(1)
    $onMin = 0.0; $useMin = 0.0; $maxUse = 0.0; $useSess = 0
    $firstUse = $null; $lastUse = $null
    $firstStartAt = $null; $firstEndAt = $null

    foreach ($o in $online) {
        if ($o.end -lt $ds -or $o.start -gt $de) { continue }
        $a = if ($o.start -lt $ds) { $ds } else { $o.start }
        $b = if ($o.end -gt $de) { $de } else { $o.end }
        if ($b -gt $a) { $onMin += ($b - $a).TotalMinutes }
    }
    foreach ($u in $usage) {
        if ($u.end -lt $ds -or $u.start -gt $de) { continue }
        $a = if ($u.start -lt $ds) { $ds } else { $u.start }
        $b = if ($u.end -gt $de) { $de } else { $u.end }
        $m = ($b - $a).TotalMinutes
        if ($m -le 0) { continue }
        $useMin += $m; $useSess++
        if ($m -gt $maxUse) { $maxUse = $m }
        # earliest session that starts inside this day, end may fall on next day
        if ($u.start -ge $ds -and $u.start -lt $de) {
            if ($null -eq $firstStartAt -or $u.start -lt $firstStartAt) {
                $firstStartAt = $u.start
                $firstEndAt = $u.end
            }
        }
        if ($u.start -ge $ds -and $u.start -lt $de) { if ($null -eq $firstUse -or $u.start -lt $firstUse) { $firstUse = $u.start } }
        if ($u.end -ge $ds -and $u.end -le $de) { if ($null -eq $lastUse -or $u.end -gt $lastUse) { $lastUse = $u.end } }
        if ($u.start -lt $ds) { $firstUse = $ds }
        if ($u.end -gt $de) { $lastUse = $de }
    }

    $lockCount = @($locked | Where-Object { $_.start -ge $ds -and $_.start -lt $de }).Count
    # only count lock time that overlaps online time (ignore locks while asleep/off)
    $lockMin = 0.0
    foreach ($s in $locked) {
        if ($s.end -lt $ds -or $s.start -gt $de) { continue }
        foreach ($o in $online) {
            $a = if ($s.start -lt $o.start) { $o.start } else { $s.start }
            if ($a -lt $ds) { $a = $ds }
            $b = if ($s.end -gt $o.end) { $o.end } else { $s.end }
            if ($b -gt $de) { $b = $de }
            if ($b -gt $a) { $lockMin += ($b - $a).TotalMinutes }
        }
    }
    $showOffMin = $onMin - $useMin - $lockMin
    if ($showOffMin -lt 0) { $showOffMin = 0 }

    $sleepMin = 0.0
    foreach ($s in $sleepIv) {
        if ($s.end -lt $ds -or $s.start -gt $de) { continue }
        $a = if ($s.start -lt $ds) { $ds } else { $s.start }
        $b = if ($s.end -gt $de) { $de } else { $s.end }
        if ($b -gt $a) { $sleepMin += ($b - $a).TotalMinutes }
    }

    $firstLabel = ''
    $lastLabel = ''
    if ($firstUse) { if ($firstUse -eq $ds) { $firstLabel = 'PREV' } else { $firstLabel = $firstUse.ToString('HH:mm') } }
    if ($lastUse) { if ($lastUse -eq $de) { $lastLabel = 'NEXT' } else { $lastLabel = $lastUse.ToString('HH:mm') } }

    $startMin = $null; $endMin = $null; $endNext = $false
    $startAt = ''; $endAt = ''
    if ($null -ne $firstStartAt) {
        $startMin = [int][math]::Round(($firstStartAt - $ds).TotalMinutes)
        $startAt = $firstStartAt.ToString('HH:mm:ss')
        $endAt = $firstEndAt.ToString('HH:mm:ss')
        if ($firstEndAt -ge $de) {
            $endNext = $true
            $endMin = [int][math]::Round((($firstEndAt - $de).TotalMinutes) % 1440)
        }
        else {
            $endMin = [int][math]::Round(($firstEndAt - $ds).TotalMinutes)
        }
    }

    $daySpans = $spanRows[$ds.ToString('yyyy-MM-dd')]
    if ($null -eq $daySpans) { $daySpans = @() }
    [void]$dayList.Add([pscustomobject]@{
            d       = $ds.ToString('yyyy-MM-dd')
            dow     = [int]$ds.DayOfWeek
            online  = [int][math]::Round($onMin)
            minutes = [int][math]::Round($useMin)
            locked  = [int][math]::Round($lockMin)
            off     = [int][math]::Round($showOffMin)
            sleep   = [int][math]::Round($sleepMin)
            maxSess = [int][math]::Round($maxUse)
            startMin = $(if ($null -ne $startMin) { [int]$startMin } else { $null })
            endMin   = $(if ($null -ne $endMin) { [int]$endMin } else { $null })
            endNext  = $endNext
            startAt  = $startAt
            endAt    = $endAt
            locks   = $lockCount
            firstOn = $firstLabel
            lastOff = $lastLabel
            spans   = @($daySpans)
        })
}

$days = @($dayList)
if ($days.Count -eq 0) { throw 'no days generated' }

$vals = @($days | ForEach-Object { $_.minutes })
$sortedVals = @($vals | Sort-Object)
$sum = ($vals | Measure-Object -Sum).Sum
$onlineSum = ($days | Measure-Object online -Sum).Sum
$lockedSum = ($days | Measure-Object locked -Sum).Sum

$longest = $usage | Sort-Object { ($_.end - $_.start) } -Descending | Select-Object -First 1

$summary = [pscustomobject]@{
    from           = $from.ToString('yyyy-MM-dd')
    to             = $today.ToString('yyyy-MM-dd')
    days           = $days.Count
    totalHours     = [math]::Round($sum / 60, 1)
    onlineHours    = [math]::Round($onlineSum / 60, 1)
    lockedHours    = [math]::Round($lockedSum / 60, 1)
    avgMinutes     = [int][math]::Round($sum / $days.Count)
    medianMinutes  = [int][math]::Round($sortedVals[[int]($sortedVals.Count / 2)])
    maxMinutes     = ($vals | Measure-Object -Maximum).Maximum
    minMinutes     = ($vals | Measure-Object -Minimum).Minimum
    maxDate        = ($days | Sort-Object minutes -Descending | Select-Object -First 1).d
    minDate        = ($days | Sort-Object minutes | Select-Object -First 1).d
    longestStart   = $(if ($longest) { $longest.start.ToString('yyyy-MM-dd HH:mm') } else { '' })
    longestEnd     = $(if ($longest) { $longest.end.ToString('yyyy-MM-dd HH:mm') } else { '' })
    longestMinutes = $(if ($longest) { [int][math]::Round(($longest.end - $longest.start).TotalMinutes) } else { 0 })
    locks          = $locked.Count
    zeroDays       = @($days | Where-Object { $_.minutes -eq 0 }).Count
    generatedAt    = $now.ToString('yyyy-MM-dd HH:mm')
}

$out = [pscustomobject]@{
    summary = $summary
    days    = $days
}

$json = $out | ConvertTo-Json -Depth 6 -Compress
$assets = Join-Path $ReportDir 'assets'
if (!(Test-Path $assets)) { New-Item -ItemType Directory -Path $assets -Force | Out-Null }
$dataPath = Join-Path $assets 'data.js'
[System.IO.File]::WriteAllText($dataPath, 'window.REPORT_DATA = ' + $json + ';', (New-Object System.Text.UTF8Encoding($false)))

# ---- usage event log: use = unlocked, idle = locked / asleep / off ----
# One line per state change; the plugin will append real display data later.
$sb = New-Object System.Text.StringBuilder
[void]$sb.AppendLine('time,state')
foreach ($u in $usage) {
    if ($u.end -lt $from) { continue }
    $startAt = if ($u.start -lt $from) { $from } else { $u.start }
    [void]$sb.AppendLine(('{0},use' -f $startAt.ToString('yyyy-MM-dd HH:mm:ss')))
    if ($u.end -lt $now) {
        [void]$sb.AppendLine(('{0},idle' -f $u.end.ToString('yyyy-MM-dd HH:mm:ss')))
    }
}
$csvPath = Join-Path $ReportDir 'usage-events.csv'
[System.IO.File]::WriteAllText($csvPath, $sb.ToString(), (New-Object System.Text.UTF8Encoding($false)))
