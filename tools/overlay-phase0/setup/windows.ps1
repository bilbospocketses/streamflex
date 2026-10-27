<#
Phase 0 setup on Windows: force-install policy for Chrome and Edge, the Edge hygiene policies, and the
native-messaging host registration for all three browsers. Policy and host keys both go under "<Scope>:\Software":
HKCU by default, or HKLM (needs an elevated shell; pass a machine-wide -ManifestDir such as
"$env:ProgramData\cptk-probe\hosts"). -RegistryRoot replaces "<Scope>:\Software" for tests and must start with
HKCU:\ or HKLM:\. -ManifestDir may be relative; it is resolved to an absolute path. -HostPath must be absolute.

Setup records what it changes in "<ManifestDir>\setup-state.json": every value's prior state (absent, or its type
and data), the forcelist entries it created and the keys it created. -Remove reads that file, puts each prior value
back, deletes only the entries and keys it created, and deletes the file. With no state file (an install made before
the file existed) -Remove removes by name, as it used to, and says so.
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

function Fail([string]$Message) {
    [Console]::Error.WriteLine("setup: $Message")
    exit 1
}

# Validate everything before the first write.
$ids = Get-Content -Raw -LiteralPath $IdsFile | ConvertFrom-Json
foreach ($field in 'chrome_web_store', 'edge_addons') {
    if (-not ([string]$ids.$field -cmatch '^[a-p]{32}$')) { Fail "ids file ${IdsFile}: $field must be 32 letters a-p, got '$($ids.$field)'" }
}
# The profile scripts name the sideloaded XPI after this id, and Firefox loads it only under that name.
if ([string]$ids.gecko -cne 'overlay-probe@cptk.app') { Fail "ids file ${IdsFile}: gecko must be overlay-probe@cptk.app, got '$($ids.gecko)'" }
if ($HostPath -notmatch '^([A-Za-z]:\\|\\\\[^\\])') { Fail "-HostPath must be an absolute path (C:\... or \\server\...), got '$HostPath'" }
if ($RegistryRoot -and $RegistryRoot -notmatch '^(HKCU|HKLM):\\.') { Fail "-RegistryRoot must start with HKCU:\ or HKLM:\, got '$RegistryRoot'" }
$ManifestDir = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($ManifestDir)

$root = if ($RegistryRoot) { $RegistryRoot.TrimEnd('\') } else { "$($Scope):\Software" }
$pol = "$root\Policies"
$cwsUpdate = 'https://clients2.google.com/service/update2/crx'
$edgeUpdate = 'https://edge.microsoft.com/extensionwebstorebase/v1/crx'
$chromiumManifest = Join-Path $ManifestDir 'app.cptk.overlay.chromium.json'
$firefoxManifest = Join-Path $ManifestDir 'app.cptk.overlay.firefox.json'
$statePath = Join-Path $ManifestDir 'setup-state.json'
$utf8 = New-Object System.Text.UTF8Encoding $false
$forcelists = @(
    @{ Key = "$pol\Google\Chrome\ExtensionInstallForcelist"; Id = $ids.chrome_web_store; Url = $cwsUpdate },
    @{ Key = "$pol\Microsoft\Edge\ExtensionInstallForcelist"; Id = $ids.edge_addons; Url = $edgeUpdate }
)
$dwords = @('StartupBoostEnabled', 'BackgroundModeEnabled')
$hostKeys = @(
    @{ Key = "$root\Google\Chrome\NativeMessagingHosts\app.cptk.overlay"; Manifest = $chromiumManifest },
    @{ Key = "$root\Microsoft\Edge\NativeMessagingHosts\app.cptk.overlay"; Manifest = $chromiumManifest },
    @{ Key = "$root\Mozilla\NativeMessagingHosts\app.cptk.overlay"; Manifest = $firefoxManifest }
)
# The keys the by-name -Remove deletes once they hold nothing, deepest first. An empty policy key left behind can
# keep a browser "Managed by your organization". Policies and Policies\Microsoft are shared and never pruned.
$prunable = @("$pol\Google\Chrome\ExtensionInstallForcelist", "$pol\Google\Chrome", "$pol\Google",
              "$pol\Microsoft\Edge\ExtensionInstallForcelist", "$pol\Microsoft\Edge") + @($hostKeys | ForEach-Object { $_.Key })
# The value kinds a prior value can be recorded and put back as.
$kinds = @('String', 'ExpandString', 'DWord', 'QWord', 'MultiString', 'Binary')

$state = [ordered]@{
    version = 1
    values = New-Object System.Collections.ArrayList
    forcelist = New-Object System.Collections.ArrayList
    createdKeys = New-Object System.Collections.ArrayList
}
$hadState = Test-Path -LiteralPath $statePath
if ($hadState) {
    $saved = [IO.File]::ReadAllText($statePath, $utf8) | ConvertFrom-Json
    foreach ($field in 'values', 'forcelist', 'createdKeys') {
        foreach ($entry in @($saved.$field)) { if ($null -ne $entry) { [void]$state[$field].Add($entry) } }
    }
}

function Save-State {
    [IO.File]::WriteAllText($statePath, ($state | ConvertTo-Json -Depth 6), $utf8)
}

function Open-RegKey([string]$Key) {
    # Writable .NET handle (created if absent), so a value goes back with its own kind; the default value is "".
    $hive, $sub = $Key -split ':\\', 2
    $base = if ($hive -eq 'HKLM') { [Microsoft.Win32.Registry]::LocalMachine } else { [Microsoft.Win32.Registry]::CurrentUser }
    $base.CreateSubKey($sub)
}

function Write-RegValue([string]$Key, [string]$Name, [string]$Kind, $Data) {
    $rk = Open-RegKey $Key
    try { $rk.SetValue($Name, $Data, [Microsoft.Win32.RegistryValueKind]$Kind) } finally { $rk.Close() }
}

function Remove-RegValue([string]$Key, [string]$Name) {
    if (-not (Test-Path -LiteralPath $Key)) { return }
    $rk = Open-RegKey $Key
    try { $rk.DeleteValue($Name, $false) } finally { $rk.Close() }
}

function Ensure-Key([string]$Key) {
    $missing = @()
    $k = $Key
    while (-not (Test-Path -LiteralPath $k)) { $missing = @($k) + $missing; $k = Split-Path -Parent $k }
    if ($missing.Count -eq 0) { return }
    foreach ($m in $missing) { if (-not $state.createdKeys.Contains($m)) { [void]$state.createdKeys.Add($m) } }
    Save-State
    New-Item -Path $Key -Force | Out-Null
}

function Set-TrackedValue([string]$Key, [string]$Name, [string]$Kind, $Data) {
    # Records the value's prior state once, before the first change; a later run keeps that first record.
    Ensure-Key $Key
    if (-not ($state.values | Where-Object { $_.key -eq $Key -and $_.name -eq $Name })) {
        $item = Get-Item -LiteralPath $Key
        $prior = [ordered]@{ key = $Key; name = $Name; existed = $false; kind = $null; data = $null }
        if (@($item.GetValueNames()) -contains $Name) {
            $prior.existed = $true
            $prior.kind = [string]$item.GetValueKind($Name)
            $value = $item.GetValue($Name, $null, [Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames)
            $prior.data = switch ($prior.kind) {
                'Binary' { [Convert]::ToBase64String([byte[]]$value) }
                'MultiString' { , [string[]]$value }
                default { if ($kinds -contains $prior.kind) { $value } else { $null } }
            }
        }
        [void]$state.values.Add([pscustomobject]$prior)
        Save-State
    }
    Write-RegValue $Key $Name $Kind $Data
}

function Restore-Value($Record) {
    if (-not $Record.existed) { Remove-RegValue $Record.key $Record.name; return }
    if ($kinds -notcontains $Record.kind) {
        Write-Output "setup: cannot put back $($Record.key)\$($Record.name): its prior kind $($Record.kind) is not supported"
        return
    }
    $data = switch ($Record.kind) {
        'Binary' { , [Convert]::FromBase64String([string]$Record.data) }
        'MultiString' { , [string[]]@($Record.data) }
        'DWord' { [int]$Record.data }
        'QWord' { [long]$Record.data }
        default { [string]$Record.data }
    }
    Write-RegValue $Record.key $Record.name $Record.kind $data
}

function Remove-IfEmpty([string]$Key) {
    if (Test-Path -LiteralPath $Key) {
        $item = Get-Item -LiteralPath $Key
        if ($item.ValueCount -eq 0 -and $item.SubKeyCount -eq 0) { Remove-Item -LiteralPath $Key }
    }
}

function Find-Forcelist([string]$Key, [string]$Id) {
    # The value names under $Key whose entry is for $Id.
    if (-not (Test-Path -LiteralPath $Key)) { return }
    $item = Get-Item -LiteralPath $Key
    foreach ($n in @($item.GetValueNames())) { if ([string]$item.GetValue($n) -like "$Id;*") { $n } }
}

if ($Remove -and $hadState) {
    foreach ($f in $state.forcelist) {
        if ((Test-Path -LiteralPath $f.key) -and [string](Get-Item -LiteralPath $f.key).GetValue($f.name) -eq $f.data) {
            Remove-RegValue $f.key $f.name
        }
    }
    for ($j = $state.values.Count - 1; $j -ge 0; $j--) { Restore-Value $state.values[$j] }
    # Of the keys setup created, only the prunable kinds go, deepest first, and only once they hold nothing.
    $prunableTail = '\\(Policies\\Google(\\Chrome(\\ExtensionInstallForcelist)?)?|Policies\\Microsoft\\Edge(\\ExtensionInstallForcelist)?|NativeMessagingHosts\\app\.cptk\.overlay)$'
    foreach ($k in @(@($state.createdKeys) | Sort-Object -Property Length -Descending)) {
        if ($k -match $prunableTail) { Remove-IfEmpty $k }
    }
    Remove-Item -LiteralPath $chromiumManifest, $firefoxManifest, $statePath -ErrorAction SilentlyContinue
} elseif ($Remove) {
    Write-Output "setup: no $statePath; removing by name, so prior values cannot be put back"
    foreach ($f in $forcelists) { foreach ($n in @(Find-Forcelist $f.Key $f.Id)) { Remove-RegValue $f.Key $n } }
    foreach ($d in $dwords) { Remove-RegValue "$pol\Microsoft\Edge" $d }
    foreach ($h in $hostKeys) { if (Test-Path -LiteralPath $h.Key) { Remove-Item -LiteralPath $h.Key -Recurse } }
    foreach ($k in $prunable) { Remove-IfEmpty $k }
    Remove-Item -LiteralPath $chromiumManifest, $firefoxManifest -ErrorAction SilentlyContinue
} else {
    New-Item -ItemType Directory -Path $ManifestDir -Force | Out-Null
    $chromium = [ordered]@{
        name = 'app.cptk.overlay'; description = 'CPTK Overlay Probe host'; path = $HostPath; type = 'stdio'
        allowed_origins = @("chrome-extension://$($ids.chrome_web_store)/", "chrome-extension://$($ids.edge_addons)/")
    }
    $firefox = [ordered]@{
        name = 'app.cptk.overlay'; description = 'CPTK Overlay Probe host'; path = $HostPath; type = 'stdio'
        allowed_extensions = @($ids.gecko)
    }
    [IO.File]::WriteAllText($chromiumManifest, ($chromium | ConvertTo-Json), $utf8)
    [IO.File]::WriteAllText($firefoxManifest, ($firefox | ConvertTo-Json), $utf8)
    foreach ($f in $forcelists) {
        # An entry for this id that is already there, ours or IT's, is left as it is and never recorded as ours.
        if (@(Find-Forcelist $f.Key $f.Id).Count -gt 0) { continue }
        Ensure-Key $f.Key
        $names = @((Get-Item -LiteralPath $f.Key).GetValueNames())
        $i = 1
        while ($names -contains [string]$i) { $i++ }
        $entry = [pscustomobject]@{ key = $f.Key; name = [string]$i; data = "$($f.Id);$($f.Url)" }
        [void]$state.forcelist.Add($entry)
        Save-State
        Write-RegValue $entry.key $entry.name 'String' $entry.data
    }
    foreach ($d in $dwords) { Set-TrackedValue "$pol\Microsoft\Edge" $d 'DWord' 0 }
    foreach ($h in $hostKeys) { Set-TrackedValue $h.Key '' 'String' $h.Manifest }
    Save-State
}
Write-Output ("setup: {0} (policy and host keys under {1}, manifests in {2})" -f ($(if ($Remove) { 'removed' } else { 'wrote' })), $root, $ManifestDir)
