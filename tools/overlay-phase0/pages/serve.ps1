<#
Serves tools/overlay-phase0/pages (its pages, and the scripts under vendor\) at http://127.0.0.1:8765/ for a Windows guest with no Python.
Stop it with Ctrl+C. Linux guests use: python3 -m http.server 8765 --bind 127.0.0.1 --directory <pages>
#>
param([int]$Port = 8765)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$listener = New-Object System.Net.HttpListener
$listener.Prefixes.Add("http://127.0.0.1:$Port/")
$listener.Start()
Write-Output "serving $root at http://127.0.0.1:$Port/"
try {
    while ($listener.IsListening) {
        # A blocking GetContext() ignores Ctrl+C; waiting in 500 ms slices lets PowerShell stop between them.
        $pending = $listener.GetContextAsync()
        while (-not $pending.Wait(500)) { }
        $ctx = $pending.Result
        $rel = $ctx.Request.Url.AbsolutePath.TrimStart('/')
        if (-not $rel) { $rel = 'probe.html' }
        # A page or script in pages\, or a script under vendor\<package>\; never a path that climbs out of pages\.
        $ok = ($rel -match '^(vendor/[A-Za-z0-9._-]+/)?[A-Za-z0-9_-][A-Za-z0-9._-]*\.(html|js)$') -and ($rel -notmatch '\.\.')
        $file = Join-Path $root ($rel -replace '/', '\')
        if ($ok -and (Test-Path -LiteralPath $file -PathType Leaf)) {
            $bytes = [IO.File]::ReadAllBytes($file)
            if ($rel -like '*.js') { $ctx.Response.ContentType = 'text/javascript; charset=utf-8' }
            else { $ctx.Response.ContentType = 'text/html; charset=utf-8' }
            $ctx.Response.OutputStream.Write($bytes, 0, $bytes.Length)
        } else {
            $ctx.Response.StatusCode = 404
        }
        $ctx.Response.Close()
    }
} finally {
    $listener.Stop()
}
