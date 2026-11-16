param(
    [switch]$Force,
    [switch]$SkipVerify
)

$ErrorActionPreference = 'Stop'

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot '..')
$LocalDir = Join-Path $RepoRoot '.local'
$CacheDir = Join-Path $LocalDir 'cache'
$ToolchainDir = Join-Path $LocalDir 'toolchain'
$SdlDir = Join-Path $LocalDir 'sdl1'
$InstalledPackageDir = Join-Path $SdlDir '.installed'

$WinLibsUrl = 'https://github.com/brechtsanders/winlibs_mingw/releases/download/16.1.0posix-14.0.0-ucrt-r3/winlibs-x86_64-posix-seh-gcc-16.1.0-mingw-w64ucrt-14.0.0-r3.zip'
$WinLibsZip = Join-Path $CacheDir 'winlibs-x86_64-posix-seh-gcc-16.1.0-mingw-w64ucrt-14.0.0-r3.zip'
$GccExe = Join-Path $ToolchainDir 'mingw64\bin\gcc.exe'
$MakeExe = Join-Path $ToolchainDir 'mingw64\bin\mingw32-make.exe'

$SdlUrl = 'https://repo.msys2.org/mingw/ucrt64/mingw-w64-ucrt-x86_64-SDL-1.2.15%2Br419%2Bgef3a6c05-2-any.pkg.tar.zst'
$SdlZst = Join-Path $CacheDir 'mingw-w64-ucrt-x86_64-SDL.pkg.tar.zst'
$SdlTar = Join-Path $CacheDir 'mingw-w64-ucrt-x86_64-SDL.pkg.tar'
$SdlDll = Join-Path $SdlDir 'ucrt64\bin\SDL.dll'
$MixerHeader = Join-Path $SdlDir 'ucrt64\include\SDL\SDL_mixer.h'
$TtfHeader = Join-Path $SdlDir 'ucrt64\include\SDL\SDL_ttf.h'
# WinLibs uses UCRT, so SDL and every transitive runtime must use UCRT too.
$MsysRuntimePackages = @(
    'mingw-w64-ucrt-x86_64-libgcc',
    'mingw-w64-ucrt-x86_64-libstdc++',
    'mingw-w64-ucrt-x86_64-libwinpthread'
)
$MsysMixerPackages = @(
    'mingw-w64-ucrt-x86_64-sdl-mixer',
    'mingw-w64-ucrt-x86_64-flac',
    'mingw-w64-ucrt-x86_64-gettext-runtime',
    'mingw-w64-ucrt-x86_64-libiconv',
    'mingw-w64-ucrt-x86_64-libmad',
    'mingw-w64-ucrt-x86_64-libmikmod',
    'mingw-w64-ucrt-x86_64-libogg',
    'mingw-w64-ucrt-x86_64-libvorbis'
)
# SDL_mixer 1.2.12-11 imports SDL allocation functions absent from SDL 1.2.
# Keep the build made against native SDL 1.2 alongside the pinned SDL package.
$MsysPackagePins = @{
    'mingw-w64-ucrt-x86_64-sdl-mixer' = @{
        Url = 'https://repo.msys2.org/mingw/ucrt64/mingw-w64-ucrt-x86_64-SDL_mixer-1.2.12-10-any.pkg.tar.zst'
        Sha256 = '15e4dd92bf55cdda78c1cb93abf5471bab596b45f78f5e517f8209bc14865164'
    }
}
$MsysTtfPackages = @(
    'mingw-w64-ucrt-x86_64-sdl-ttf',
    'mingw-w64-ucrt-x86_64-freetype',
    'mingw-w64-ucrt-x86_64-harfbuzz',
    'mingw-w64-ucrt-x86_64-glib2',
    'mingw-w64-ucrt-x86_64-libffi',
    'mingw-w64-ucrt-x86_64-pcre2',
    'mingw-w64-ucrt-x86_64-graphite2',
    'mingw-w64-ucrt-x86_64-brotli',
    'mingw-w64-ucrt-x86_64-bzip2',
    'mingw-w64-ucrt-x86_64-libpng',
    'mingw-w64-ucrt-x86_64-zlib'
)

function New-Directory($Path) {
    if (!(Test-Path $Path)) {
        New-Item -ItemType Directory -Force -Path $Path | Out-Null
    }
}

function Save-Url($Url, $Path) {
    if ($Force -or !(Test-Path $Path)) {
        Write-Host "Downloading $Url"
        Invoke-WebRequest -Uri $Url -OutFile $Path
    } else {
        Write-Host "Using cached $Path"
    }
}

function Resolve-Msys2PackageUrl($PackageName) {
    $Page = "https://packages.msys2.org/package/$PackageName"
    Write-Host "Resolving $PackageName"
    $Html = (Invoke-WebRequest -Uri $Page).Content
    $Match = [regex]::Match($Html, 'https://mirror\.msys2\.org/mingw/ucrt64/[^"<]+\.pkg\.tar\.zst')
    if (!$Match.Success) {
        throw "Could not resolve MSYS2 package URL for $PackageName"
    }
    return $Match.Value -replace '^https://mirror\.msys2\.org/', 'https://repo.msys2.org/'
}

function Expand-Zip($Zip, $Destination, $Probe) {
    if ($Force -and (Test-Path $Destination)) {
        Remove-Item -LiteralPath $Destination -Recurse -Force
    }
    if (!(Test-Path $Probe)) {
        Write-Host "Extracting $Zip"
        New-Directory $Destination
        Expand-Archive -LiteralPath $Zip -DestinationPath $Destination -Force
    } else {
        Write-Host "Toolchain already present: $Probe"
    }
}

function Ensure-Zstandard {
    $script = @'
import importlib.util
raise SystemExit(0 if importlib.util.find_spec("zstandard") else 1)
'@
    $script | python -
    if ($LASTEXITCODE -ne 0) {
        Write-Host 'Installing Python zstandard module for .tar.zst extraction'
        python -m pip install --user zstandard
    }
}

function Expand-ZstdTar($Zst, $Tar, $Destination, $Probe) {
    if (Test-Path $Probe) {
        Write-Host "Package content already present: $Probe"
        return
    }

    Ensure-Zstandard
    Write-Host "Decompressing $Zst"
    $py = @"
from pathlib import Path
import zstandard as zstd
src = Path(r'$Zst')
dst = Path(r'$Tar')
with src.open('rb') as fin, dst.open('wb') as fout:
    zstd.ZstdDecompressor().copy_stream(fin, fout)
print(dst)
"@
    $py | python -

    Write-Host "Extracting $Tar"
    New-Directory $Destination
    tar -xf $Tar -C $Destination
}

function Install-Msys2Package($PackageName, $Destination) {
    $Marker = Join-Path $InstalledPackageDir "$PackageName.ok"
    $Pin = $MsysPackagePins[$PackageName]
    if (!$Force -and (Test-Path $Marker) -and
        (!$Pin -or (Get-Content -LiteralPath $Marker -Raw).Trim() -eq $Pin.Url)) {
        Write-Host "Package already extracted: $PackageName"
        return
    }
    if ($Pin) {
        $Url = $Pin.Url
        $CacheName = ([uri]$Url).Segments[-1]
    } else {
        $Url = Resolve-Msys2PackageUrl $PackageName
        $CacheName = "$PackageName.pkg.tar.zst"
    }
    $Zst = Join-Path $CacheDir $CacheName
    $Tar = Join-Path $CacheDir "$PackageName.pkg.tar"
    Save-Url $Url $Zst
    if ($Pin -and (Get-FileHash -LiteralPath $Zst -Algorithm SHA256).Hash -ne $Pin.Sha256) {
        throw "SHA256 mismatch for $Zst"
    }
    # A changed pin must replace an already extracted package as well.
    if (Test-Path $Marker) {
        Remove-Item -LiteralPath $Marker -Force
    }
    Expand-ZstdTar $Zst $Tar $Destination $Marker
    New-Directory $InstalledPackageDir
    Set-Content -Path $Marker -Value $Url
}

function Test-SdlRuntime {
    Write-Host 'Checking SDL runtime DLL loading'
    $py = @'
import ctypes
from pathlib import Path
import sys

ctypes.windll.kernel32.SetErrorMode(0x8003)
directory = Path(sys.argv[1]).resolve()
for name in ("SDL.dll", "libSDL_mixer-1-2-0.dll", "SDL_ttf.dll"):
    try:
        # Resolve transitive DLLs from this package and Windows, without PATH.
        ctypes.WinDLL(str(directory / name), winmode=0x1100)
    except OSError as exc:
        raise SystemExit(f"SDL runtime load failed: {directory / name}: {exc}")
    print(f"SDL runtime load OK: {name}")
'@
    $py | python - (Join-Path $SdlDir 'ucrt64\bin')
    if ($LASTEXITCODE -ne 0) {
        throw 'SDL runtime DLL loading failed'
    }
}

function Run-Verify {
    Write-Host 'Verifying local toolchain'
    & $GccExe --version | Select-Object -First 1
    & $MakeExe --version | Select-Object -First 1

    Write-Host 'Building tools'
    & $MakeExe -C $RepoRoot tools
    if ($LASTEXITCODE -ne 0) {
        throw 'tools build failed'
    }

    Write-Host 'Building SDL validation program'
    & $MakeExe -C $RepoRoot sdl
    if ($LASTEXITCODE -ne 0) {
        throw 'SDL build failed'
    }
}

if ($env:OS -ne 'Windows_NT') {
    throw 'This setup script is intended for Windows.'
}

New-Directory $CacheDir
New-Directory $ToolchainDir

if ($Force -and (Test-Path $SdlDir)) {
    Remove-Item -LiteralPath $SdlDir -Recurse -Force
}
New-Directory $SdlDir
New-Directory $InstalledPackageDir

Save-Url $WinLibsUrl $WinLibsZip
Expand-Zip $WinLibsZip $ToolchainDir $GccExe

Save-Url $SdlUrl $SdlZst
Expand-ZstdTar $SdlZst $SdlTar $SdlDir $SdlDll

foreach ($PackageName in $MsysRuntimePackages) {
    Install-Msys2Package $PackageName $SdlDir
}

foreach ($PackageName in $MsysMixerPackages) {
    Install-Msys2Package $PackageName $SdlDir
}

foreach ($PackageName in $MsysTtfPackages) {
    Install-Msys2Package $PackageName $SdlDir
}

$RequiredSdlFiles = @(
    $SdlDll,
    $MixerHeader,
    $TtfHeader,
    (Join-Path $SdlDir 'ucrt64\bin\libgcc_s_seh-1.dll'),
    (Join-Path $SdlDir 'ucrt64\bin\libstdc++-6.dll'),
    (Join-Path $SdlDir 'ucrt64\bin\libwinpthread-1.dll')
)
foreach ($RequiredFile in $RequiredSdlFiles) {
    if (!(Test-Path $RequiredFile)) {
        throw "Required UCRT runtime file was not installed: $RequiredFile"
    }
}

# -SkipVerify skips compilation, but CI still needs a loadable runtime.
Test-SdlRuntime

if (!$SkipVerify) {
    Run-Verify
}

Write-Host ''
Write-Host 'Windows toolchain is ready.'
Write-Host "GCC:  $GccExe"
Write-Host "Make: $MakeExe"
Write-Host "SDL:  $SdlDll"
Write-Host "Mix:  $MixerHeader"
Write-Host "TTF:  $TtfHeader"
