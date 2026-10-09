#Requires -Version 7.2
<#
.SYNOPSIS
    Run a set of headless passes, each split into shards on containers of their own, and merge
    each pass into one result.

.DESCRIPTION
    A pass (Debian, Fedora, and the leak pass of each) is about 250 checks run one after
    another, mostly waiting on the launcher, so it is split: run.sh <label> [leaks] K/N runs
    shard K of N. This script, for a Windows host with Docker Desktop:
    1. builds the images the passes need (streamflex-test, streamflex-test-fedora);
    2. starts every pass's shards as containers named sfdev-<label>-s<K>, each on a CPU set of
       its own (3 CPUs: 0-2, 3-5, ... 21-23), -m 2g --memory-swap 2g, at most 8 at once, and
       only while at least -MinFreeGB of host RAM is free (it waits up to -MemoryWaitMinutes
       for it, then leaves the shards it could not start unrun, which fails their pass);
    3. writes each shard's console to <OutDir>/<label>-s<K>.console.log as it comes, and keeps
       its files in <OutDir>/<label>-s<K>/;
    4. once a pass's shards have all ended, merges them with merge.py into
       <OutDir>/<label>.merged.log, which says whether they add up to one complete run;
    5. prints one line per pass and one for the set, and exits 0 only when every pass is
       complete with 0 failed.
    The labels are <Prefix> (Debian), <Prefix>-fedora, <Prefix>-leaks and <Prefix>-fedora-leaks.
    Needs Python 3 on PATH for merge.py. Stopping the script stops the containers it started.

.EXAMPLE
    pwsh tests/headless/run-shards.ps1 -Prefix local
    All four passes in 2 shards each: 8 containers.

.EXAMPLE
    pwsh tests/headless/run-shards.ps1 -Prefix try -Legs plain -Shards 2 -OutDir C:/scratch/out
    The Debian pass alone, in 2 shards.
#>
param(
    # The label every pass's label starts with
    [Parameter(Mandatory)] [ValidatePattern('^[A-Za-z0-9][A-Za-z0-9_.-]*$')] [string] $Prefix,
    # How many shards each pass is split into
    [ValidateRange(1, 64)] [int] $Shards = 2,
    # Which passes to run: plain, fedora, leaks, fedora-leaks (pwsh -File gives "a,b" as one
    # string, so commas inside an item split it too)
    [string[]] $Legs = @('plain', 'fedora', 'leaks', 'fedora-leaks'),
    # The repository to mount at /src (by default the one holding this script)
    [string] $Repo = (Join-Path $PSScriptRoot '..' '..'),
    # The folder to mount at /out (by default the repository's headless-out)
    [string] $OutDir,
    # Host RAM that must be free before each container starts
    [int] $MinFreeGB = 8,
    [int] $MemoryWaitMinutes = 15
)
$ErrorActionPreference = 'Stop'
# A native command's exit code is read where it is run, never thrown: merge.py's exit 1 for one
# failing pass, thrown, would end the script and stop the other passes' shards (a profile can
# turn this on for the whole session)
$PSNativeCommandUseErrorActionPreference = $false

$Legs = @($Legs -split ',' | ForEach-Object { $_.Trim() } | Where-Object { $_ } | Select-Object -Unique)
$unknown = @($Legs | Where-Object { $_ -notin 'plain', 'fedora', 'leaks', 'fedora-leaks' })
if ($unknown -or -not $Legs) { throw "unknown pass '$($unknown -join ', ')': give plain, fedora, leaks or fedora-leaks" }
$Repo = (Resolve-Path -LiteralPath $Repo).Path -replace '\\', '/'
if (-not $OutDir) { $OutDir = "$Repo/headless-out" }
New-Item -ItemType Directory -Force $OutDir | Out-Null
$OutDir = (Resolve-Path -LiteralPath $OutDir).Path -replace '\\', '/'
$merge = "$PSScriptRoot/merge.py"
$cpuSets = '0-2', '3-5', '6-8', '9-11', '12-14', '15-17', '18-20', '21-23'

$python = Get-Command python, python3 -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $python) { throw 'merge.py needs Python 3 on PATH (python or python3)' }

# A function to say how much host RAM is free, in GB
function Get-FreeGB { (Get-CimInstance Win32_OperatingSystem).FreePhysicalMemory / 1MB }

# A function to print a line with the time in front
function Say([string] $text) { "[{0:HH:mm:ss}] {1}" -f (Get-Date), $text }

# A function to format a span as minutes and seconds
function Span([TimeSpan] $t) { '{0}:{1:D2}' -f [int][math]::Floor($t.TotalMinutes), $t.Seconds }

# The passes and their shards, in the order they start
$passes = foreach ($leg in $Legs) {
    $label = switch ($leg) {
        'plain' { $Prefix }
        default { "$Prefix-$leg" }
    }
    $pass = [pscustomobject]@{
        Label  = $label
        Image  = if ($leg -like 'fedora*') { 'streamflex-test-fedora' } else { 'streamflex-test' }
        Mode   = if ($leg -like '*leaks') { 'leaks' } else { $null }
        Shards = @()
        Line   = $null
        Ok     = $false
    }
    $pass.Shards = foreach ($k in 1..$Shards) {
        [pscustomobject]@{
            Pass  = $pass
            Label = "$label-s$k"
            K     = $k
            Log   = "$OutDir/$label-s$k.console.log"
            Cpu   = $null
            Job   = $null
            Start = $null
            End   = $null
            Rc    = $null
        }
    }
    $pass
}

# No container this set would start may exist already: docker would refuse its name, and the
# shard would be lost after the others had started
$names = @(docker ps -a --format '{{.Names}}')
if ($LASTEXITCODE -ne 0) { throw 'docker ps failed: is Docker Desktop running?' }
$taken = @($passes.Shards | Where-Object { "sfdev-$($_.Label)" -in $names } | ForEach-Object { "sfdev-$($_.Label)" })
if ($taken) { throw "these containers exist already: $($taken -join ', ')" }

foreach ($image in @($passes.Image | Select-Object -Unique)) {
    $buildArgs = [System.Collections.Generic.List[string]]::new()
    $buildArgs.AddRange([string[]] @('build', '-t', $image))
    if ($image -eq 'streamflex-test-fedora') { $buildArgs.AddRange([string[]] @('-f', "$Repo/tests/headless/Dockerfile.fedora")) }
    $buildArgs.Add("$Repo/tests/headless")
    Say "building $image"
    $buildArray = $buildArgs.ToArray()
    $built = docker @buildArray 2>&1
    if ($LASTEXITCODE -ne 0) { $built | Select-Object -Last 20; throw "docker build of $image failed" }
}

# Each shard runs on a thread of its own, writing its console line by line as docker gives it
$runShard = {
    param($Log, $DockerArgs)
    $PSNativeCommandUseErrorActionPreference = $false
    $writer = [System.IO.StreamWriter]::new($Log, $false, [System.Text.UTF8Encoding]::new($false))
    $writer.AutoFlush = $true
    try {
        $writer.WriteLine("command: docker $($DockerArgs -join ' ')")
        $writer.WriteLine("start $(Get-Date -Format HH:mm:ss)")
        & docker @DockerArgs 2>&1 | ForEach-Object { $writer.WriteLine("$_") }
        $code = $LASTEXITCODE
        $writer.WriteLine("end $(Get-Date -Format HH:mm:ss) rc=$code")
        $code
    } finally {
        $writer.Dispose()
    }
}

# A function to merge a pass whose shards have all ended, and print its line. The pass is good
# only when merge.py said so in words ("complete" and "0 failed") and by its exit code, and every
# shard's container exited 0: a merge that never ran says neither.
function Merge-Pass($pass) {
    $logs = @($pass.Shards.Log)
    $lines = @(& $python.Source $merge --leg $pass.Label @logs 2>&1 | ForEach-Object { "$_" })
    $code = $LASTEXITCODE
    Set-Content -LiteralPath "$OutDir/$($pass.Label).merged.log" -Value $lines -Encoding utf8NoBOM
    $summary = $lines | Where-Object { $_ -like "$($pass.Label): * PASS, *" } | Select-Object -Last 1
    $failed = $lines | Where-Object { $_ -match '^\d+ failed$' } | Select-Object -Last 1
    $exits = @($pass.Shards | Where-Object { $_.Rc -isnot [int] -or $_.Rc -ne 0 })
    $pass.Ok = $code -eq 0 -and $summary -like '*, complete' -and $failed -eq '0 failed' -and -not $exits
    $started = @($pass.Shards | Where-Object Start)
    $times = ($pass.Shards | ForEach-Object {
        if ($_.Start) { "s$($_.K) $(Span ($_.End - $_.Start))" } else { "s$($_.K) not started" }
    }) -join ', '
    $wall = '-'
    if ($started) { $wall = Span ((@($started.End | Sort-Object)[-1]) - (@($started.Start | Sort-Object)[0])) }
    if (-not $summary) { $summary = "$($pass.Label): merge.py gave no result (exit $code)" }
    $pass.Line = "{0}; {1}; wall {2} ({3})" -f $summary, ($failed ?? 'no count'), $wall, $times
    if ($exits) { $pass.Line += '; exits: ' + (($exits | ForEach-Object { "s$($_.K) $($_.Rc)" }) -join ', ') }
    Say $pass.Line
}

$oldEncoding = [Console]::OutputEncoding
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)
$pending = [System.Collections.Generic.Queue[object]]::new([object[]] @($passes.Shards))
$free = [System.Collections.Generic.Queue[string]]::new([string[]] $cpuSets)
$running = [System.Collections.Generic.List[object]]::new()
$waitingSince = $null
$setStart = Get-Date
try {
    while ($pending.Count -or $running.Count) {
        while ($pending.Count -and $free.Count) {
            $gb = Get-FreeGB
            if ($gb -lt $MinFreeGB) {
                if (-not $waitingSince) {
                    $waitingSince = Get-Date
                    Say ('waiting: {0:N1} GB of host RAM free, {1} GB needed' -f $gb, $MinFreeGB)
                } elseif ((Get-Date) - $waitingSince -gt [TimeSpan]::FromMinutes($MemoryWaitMinutes)) {
                    while ($pending.Count) {
                        $s = $pending.Dequeue()
                        $why = 'not started: only {0:N1} GB of host RAM was free after {1} minutes' -f $gb, $MemoryWaitMinutes
                        Set-Content -LiteralPath $s.Log -Value $why -Encoding utf8NoBOM
                        Say "$($s.Label) $why"
                        $s.Rc = 'not started'
                        if (-not @($s.Pass.Shards | Where-Object { $null -eq $_.Rc })) { Merge-Pass $s.Pass }
                    }
                }
                break
            }
            $waitingSince = $null
            $s = $pending.Dequeue()
            $s.Cpu = $free.Dequeue()
            $dockerArgs = [System.Collections.Generic.List[string]]::new()
            $dockerArgs.AddRange([string[]] @('run', '--rm', '--name', "sfdev-$($s.Label)", '-m', '2g', '--memory-swap', '2g',
                                              '--cpuset-cpus', $s.Cpu, '--security-opt', 'seccomp=unconfined',
                                              '-v', "${Repo}:/src:ro", '-v', "${OutDir}:/out", $s.Pass.Image,
                                              'bash', '/src/tests/headless/run.sh', $s.Label))
            if ($s.Pass.Mode) { $dockerArgs.Add($s.Pass.Mode) }
            $dockerArgs.Add("$($s.K)/$Shards")
            $s.Start = Get-Date
            $s.Job = Start-ThreadJob -ThrottleLimit 16 -ScriptBlock $runShard -ArgumentList $s.Log, $dockerArgs.ToArray()
            $running.Add($s)
            Say ('{0} started on CPUs {1} ({2:N1} GB free)' -f $s.Label, $s.Cpu, $gb)
        }
        foreach ($s in @($running)) {
            if ($s.Job.State -notin 'Completed', 'Failed', 'Stopped') { continue }
            $s.End = $s.Job.PSEndTime ?? (Get-Date)
            $s.Rc = @(Receive-Job $s.Job -ErrorAction SilentlyContinue) | Select-Object -Last 1
            if ($null -eq $s.Rc) { $s.Rc = "lost ($($s.Job.State))" }
            Remove-Job $s.Job
            $s.Job = $null
            $free.Enqueue($s.Cpu)
            [void] $running.Remove($s)
            Say ('{0} ended: exit {1}, {2}' -f $s.Label, $s.Rc, (Span ($s.End - $s.Start)))
            if (-not @($s.Pass.Shards | Where-Object { $null -eq $_.Rc })) { Merge-Pass $s.Pass }
        }
        if ($pending.Count -or $running.Count) { Start-Sleep -Seconds 5 }
    }
} finally {
    # Stopped part-way (Ctrl+C): the containers would run on without the script, so stop them
    foreach ($s in $running) {
        docker rm -f "sfdev-$($s.Label)" 2>&1 | Out-Null
        Stop-Job $s.Job -ErrorAction SilentlyContinue
    }
    [Console]::OutputEncoding = $oldEncoding
}

''
foreach ($pass in $passes) { $pass.Line }
$bad = @($passes | Where-Object { -not $_.Ok })
if ($bad) {
    "SET FAILED: $($bad.Label -join ', ') (wall $(Span ((Get-Date) - $setStart)))"
    exit 1
}
"SET PASSED: $($passes.Count) passes in $Shards shards each, every one complete with 0 failed (wall $(Span ((Get-Date) - $setStart)))"
exit 0
