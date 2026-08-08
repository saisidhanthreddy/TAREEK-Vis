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

echo Copying MinGW/UCRT64 runtime DLLs...
copy /y "C:\msys64\ucrt64\bin\libgcc_s_seh-1.dll" "%DIST_DIR%\" >nul
copy /y "C:\msys64\ucrt64\bin\libstdc++-6.dll" "%DIST_DIR%\" >nul
copy /y "C:\msys64\ucrt64\bin\libwinpthread-1.dll" "%DIST_DIR%\" >nul
copy /y "C:\msys64\ucrt64\bin\zlib1.dll" "%DIST_DIR%\" >nul

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
