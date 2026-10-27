<#
Prepares a Firefox profile for the probe on Windows: CPTK's user.js lines and the sideloaded XPI.
Run with Firefox closed. Windows PowerShell 5.1 compatible. The Linux twin is firefox_profile.py.
#>
param(
    [Parameter(Mandatory = $true)][string]$ProfileDir,
    [Parameter(Mandatory = $true)][string]$Xpi,
    [switch]$NoScanScopes
)
$ErrorActionPreference = 'Stop'
$mark = '// cptk-overlay'
New-Item -ItemType Directory -Path $ProfileDir -Force | Out-Null
$userJs = Join-Path $ProfileDir 'user.js'
$kept = @()
if (Test-Path -LiteralPath $userJs) {
    # -Encoding UTF8: Windows PowerShell 5.1 would read the file as ANSI and change a non-ASCII line on write.
    $kept = @(Get-Content -LiteralPath $userJs -Encoding UTF8 | Where-Object { -not $_.TrimEnd().EndsWith($mark) })
}
$ours = @(
    "user_pref(""extensions.autoDisableScopes"", 0); $mark"
)
# -NoScanScopes writes 0, Firefox's default, rather than leaving the line out: Firefox keeps a user.js value in
# prefs.js, so a profile that once ran with 1 would otherwise keep rescanning.
$scan = if ($NoScanScopes) { 0 } else { 1 }
$ours += "user_pref(""extensions.startupScanScopes"", $scan); $mark"
$ours += "user_pref(""media.autoplay.default"", 0); $mark"
$utf8 = New-Object System.Text.UTF8Encoding $false
[IO.File]::WriteAllText($userJs, (($kept + $ours) -join "`n") + "`n", $utf8)
$extensions = Join-Path $ProfileDir 'extensions'
New-Item -ItemType Directory -Path $extensions -Force | Out-Null
$dest = Join-Path $extensions 'overlay-probe@cptk.app.xpi'
# Compare the bytes, as firefox_profile.py does. Not Get-FileHash: 5.1 defines it in a script module, and a
# PSModulePath that lists PowerShell 7's modules first hides it.
$new = [IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $Xpi).ProviderPath)
$same = (Test-Path -LiteralPath $dest) -and
    ([Convert]::ToBase64String([IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $dest).ProviderPath)) -eq [Convert]::ToBase64String($new))
if ($same) { Write-Output 'unchanged' } else { Copy-Item -LiteralPath $Xpi -Destination $dest -Force; Write-Output 'copied' }
