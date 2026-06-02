# gen_def.ps1
# Auto-generates winhttp.def with ALL exports forwarded to winhttp_orig.dll
# Excludes WinHttpConnect and WinHttpOpenRequest (we implement those in C++)
#
# Must run from VS Developer Command Prompt (needs dumpbin.exe)

$systemDll = "C:\Windows\System32\winhttp.dll"
$defFile   = "winhttp.def"
$origDll   = "winhttp_orig"

# Functions we implement ourselves - don't forward these
$skip = @("WinHttpConnect", "WinHttpOpenRequest")

Write-Host "Generating $defFile from $systemDll ..."

# Run dumpbin to get exports
$dumpOutput = & dumpbin /exports $systemDll 2>&1

# Parse export names from dumpbin output
$exports = @()
foreach ($line in $dumpOutput) {
    # Match lines like:          1    0 00003E70 WinHttpAddRequestHeaders
    if ($line -match '^\s+\d+\s+[0-9A-Fa-f]+\s+[0-9A-Fa-f]+\s+(\S+)\s*$') {
        $funcName = $Matches[1]
        if ($skip -notcontains $funcName) {
            $exports += $funcName
        }
    }
}

if ($exports.Count -eq 0) {
    Write-Host "ERROR: No exports found! Make sure dumpbin is available."
    Write-Host "Try running from 'x64 Native Tools Command Prompt for VS'"
    exit 1
}

# Write .def file
$defContent = "LIBRARY winhttp`nEXPORTS`n"
foreach ($func in $exports) {
    $defContent += "    $func=$origDll.$func`n"
}

Set-Content -Path $defFile -Value $defContent -Encoding ASCII

Write-Host ""
Write-Host "Generated $defFile with $($exports.Count) exports."
Write-Host "Skipped (intercepted): $($skip -join ', ')"
Write-Host ""
Write-Host "Export list:"
foreach ($func in $exports) {
    Write-Host "  $func -> $origDll.$func"
}
