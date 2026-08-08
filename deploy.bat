@echo off
REM Assembles a self-contained, redistributable TAREEK-Vis folder in dist\TAREEK-Vis.
REM Run this AFTER a successful Release build (build.bat).
REM The resulting folder needs no Qt, no compiler, and no MSYS2 on the target machine.

setlocal

set PATH=C:\msys64\ucrt64\bin;%PATH%

set PROJECT_DIR=%~dp0
set BUILD_DIR=%PROJECT_DIR%build
set DIST_DIR=%PROJECT_DIR%dist\TAREEK-Vis

if not exist "%BUILD_DIR%\TAREEK-Vis.exe" (
    echo Build not found at %BUILD_DIR%\TAREEK-Vis.exe
    echo Run build.bat first.
    exit /b 1
)

echo Cleaning previous dist...
if exist "%PROJECT_DIR%dist" rmdir /s /q "%PROJECT_DIR%dist"
mkdir "%DIST_DIR%"

echo Copying executable...
copy /y "%BUILD_DIR%\TAREEK-Vis.exe" "%DIST_DIR%\" >nul

echo Running windeployqt...
windeployqt6.exe --release --no-translations "%DIST_DIR%\TAREEK-Vis.exe"
if errorlevel 1 (
    echo windeployqt failed!
    exit /b 1
)

echo Resolving and copying UCRT64 runtime DLLs (ldd, incl. ICU, transitively)...
C:\msys64\usr\bin\bash.exe -lc "set -e; cd '%DIST_DIR:\=/%'; changed=1; while [ \"$changed\" = 1 ]; do changed=0; for f in $(find . -iname '*.dll' -o -iname 'TAREEK-Vis.exe'); do for dep in $(ldd \"$f\" 2>/dev/null | grep -oi '/ucrt64/[^ ]*\.dll'); do base=$(basename \"$dep\"); if [ ! -f \"./$base\" ]; then cp -u \"$dep\" .; changed=1; fi; done; done; done"
if errorlevel 1 (
    echo Failed to resolve/copy UCRT64 runtime DLLs via ldd!
    exit /b 1
)

if exist "%PROJECT_DIR%ffmpeg\ffmpeg.exe" (
    echo Bundling FFmpeg...
    mkdir "%DIST_DIR%\ffmpeg"
    copy /y "%PROJECT_DIR%ffmpeg\ffmpeg.exe" "%DIST_DIR%\ffmpeg\" >nul
) else (
    echo FFmpeg not found at %PROJECT_DIR%ffmpeg\ffmpeg.exe - skipping.
    echo Video recording will require system FFmpeg on the target machine.
)

echo.
echo Done. Self-contained app is at: %DIST_DIR%
echo This folder can be zipped and shared directly, or wrapped by installer\TAREEK-Vis.iss.

endlocal
