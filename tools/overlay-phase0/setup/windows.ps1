<#
Phase 0 setup on Windows: force-install policy for Chrome and Edge, the Edge hygiene policies, and the
native-messaging host registration for all three browsers. Policy goes under -Scope (HKCU by default; HKLM needs
an elevated shell); host keys always go under HKCU. -RegistryRoot replaces "<Scope>:\Software" for tests.
Windows PowerShell 5.1 compatible.
#>
[CmdletBinding()]
param(
    [ValidateSet('HKCU', 'HKLM')][string]$Scope = 'HKCU',
    [Parameter(Mandatory = $true)][string]$IdsFile,
    [Parameter(Mandatory = $true)][string]$HostPath,
    [Parameter(Mandatory = $true)][string]$ManifestDir,
    [string]$RegistryRoot,
    [switch]$Remove
)
$ErrorActionPreference = 'Stop'
$ids = Get-Content -Raw -LiteralPath $IdsFile | ConvertFrom-Json
$policyRoot = if ($RegistryRoot) { $RegistryRoot } else { "$($Scope):\Software" }
$hostRoot = if ($RegistryRoot) { $RegistryRoot } else { 'HKCU:\Software' }
$cwsUpdate = 'https://clients2.google.com/service/update2/crx'
$edgeUpdate = 'https://edge.microsoft.com/extensionwebstorebase/v1/crx'

function Ensure-Key([string]$Key) {
    if (-not (Test-Path -LiteralPath $Key)) { New-Item -Path $Key -Force | Out-Null }
}

function Set-ForcelistEntry([string]$Key, [string]$Id, [string]$UpdateUrl) {
    if (-not (Test-Path -LiteralPath $Key)) {
        if ($Remove) { return }
        Ensure-Key $Key
    }
    $item = Get-Item -LiteralPath $Key
    $names = @($item.GetValueNames())
    foreach ($n in $names) {
        if ([string]$item.GetValue($n) -like "$Id;*") {
            if ($Remove) { Remove-ItemProperty -LiteralPath $Key -Name $n }
            return
        }
    }
    if ($Remove) { return }
    $i = 1
    while ($names -contains [string]$i) { $i++ }
    New-ItemProperty -LiteralPath $Key -Name ([string]$i) -Value "$Id;$UpdateUrl" -PropertyType String | Out-Null
}

function Set-DwordPolicy([string]$Key, [string]$Name, [int]$Value) {
    if ($Remove) {
        if (Test-Path -LiteralPath $Key) { Remove-ItemProperty -LiteralPath $Key -Name $Name -ErrorAction SilentlyContinue }
        return
    }
    Ensure-Key $Key
    New-ItemProperty -LiteralPath $Key -Name $Name -Value $Value -PropertyType DWord -Force | Out-Null
}

function Set-HostKey([string]$Key, [string]$ManifestPath) {
    if ($Remove) {
        if (Test-Path -LiteralPath $Key) { Remove-Item -LiteralPath $Key -Recurse }
        return
    }
    Ensure-Key $Key
    Set-Item -LiteralPath $Key -Value $ManifestPath
}

$chromiumManifest = Join-Path $ManifestDir 'app.cptk.overlay.chromium.json'
$firefoxManifest = Join-Path $ManifestDir 'app.cptk.overlay.firefox.json'
if ($Remove) {
    Remove-Item -LiteralPath $chromiumManifest, $firefoxManifest -ErrorAction SilentlyContinue
} else {
    New-Item -ItemType Directory -Path $ManifestDir -Force | Out-Null
    $utf8 = New-Object System.Text.UTF8Encoding $false
    $chromium = [ordered]@{
        name = 'app.cptk.overlay'; description = 'CPTK Overlay probe host'; path = $HostPath; type = 'stdio'
        allowed_origins = @("chrome-extension://$($ids.chrome_web_store)/", "chrome-extension://$($ids.edge_addons)/")
    }
    $firefox = [ordered]@{
        name = 'app.cptk.overlay'; description = 'CPTK Overlay probe host'; path = $HostPath; type = 'stdio'
        allowed_extensions = @($ids.gecko)
    }
    [IO.File]::WriteAllText($chromiumManifest, ($chromium | ConvertTo-Json), $utf8)
    [IO.File]::WriteAllText($firefoxManifest, ($firefox | ConvertTo-Json), $utf8)
}

$pol = "$policyRoot\Policies"
Set-ForcelistEntry "$pol\Google\Chrome\ExtensionInstallForcelist" $ids.chrome_web_store $cwsUpdate
Set-ForcelistEntry "$pol\Microsoft\Edge\ExtensionInstallForcelist" $ids.edge_addons $edgeUpdate
Set-DwordPolicy "$pol\Microsoft\Edge" 'StartupBoostEnabled' 0
Set-DwordPolicy "$pol\Microsoft\Edge" 'BackgroundModeEnabled' 0
Set-HostKey "$hostRoot\Google\Chrome\NativeMessagingHosts\app.cptk.overlay" $chromiumManifest
Set-HostKey "$hostRoot\Microsoft\Edge\NativeMessagingHosts\app.cptk.overlay" $chromiumManifest
Set-HostKey "$hostRoot\Mozilla\NativeMessagingHosts\app.cptk.overlay" $firefoxManifest
if ($Remove) {
    # An empty policy key left behind can keep a browser "Managed by your organization"; prune the ones this script
    # creates once they hold nothing, deepest first. Policies and Policies\Microsoft are shared and never pruned.
    foreach ($k in @("$pol\Google\Chrome\ExtensionInstallForcelist", "$pol\Google\Chrome", "$pol\Google",
                     "$pol\Microsoft\Edge\ExtensionInstallForcelist", "$pol\Microsoft\Edge")) {
        if (Test-Path -LiteralPath $k) {
            $item = Get-Item -LiteralPath $k
            if ($item.ValueCount -eq 0 -and $item.SubKeyCount -eq 0) { Remove-Item -LiteralPath $k }
        }
    }
}
Write-Output ("setup: {0} (policy under {1}, host keys under {2})" -f ($(if ($Remove) { 'removed' } else { 'wrote' })), $policyRoot, $hostRoot)
