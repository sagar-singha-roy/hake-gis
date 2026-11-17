#Requires -Version 5.1
<#
.SYNOPSIS
  Package assertions + NSIS install + PROJ/GDAL runtime smoke for Hake GeoDesk Windows CI.

.DESCRIPTION
  Mirrors src/app/mainwin.cpp env expansion for {app}/{prefix} from hake-geodesk.env,
  then runs scripts/ci/windows_runtime_smoke.py with the bundled python.exe.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir,

    [Parameter(Mandatory = $true)]
    [string]$RepoRoot,

    [Parameter(Mandatory = $false)]
    [string]$ReportDir = "",

    [Parameter(Mandatory = $false)]
    [ValidateSet("All", "AssertZip", "Install", "Smoke", "Process", "Portable", "Runtime")]
    [string]$Mode = "All"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

if (-not $ReportDir) {
    $ReportDir = Join-Path $BuildDir "runtime-smoke"
}
New-Item -ItemType Directory -Force -Path $ReportDir | Out-Null

function Write-Section([string]$Title) {
    Write-Host ""
    Write-Host "==== $Title ===="
}

function Get-CPackZip {
    $zips = Get-ChildItem -Path $BuildDir -Filter *.zip -ErrorAction SilentlyContinue |
        Sort-Object FullName
    if (-not $zips) {
        throw "No CPack zip in $BuildDir"
    }
    return $zips[0]
}

function Assert-ZipContents {
    Write-Section "Package content assertion (ZIP)"
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $zip = Get-CPackZip
    Write-Host "Inspecting $($zip.FullName)"
    $archive = [System.IO.Compression.ZipFile]::OpenRead($zip.FullName)
    try {
        $names = @($archive.Entries | ForEach-Object { ($_.FullName -replace '\\', '/') })

        $projDb = @($names | Where-Object { $_ -match '(^|/)share/proj/proj\.db$' })
        $gdalData = @($names | Where-Object { $_ -match '(^|/)share/gdal/' })
        $python = @($names | Where-Object { $_ -match '(^|/)bin/python\.exe$' })
        $polyBat = @($names | Where-Object { $_ -match '(^|/)bin/gdal_polygonize\.bat$' })
        $polyPy = @($names | Where-Object { $_ -match '(^|/)bin/Scripts/gdal_polygonize\.py$' })
        $osgeo = @($names | Where-Object { $_ -match '(^|/)bin/Lib/site-packages/osgeo/' })
        $envFile = @($names | Where-Object { $_ -match '(^|/)bin/hake-geodesk\.env$' })
        $process = @($names | Where-Object { $_ -match '(^|/)bin/hake-geodesk-process\.exe$' })
        $allProjDb = @($names | Where-Object { $_ -match '(^|/)proj\.db$' })
        $projDlls = @($names | Where-Object { $_ -match '(^|/)bin/proj[^/]*\.dll$' })
        $qtPlatform = @($names | Where-Object { $_ -match '(^|/)bin/Qt6/plugins/platforms/qwindows\.dll$' })
        $qtCoreDll = @($names | Where-Object { $_ -match '(^|/)bin/Qt6Core\.dll$' })
        $qtGuiDll = @($names | Where-Object { $_ -match '(^|/)bin/Qt6Gui\.dll$' })
        $qtWidgetsDll = @($names | Where-Object { $_ -match '(^|/)bin/Qt6Widgets\.dll$' })
        $qtNetworkDll = @($names | Where-Object { $_ -match '(^|/)bin/Qt6Network\.dll$' })
        $qtMultimediaDll = @($names | Where-Object { $_ -match '(^|/)bin/Qt6Multimedia\.dll$' })
        $qtPositioningDll = @($names | Where-Object { $_ -match '(^|/)bin/Qt6Positioning\.dll$' })
        $qgisCoreDll = @($names | Where-Object { $_ -match '(^|/)bin/qgis_core\.dll$' })
        $qgisGuiDll = @($names | Where-Object { $_ -match '(^|/)bin/qgis_gui\.dll$' })
        $qgisAppDll = @($names | Where-Object { $_ -match '(^|/)bin/qgis_app\.dll$' })
        $appExe = @($names | Where-Object { $_ -match '(^|/)bin/hake-geodesk\.exe$' })
        $pyqtCore = @($names | Where-Object { $_ -match '(^|/)bin/Lib/site-packages/PyQt6/QtCore[^/]*\.pyd$' })
        $pyqtGui = @($names | Where-Object { $_ -match '(^|/)bin/Lib/site-packages/PyQt6/QtGui[^/]*\.pyd$' })
        $pyqtWidgets = @($names | Where-Object { $_ -match '(^|/)bin/Lib/site-packages/PyQt6/QtWidgets[^/]*\.pyd$' })
        $pyqtMultimedia = @($names | Where-Object { $_ -match '(^|/)bin/Lib/site-packages/PyQt6/QtMultimedia[^/]*\.pyd$' })
        $pyqtPositioning = @($names | Where-Object { $_ -match '(^|/)bin/Lib/site-packages/PyQt6/QtPositioning[^/]*\.pyd$' })
        $pyqtSip = @($names | Where-Object { $_ -match '(^|/)bin/Lib/site-packages/PyQt6/sip[^/]*\.pyd$' })
        $qgisCore = @($names | Where-Object { $_ -match '(^|/)python/qgis/core/__init__\.py$' })
        $qgisGui = @($names | Where-Object { $_ -match '(^|/)python/qgis/gui/__init__\.py$' })
        $qgisCorePyd = @($names | Where-Object { $_ -match '(^|/)python/qgis/_core[^/]*\.pyd$' })
        $qgisGuiPyd = @($names | Where-Object { $_ -match '(^|/)python/qgis/_gui[^/]*\.pyd$' })

        Write-Host "proj.db entries:"
        $allProjDb | ForEach-Object { Write-Host "  $_" }
        Write-Host "proj*.dll entries:"
        $projDlls | ForEach-Object { Write-Host "  $_" }
        Write-Host "Qt6 platform plugin entries:"
        $qtPlatform | ForEach-Object { Write-Host "  $_" }
        Write-Host "Qt6 core/gui/widgets DLL entries:"
        Write-Host "  Qt6Core: $($qtCoreDll.Count), Qt6Gui: $($qtGuiDll.Count), Qt6Widgets: $($qtWidgetsDll.Count)"
        Write-Host "PyQt6 multimedia entries:"
        $pyqtMultimedia | ForEach-Object { Write-Host "  $_" }
        Write-Host "PyQt6 positioning entries:"
        $pyqtPositioning | ForEach-Object { Write-Host "  $_" }

        $missing = @()
        if (-not $projDb) { $missing += "share/proj/proj.db" }
        if (-not $gdalData) { $missing += "share/gdal/*" }
        if (-not $python) { $missing += "bin/python.exe" }
        if (-not $polyBat) { $missing += "bin/gdal_polygonize.bat" }
        if (-not $polyPy) { $missing += "bin/Scripts/gdal_polygonize.py" }
        if (-not $osgeo) { $missing += "bin/Lib/site-packages/osgeo/" }
        if (-not $envFile) { $missing += "bin/hake-geodesk.env" }
        if (-not $process) { $missing += "bin/hake-geodesk-process.exe" }
        if (-not $appExe) { $missing += "bin/hake-geodesk.exe" }
        if (-not $qtCoreDll) { $missing += "bin/Qt6Core.dll" }
        if (-not $qtGuiDll) { $missing += "bin/Qt6Gui.dll" }
        if (-not $qtWidgetsDll) { $missing += "bin/Qt6Widgets.dll" }
        if (-not $qtMultimediaDll) { $missing += "bin/Qt6Multimedia.dll" }
        if (-not $qtPositioningDll) { $missing += "bin/Qt6Positioning.dll" }
        if (-not $qgisCoreDll) { $missing += "bin/qgis_core.dll" }
        if (-not $qgisGuiDll) { $missing += "bin/qgis_gui.dll" }
        if (-not $qgisAppDll) { $missing += "bin/qgis_app.dll" }
        if (-not $qtPlatform) { $missing += "bin/Qt6/plugins/platforms/qwindows.dll" }
        if (-not $pyqtCore) { $missing += "bin/Lib/site-packages/PyQt6/QtCore*.pyd" }
        if (-not $pyqtGui) { $missing += "bin/Lib/site-packages/PyQt6/QtGui*.pyd" }
        if (-not $pyqtWidgets) { $missing += "bin/Lib/site-packages/PyQt6/QtWidgets*.pyd" }
        # Note: PyQt6.QtMultimedia is an optional module in vcpkg py-pyqt6; do not fail if omitted
        if (-not $pyqtPositioning) { $missing += "bin/Lib/site-packages/PyQt6/QtPositioning*.pyd" }
        if (-not $qgisCore) { $missing += "python/qgis/core/__init__.py" }
        if (-not $qgisGui) { $missing += "python/qgis/gui/__init__.py" }
        if (-not $qgisCorePyd) { $missing += "python/qgis/_core*.pyd" }
        if (-not $qgisGuiPyd) { $missing += "python/qgis/_gui*.pyd" }

        if ($missing.Count -gt 0) {
            throw "ZIP missing required packaged paths: $($missing -join ', ')"
        }

        # Verify packaged .env points PROJ_DATA / PROJ_LIB / GDAL_DATA at {prefix}\share\...
        $envEntry = $archive.Entries | Where-Object {
            ($_.FullName -replace '\\', '/') -match '(^|/)bin/hake-geodesk\.env$'
        } | Select-Object -First 1
        $reader = New-Object System.IO.StreamReader($envEntry.Open())
        try { $envText = $reader.ReadToEnd() } finally { $reader.Dispose() }
        Write-Host "Packaged hake-geodesk.env:"
        Write-Host $envText
        foreach ($req in @(
            'PROJ_DATA={prefix}\share\proj',
            'PROJ_LIB={prefix}\share\proj',
            'GDAL_DATA={prefix}\share\gdal'
        )) {
            if ($envText -notlike "*$req*") {
                throw "Packaged hake-geodesk.env missing required line: $req"
            }
        }

        Write-Host "ZIP content assertion PASSED"    }
    finally {
        $archive.Dispose()
    }
}

function Install-NsisArtifact {
    Write-Section "Silent NSIS install"
    $exes = @(Get-ChildItem -Path $BuildDir -Filter "*.exe" -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -like "Hake-Geodesk-Installer*" -or $_.Name -like "*Installer*.exe" } |
        Sort-Object FullName)
    if (-not $exes) {
        # Fall back to any CPack-produced exe in build/
        $exes = @(Get-ChildItem -Path $BuildDir -Filter "*.exe" -ErrorAction SilentlyContinue |
            Where-Object { $_.Length -gt 10MB } |
            Sort-Object Length -Descending)
    }
    if (-not $exes) {
        throw "No NSIS installer .exe found in $BuildDir"
    }
    $installer = $exes[0]
    # ASCII install folder (HAKE_PRODUCT_INSTALL_DIRECTORY). En-dash paths break
    # PROJ/SQLite open of proj.db on Windows. Issue #10 coverage is launcher
    # PROJ_DATA/PROJ_LIB override + packaged layout, not a Unicode install folder.
    $target = "C:\Hake GeoDesk"
    Write-Host "Installing $($installer.FullName) silently into $target ..."

    # NSIS: /S = silent, /D= must be last and unquoted
    $proc = Start-Process -FilePath $installer.FullName -ArgumentList "/S", "/D=$target" -Wait -PassThru
    if ($proc.ExitCode -ne 0) {
        throw "NSIS installer exited with code $($proc.ExitCode)"
    }
    if (-not (Test-Path -LiteralPath (Join-Path $target "bin\hake-geodesk.exe"))) {
        throw "NSIS install finished but bin\hake-geodesk.exe missing under $target"
    }
    Write-Host "NSIS installer completed"
    return $target
}

function Find-InstallRoot {
    Write-Section "Locate installed application"
    $candidates = @()

    # Preferred CI install path (ASCII; matches HAKE_PRODUCT_INSTALL_DIRECTORY)
    $candidates += "C:\Hake GeoDesk"

    $pf = ${env:ProgramFiles}
    if ($pf) {
        Get-ChildItem -Path $pf -Directory -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -like "Hake GeoDesk*" -or $_.Name -like "Hake*GIS*" } |
            ForEach-Object { $candidates += $_.FullName }
    }
    $pf86 = ${env:ProgramFiles(x86)}
    if ($pf86) {
        Get-ChildItem -Path $pf86 -Directory -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -like "Hake GeoDesk*" } |
            ForEach-Object { $candidates += $_.FullName }
    }

    # Also search for hake-geodesk.exe under Program Files / C:\Hake*
    foreach ($root in @($pf, $pf86, "C:\") | Where-Object { $_ }) {
        $searchRoots = @()
        if ($root -eq "C:\") {
            $searchRoots = @(Get-ChildItem -Path "C:\" -Directory -ErrorAction SilentlyContinue |
                Where-Object { $_.Name -like "Hake*" } |
                Select-Object -ExpandProperty FullName)
        }
        else {
            $searchRoots = @($root)
        }
        foreach ($sr in $searchRoots) {
            $found = Get-ChildItem -Path $sr -Filter "hake-geodesk.exe" -Recurse -ErrorAction SilentlyContinue |
                Select-Object -First 5
            foreach ($f in $found) {
                $candidates += (Split-Path (Split-Path $f.FullName -Parent) -Parent)
            }
        }
    }

    $candidates = @($candidates | Select-Object -Unique)
    Write-Host "Candidate install roots:"
    $candidates | ForEach-Object { Write-Host "  $_" }

    foreach ($c in $candidates) {
        $exe = Join-Path $c "bin\hake-geodesk.exe"
        if (Test-Path -LiteralPath $exe) {
            Write-Host "Selected install root: $c"
            return (Resolve-Path -LiteralPath $c).Path
        }
    }
    throw "Could not locate installed Hake GeoDesk (bin\hake-geodesk.exe)"
}

function Find-ProjDb([string]$InstallRoot) {
    Write-Section "Discover proj.db under install root"
    $hits = @(Get-ChildItem -LiteralPath $InstallRoot -Filter "proj.db" -Recurse -ErrorAction SilentlyContinue)
    if (-not $hits) {
        throw "proj.db NOT FOUND under $InstallRoot"
    }
    Write-Host "proj.db locations:"
    $hits | ForEach-Object { Write-Host "  $($_.FullName)" }
    $expected = Join-Path $InstallRoot "share\proj\proj.db"
    if (-not (Test-Path -LiteralPath $expected)) {
        throw "proj.db exists but not at expected packaged path: $expected"
    }
    Write-Host "Expected path OK: $expected"
    return $hits
}

function Expand-EnvValue([string]$Value, [string]$AppDir, [string]$PrefixDir) {
    $v = $Value.Replace("{app}", $AppDir).Replace("{prefix}", $PrefixDir)
    # Expand %VAR% style if present
    $v = [Environment]::ExpandEnvironmentVariables($v)
    return $v
}

function Get-LauncherEnvironment([string]$InstallRoot) {
    $appDir = Join-Path $InstallRoot "bin"
    $prefixDir = $InstallRoot
    $envFile = Join-Path $appDir "hake-geodesk.env"
    if (-not (Test-Path -LiteralPath $envFile)) {
        throw "Missing hake-geodesk.env at $envFile"
    }

    $map = [ordered]@{}
    Get-Content -LiteralPath $envFile -Encoding UTF8 | ForEach-Object {
        $line = $_.TrimEnd("`r")
        if (-not $line -or $line.StartsWith("#")) { return }
        $eq = $line.IndexOf("=")
        if ($eq -lt 1) { return }
        $name = $line.Substring(0, $eq)
        $value = Expand-EnvValue $line.Substring($eq + 1) $appDir $prefixDir
        if ($name -ieq "PATH") {
            $old = [Environment]::GetEnvironmentVariable("PATH", "Process")
            if ($old) {
                $value = "$value;$old"
            }
        }
        $map[$name] = $value
    }
    return @{
        AppDir    = $appDir
        PrefixDir = $prefixDir
        Env       = $map
        EnvFile   = $envFile
    }
}

function Clear-ProjEnvVars {
    foreach ($k in @("PROJ_DATA", "PROJ_LIB", "GDAL_DATA")) {
        Remove-Item -Path "Env:$k" -ErrorAction SilentlyContinue
        [Environment]::SetEnvironmentVariable($k, $null, "Process")
    }
}

function Apply-EnvMap([hashtable]$Map) {
    foreach ($k in $Map.Keys) {
        Set-Item -Path "Env:$k" -Value $Map[$k]
        [Environment]::SetEnvironmentVariable($k, $Map[$k], "Process")
    }
}

function Write-EnvDiagnostics([string]$Label) {
    Write-Host "[$Label] PROJ_DATA=$env:PROJ_DATA"
    Write-Host "[$Label] PROJ_LIB=$env:PROJ_LIB"
    Write-Host "[$Label] GDAL_DATA=$env:GDAL_DATA"
    $pathParts = @($env:PATH -split ";" | Select-Object -First 8)
    Write-Host "[$Label] PATH(first)=$($pathParts -join ';')"
    Write-Host "[$Label] where.exe diagnostics:"
    foreach ($tool in @("python", "gdalinfo", "projinfo")) {
        try {
            $w = & where.exe $tool 2>$null
            if ($w) { Write-Host "  $tool => $($w -join ' | ')" }
            else { Write-Host "  $tool => (not found)" }
        }
        catch {
            Write-Host "  $tool => (not found)"
        }
    }
}

function Invoke-AppExeVersion([string]$InstallRoot, [string]$Label) {
    Write-Section "Executable version invocation ($Label)"
    $appExe = Join-Path $InstallRoot "bin\hake-geodesk.exe"
    $processExe = Join-Path $InstallRoot "bin\hake-geodesk-process.exe"

    if (-not (Test-Path -LiteralPath $appExe)) {
        throw "Missing $appExe"
    }
    if (-not (Test-Path -LiteralPath $processExe)) {
        throw "Missing $processExe"
    }

    $launcher = Get-LauncherEnvironment $InstallRoot
    Apply-EnvMap $launcher.Env
    $env:QT_QPA_PLATFORM = "offscreen"

    Write-Host "Running $appExe --version ..."
    $logApp = Join-Path $ReportDir "app_version_$Label.log"
    & $appExe --version *> $logApp
    $codeApp = $LASTEXITCODE
    Get-Content -LiteralPath $logApp -ErrorAction SilentlyContinue | Write-Host
    if ($codeApp -ne 0) {
        throw "$appExe --version exited with code $codeApp (see $logApp)"
    }
    Write-Host "App launcher executable ($appExe --version) PASSED ($Label)"

    Write-Host "Running $processExe --version ..."
    $logProc = Join-Path $ReportDir "process_version_$Label.log"
    & $processExe --version *> $logProc
    $codeProc = $LASTEXITCODE
    Get-Content -LiteralPath $logProc -ErrorAction SilentlyContinue | Write-Host
    if ($codeProc -ne 0) {
        throw "$processExe --version exited with code $codeProc (see $logProc)"
    }
    Write-Host "Process CLI executable ($processExe --version) PASSED ($Label)"
}

function Invoke-Smoke([string]$InstallRoot, [string]$Label, [switch]$Isolated) {
    Write-Section "Runtime smoke ($Label)"
    $python = Join-Path $InstallRoot "bin\python.exe"
    if (-not (Test-Path -LiteralPath $python)) {
        throw "Bundled python.exe missing: $python"
    }

    if (-not $Isolated) {
        Invoke-AppExeVersion -InstallRoot $InstallRoot -Label $Label
    }

    Clear-ProjEnvVars

    $launcher = Get-LauncherEnvironment $InstallRoot
    if ($Isolated) {
        # Only PATH=bin; no PROJ_DATA / GDAL_DATA — prove DLL-relative discovery
        $windir = $env:WINDIR
        $pathOnly = "$($launcher.AppDir);$windir;$windir\system32;$windir\system32\WBem"
        Clear-ProjEnvVars
        $env:PATH = $pathOnly
        $env:PYTHONHOME = $launcher.AppDir
        $env:PYTHONPATH = "$($launcher.PrefixDir)\python;$($launcher.AppDir)\Lib;$($launcher.AppDir)\Lib\site-packages;$($launcher.AppDir)\DLLs"
        $env:QGIS_PREFIX_PATH = $launcher.PrefixDir
        Write-Host "Isolated env: PROJ_DATA/PROJ_LIB/GDAL_DATA unset; PATH=bin+system32"
    }
    else {
        # Simulate a machine-global PostGIS-style PROJ_LIB; launcher .env must override it.
        $env:PROJ_LIB = "C:\nonexistent\PostgreSQL\share\contrib\postgis\proj"
        Write-Host "Seeded stale PROJ_LIB=$env:PROJ_LIB (must be overridden by hake-geodesk.env)"
        Apply-EnvMap $launcher.Env
        Write-Host "Launcher-equivalent env from $($launcher.EnvFile)"
        $expectedProj = [System.IO.Path]::GetFullPath((Join-Path $InstallRoot "share\proj"))
        $gotLib = if ($env:PROJ_LIB) { [System.IO.Path]::GetFullPath($env:PROJ_LIB) } else { "" }
        $gotData = if ($env:PROJ_DATA) { [System.IO.Path]::GetFullPath($env:PROJ_DATA) } else { "" }
        if ($gotLib -ne $expectedProj) {
            throw "PROJ_LIB was not overridden by launcher env. Got '$env:PROJ_LIB', expected '$expectedProj'"
        }
        if ($gotData -ne $expectedProj) {
            throw "PROJ_DATA incorrect. Got '$env:PROJ_DATA', expected '$expectedProj'"
        }
        if (-not (Test-Path -LiteralPath (Join-Path $env:PROJ_DATA "proj.db"))) {
            throw "proj.db missing at PROJ_DATA=$env:PROJ_DATA"
        }
    }

    Write-EnvDiagnostics $Label

    $script = Join-Path $RepoRoot "scripts\ci\windows_runtime_smoke.py"
    # Copy script into report dir so we do not depend on repo path encoding inside python
    $localScript = Join-Path $ReportDir "windows_runtime_smoke.py"
    Copy-Item -LiteralPath $script -Destination $localScript -Force

    & $python -u $localScript `
        --install-root $InstallRoot `
        --report-dir $ReportDir `
        --label $Label
    if ($LASTEXITCODE -ne 0) {
        throw "Smoke test '$Label' failed with exit $LASTEXITCODE"
    }
}

function Invoke-ProcessPolygonize([string]$InstallRoot) {
    Write-Section "hake-geodesk-process gdal:polygonize"
    $processExe = Join-Path $InstallRoot "bin\hake-geodesk-process.exe"
    if (-not (Test-Path -LiteralPath $processExe)) {
        throw "Missing $processExe"
    }

    Clear-ProjEnvVars
    $launcher = Get-LauncherEnvironment $InstallRoot
    Apply-EnvMap $launcher.Env
    $env:QT_QPA_PLATFORM = "offscreen"
    # Process tool is not launched via mainwin; launcher env supplies PROJ_DATA.
    Write-EnvDiagnostics "process"

    $work = Join-Path $ReportDir "process-work"
    New-Item -ItemType Directory -Force -Path $work | Out-Null

    # Build a tiny raster with the bundled python/GDAL
    $python = Join-Path $InstallRoot "bin\python.exe"
    $tif = Join-Path $work "process_input.tif"
    $out = Join-Path $work "process_output.gpkg"
    $mk = @"
from osgeo import gdal, osr
gdal.UseExceptions()
drv = gdal.GetDriverByName('GTiff')
ds = drv.Create(r'$($tif.Replace('\','\\'))', 32, 32, 1, gdal.GDT_Byte)
srs = osr.SpatialReference(); srs.ImportFromEPSG(2193)
ds.SetProjection(srs.ExportToWkt())
ds.SetGeoTransform([1600000.0, 10.0, 0.0, 5500000.0, 0.0, -10.0])
band = ds.GetRasterBand(1)
data = bytes([1]*16 + [2]*16) * 32
band.WriteRaster(0, 0, 32, 32, data)
ds = None
print('wrote', r'$($tif.Replace('\','\\'))')
"@
    $mkPath = Join-Path $work "mk_raster.py"
    Set-Content -LiteralPath $mkPath -Value $mk -Encoding UTF8
    & $python -u $mkPath
    if ($LASTEXITCODE -ne 0) { throw "Failed to create process test raster" }

    if (Test-Path -LiteralPath $out) { Remove-Item -LiteralPath $out -Force }

    # qgis_process / hake-geodesk-process argument form:
    #   run algorithm -- PARAM=value
    $args = @(
        "run", "gdal:polygonize",
        "--",
        "INPUT=$tif",
        "BAND=1",
        "FIELD=DN",
        "EIGHT_CONNECTEDNESS=false",
        "OUTPUT=$out"
    )
    Write-Host "Running: $processExe $($args -join ' ')"
    $log = Join-Path $ReportDir "process_polygonize.log"
    & $processExe @args *> $log
    $code = $LASTEXITCODE
    Get-Content -LiteralPath $log -ErrorAction SilentlyContinue | Write-Host
    if ($code -ne 0) {
        throw "hake-geodesk-process exited $code (see process_polygonize.log)"
    }
    if (-not (Test-Path -LiteralPath $out)) {
        throw "process OUTPUT GeoPackage missing: $out"
    }

    $validate = @"
from osgeo import ogr
ogr.UseExceptions()
ds = ogr.Open(r'$($out.Replace('\','\\'))')
assert ds is not None, 'open failed'
layer = ds.GetLayerByName('OUTPUT') or ds.GetLayer(0)
assert layer is not None
count = layer.GetFeatureCount()
print('layer', layer.GetName(), 'features', count)
assert count >= 2, count
"@
    $valPath = Join-Path $work "validate_out.py"
    Set-Content -LiteralPath $valPath -Value $validate -Encoding UTF8
    & $python -u $valPath
    if ($LASTEXITCODE -ne 0) { throw "process output GeoPackage validation failed" }
    Write-Host "hake-geodesk-process polygonize PASSED"
}

function Test-PortableZip {
    Write-Section "Portable ZIP extract and runtime smoke"
    $zip = Get-CPackZip
    $tempBase = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { [System.IO.Path]::GetTempPath() }
    $portableTarget = Join-Path $tempBase "HakeGeoDeskPortable"
    if (Test-Path -LiteralPath $portableTarget) {
        Remove-Item -Recurse -Force -LiteralPath $portableTarget
    }
    New-Item -ItemType Directory -Force -Path $portableTarget | Out-Null
    Write-Host "Extracting $($zip.FullName) to $portableTarget ..."
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [System.IO.Compression.ZipFile]::ExtractToDirectory($zip.FullName, $portableTarget)

    $portableRoot = $portableTarget
    if (-not (Test-Path -LiteralPath (Join-Path $portableRoot "bin\hake-geodesk.exe"))) {
        $sub = Get-ChildItem -LiteralPath $portableTarget -Directory | Select-Object -First 1
        if ($sub -and (Test-Path -LiteralPath (Join-Path $sub.FullName "bin\hake-geodesk.exe"))) {
            $portableRoot = $sub.FullName
        }
    }
    if (-not (Test-Path -LiteralPath (Join-Path $portableRoot "bin\hake-geodesk.exe"))) {
        throw "Portable package extraction did not yield bin\hake-geodesk.exe"
    }

    Write-Host "Portable package root verified at $portableRoot"
    Invoke-AppExeVersion -InstallRoot $portableRoot -Label "portable"
    Invoke-Smoke -InstallRoot $portableRoot -Label "portable-smoke"
    Write-Host "Portable package smoke PASSED"
}

# ---- main ----
Write-Host "BuildDir=$BuildDir"
Write-Host "RepoRoot=$RepoRoot"
Write-Host "ReportDir=$ReportDir"
Write-Host "Mode=$Mode"

$installRoot = $null

if ($Mode -in @("All", "AssertZip")) {
    Assert-ZipContents
}

if ($Mode -in @("All", "Install", "Runtime")) {
    $null = Install-NsisArtifact
    $installRoot = Find-InstallRoot
    Find-ProjDb $installRoot | Out-Null
    Set-Content -LiteralPath (Join-Path $ReportDir "install_root.txt") -Value $installRoot -Encoding UTF8
}

if ($Mode -in @("All", "Smoke", "Process", "Runtime")) {
    if (-not $installRoot) {
        $saved = Join-Path $ReportDir "install_root.txt"
        if (Test-Path -LiteralPath $saved) {
            $installRoot = (Get-Content -LiteralPath $saved -Raw).Trim()
        }
        else {
            $installRoot = Find-InstallRoot
        }
    }
}

if ($Mode -in @("All", "Smoke", "Runtime")) {
    Invoke-Smoke -InstallRoot $installRoot -Label "launcher-env"
    Invoke-Smoke -InstallRoot $installRoot -Label "isolated-env" -Isolated
}

if ($Mode -in @("All", "Process", "Runtime")) {
    Invoke-ProcessPolygonize -InstallRoot $installRoot
}

if ($Mode -in @("All", "Portable", "Runtime")) {
    Test-PortableZip
}

Write-Section "All requested modes completed successfully"
exit 0
