[CmdletBinding()]
param(
    [string]$BuildDir = "build-final",
    [string]$OutputDir = "final",
    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$Configuration = "Release",
    [string]$Generator = "",
    [string]$Platform = "x64",
    [switch]$Clean
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$ProjectRoot = Split-Path -Parent $PSCommandPath

function Resolve-ProjectPath {
    param([string]$Path)

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return $Path
    }

    return Join-Path $ProjectRoot $Path
}

function Get-VisualStudioGenerator {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (!(Test-Path $vswhere)) {
        return $null
    }

    $installVersion = & $vswhere -latest -requires Microsoft.Component.MSBuild -property installationVersion
    if (!$installVersion) {
        return $null
    }

    $major = [int]($installVersion.Split(".")[0])
    if ($major -ge 18) {
        return "Visual Studio 18 2026"
    }
    if ($major -ge 17) {
        return "Visual Studio 17 2022"
    }
    if ($major -ge 16) {
        return "Visual Studio 16 2019"
    }

    return $null
}

$BuildPath = Resolve-ProjectPath $BuildDir
$OutputPath = Resolve-ProjectPath $OutputDir
$CachePath = Join-Path $BuildPath "CMakeCache.txt"

if ($Clean -and (Test-Path $BuildPath)) {
    Remove-Item -Recurse -Force $BuildPath
}

if (!(Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw "CMake was not found in PATH."
}

$configureArgs = @("-S", $ProjectRoot, "-B", $BuildPath)
if (!(Test-Path $CachePath)) {
    if ($Generator) {
        $configureArgs += @("-G", $Generator)
        if ($Platform) {
            $configureArgs += @("-A", $Platform)
        }
    } else {
        $detectedGenerator = Get-VisualStudioGenerator
        if ($detectedGenerator) {
            $configureArgs += @("-G", $detectedGenerator, "-A", $Platform)
        }
    }
}

Write-Host "Configuring CMake..."
& cmake @configureArgs

Write-Host "Building $Configuration..."
& cmake --build $BuildPath --config $Configuration --target MazeScape3D

$candidatePaths = @(
    (Join-Path $BuildPath "$Configuration\MazeScape3D.exe"),
    (Join-Path $BuildPath "MazeScape3D.exe")
)

$builtExe = $candidatePaths | Where-Object { Test-Path $_ } | Select-Object -First 1
if (!$builtExe) {
    $builtExe = Get-ChildItem -Path $BuildPath -Filter "MazeScape3D.exe" -Recurse |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1 -ExpandProperty FullName
}

if (!$builtExe) {
    throw "Build finished, but MazeScape3D.exe was not found under $BuildPath."
}

New-Item -ItemType Directory -Force -Path $OutputPath | Out-Null
$appExe = Join-Path $OutputPath "app.exe"
Copy-Item -Force $builtExe $appExe

Write-Host "Created $appExe"
