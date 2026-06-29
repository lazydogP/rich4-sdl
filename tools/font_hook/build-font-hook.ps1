param(
    [ValidateSet('x86', 'x64')]
    [string]$Arch = 'x86'
)

$ErrorActionPreference = 'Stop'

$Here = Split-Path -Parent $MyInvocation.MyCommand.Path
$OutDir = Join-Path $Here 'bin'
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

if ($Arch -eq 'x86') {
    $Candidates = @(
        'i686-w64-mingw32-gcc.exe',
        'i686-w64-mingw32-gcc',
        'gcc.exe'
    )
    $Extra = @('-m32')
} else {
    $Candidates = @(
        'x86_64-w64-mingw32-gcc.exe',
        'x86_64-w64-mingw32-gcc',
        '..\..\..\.local\toolchain\mingw64\bin\gcc.exe',
        'gcc.exe'
    )
    $Extra = @()
}

$Gcc = $null
foreach ($Candidate in $Candidates) {
    $Command = Get-Command $Candidate -ErrorAction SilentlyContinue
    if ($Command) {
        $Gcc = $Command.Source
        break
    }
    $Local = Join-Path $Here $Candidate
    if (Test-Path $Local) {
        $Gcc = (Resolve-Path $Local).Path
        break
    }
}

if (!$Gcc) {
    throw "No MinGW GCC found for $Arch. Install i686-w64-mingw32-gcc for 32-bit Rich4."
}

$DllOut = Join-Path $OutDir 'font_hook.dll'
$ExeOut = Join-Path $OutDir 'font_hook_launcher.exe'

Write-Host "Using GCC: $Gcc"
& $Gcc @Extra -O2 -Wall -Wextra -shared -o $DllOut (Join-Path $Here 'font_hook_dll.c') -lgdi32 -luser32
if ($LASTEXITCODE -ne 0) {
    throw 'font_hook.dll build failed'
}
if (!(Test-Path $DllOut)) {
    throw "font_hook.dll was not created. Antivirus may have quarantined it."
}
& $Gcc @Extra -O2 -Wall -Wextra -o $ExeOut (Join-Path $Here 'font_hook_launcher.c')
if ($LASTEXITCODE -ne 0) {
    throw 'font_hook_launcher.exe build failed'
}
if (!(Test-Path $ExeOut)) {
    throw "font_hook_launcher.exe was not created. Antivirus may have quarantined it because it injects a DLL."
}

Write-Host "Built:"
Write-Host "  $DllOut"
Write-Host "  $ExeOut"
