<#
Assembles the Phase 0 kit into one folder for the run sheet and the guests: probe_host.exe, probe_spawn.exe and
every DLL beside them (SDL3.dll), the Visual C++ runtime DLLs from -RuntimeDir (app-local, because a clean
Windows 11 has no VCRUNTIME140.dll), setup\, pages\, extension\ (src\ and build.py, never dist\), ids.json when it
exists, and the signed XPI when -Xpi names one. Re-running refreshes every file; it deletes nothing.
Windows PowerShell 5.1 compatible. Run it from 64-bit PowerShell: a 32-bit one reads SysWOW64's 32-bit runtime.
#>
param(
    [string]$Kit = (Join-Path $env:USERPROFILE 'cptk-kit'),
    [Parameter(Mandatory = $true)][string]$Build,
    [string]$Xpi,
    [string]$RuntimeDir = (Join-Path $env:SystemRoot 'System32')
)
$ErrorActionPreference = 'Stop'
$source = $PSScriptRoot
$copied = New-Object System.Collections.Generic.List[string]

function Copy-KitFile([string]$From, [string]$Relative) {
    $to = Join-Path $Kit $Relative
    $parent = Split-Path -Parent $to
    if (-not (Test-Path -LiteralPath $parent)) { New-Item -ItemType Directory -Path $parent -Force | Out-Null }
    Copy-Item -LiteralPath $From -Destination $to -Force
    $copied.Add($Relative)
}

function Copy-KitTree([string]$Relative) {
    $root = Join-Path $source $Relative
    if (-not (Test-Path -LiteralPath $root -PathType Container)) { throw "make-kit: missing $root" }
    $prefix = (Resolve-Path -LiteralPath $source).ProviderPath.TrimEnd('\') + '\'
    foreach ($f in Get-ChildItem -LiteralPath $root -Recurse -File) {
        $rel = $f.FullName.Substring($prefix.Length)
        if ($rel -match '(^|\\)__pycache__\\') { continue }
        Copy-KitFile $f.FullName $rel
    }
}

foreach ($exe in 'probe_host.exe', 'probe_spawn.exe') {
    $path = Join-Path $Build $exe
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "make-kit: missing $path (build the kit first)" }
}
New-Item -ItemType Directory -Path $Kit -Force | Out-Null
$Kit = (Resolve-Path -LiteralPath $Kit).ProviderPath

foreach ($exe in 'probe_host.exe', 'probe_spawn.exe') { Copy-KitFile (Join-Path $Build $exe) $exe }
foreach ($dll in Get-ChildItem -LiteralPath $Build -Filter '*.dll' -File) { Copy-KitFile $dll.FullName $dll.Name }
# A runtime DLL the build already placed beside the executables wins over the system's copy.
foreach ($name in 'vcruntime140.dll', 'vcruntime140_1.dll', 'msvcp140.dll') {
    $path = Join-Path $RuntimeDir $name
    if ((Test-Path -LiteralPath $path -PathType Leaf) -and -not ($copied -contains $name)) { Copy-KitFile $path $name }
}
Copy-KitTree 'setup'
Copy-KitTree 'pages'
Copy-KitTree 'extension\src'
Copy-KitFile (Join-Path $source 'extension\build.py') 'extension\build.py'
$ids = Join-Path $source 'ids.json'
if (Test-Path -LiteralPath $ids -PathType Leaf) { Copy-KitFile $ids 'ids.json' } else { Write-Output 'make-kit: no ids.json yet (Task 7 records it)' }
if ($Xpi) { Copy-KitFile $Xpi (Split-Path -Leaf $Xpi) }

Write-Output "kit: $Kit"
foreach ($rel in $copied) { Write-Output "  $rel" }
Write-Output ("make-kit: copied {0} files" -f $copied.Count)
