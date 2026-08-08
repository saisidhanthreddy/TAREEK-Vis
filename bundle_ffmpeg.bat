@echo off
REM ============================================================================
REM FFmpeg Bundling Script for TAREEK-Vis
REM ============================================================================
REM This script helps bundle FFmpeg with your TAREEK-Vis distribution
REM
REM Instructions:
REM 1. Download FFmpeg from: https://www.gyan.dev/ffmpeg/builds/
REM 2. Extract the archive
REM 3. Update the FFMPEG_SOURCE path below to point to the extracted bin folder
REM 4. Run this script
REM ============================================================================

echo.
echo ========================================
echo  TAREEK-Vis FFmpeg Bundling Tool
echo ========================================
echo.

REM ============================================================================
REM CONFIGURATION - Update this path to your FFmpeg download
REM ============================================================================

REM Default path - adjust this to match your FFmpeg download location
set FFMPEG_SOURCE=C:\ffmpeg\bin\ffmpeg.exe

REM Alternative: prompt user for path
if not exist "%FFMPEG_SOURCE%" (
    echo WARNING: Default FFmpeg path not found: %FFMPEG_SOURCE%
    echo.
    echo Please enter the full path to ffmpeg.exe:
    echo Example: C:\Downloads\ffmpeg-6.1-full_build\bin\ffmpeg.exe
    echo.
    set /p FFMPEG_SOURCE="Path to ffmpeg.exe: "
)

REM ============================================================================
REM Validate source
REM ============================================================================

if not exist "%FFMPEG_SOURCE%" (
    echo.
    echo ERROR: FFmpeg not found at: %FFMPEG_SOURCE%
    echo.
    echo Please download FFmpeg from:
    echo   https://www.gyan.dev/ffmpeg/builds/
    echo.
    echo Then update the path in this script or enter it when prompted.
    echo.
    pause
    exit /b 1
)

echo Found FFmpeg: %FFMPEG_SOURCE%
echo.

REM ============================================================================
REM Create directory structure
REM ============================================================================

echo Creating ffmpeg directory...
if not exist "ffmpeg" mkdir "ffmpeg"

REM ============================================================================
REM Copy FFmpeg
REM ============================================================================

echo Copying ffmpeg.exe...
copy /Y "%FFMPEG_SOURCE%" "ffmpeg\ffmpeg.exe" >nul

if not exist "ffmpeg\ffmpeg.exe" (
    echo.
    echo ERROR: Failed to copy ffmpeg.exe
    echo.
    pause
    exit /b 1
)

REM ============================================================================
REM Copy to build directory
REM ============================================================================

echo.
echo Copying to build directory...
if exist "build" (
    if not exist "build\ffmpeg" mkdir "build\ffmpeg"
    copy /Y "ffmpeg\ffmpeg.exe" "build\ffmpeg\ffmpeg.exe" >nul
    if exist "build\ffmpeg\ffmpeg.exe" (
        echo FFmpeg copied to build\ffmpeg\ffmpeg.exe
    ) else (
        echo WARNING: Failed to copy to build directory
    )
) else (
    echo WARNING: build directory not found - skipping copy
    echo Run build.bat first to create the build directory
)

REM ============================================================================
REM Verify and report
REM ============================================================================

echo.
echo ========================================
echo  FFmpeg Bundled Successfully!
echo ========================================
echo.
echo Source location: %CD%\ffmpeg\ffmpeg.exe
if exist "build\ffmpeg\ffmpeg.exe" (
    echo Build location: %CD%\build\ffmpeg\ffmpeg.exe
)
echo.

REM Get file size
for %%A in ("ffmpeg\ffmpeg.exe") do (
    set SIZE=%%~zA
    set /A SIZE_MB=%%~zA/1024/1024
)

echo File size: %SIZE_MB% MB
echo.

REM Test FFmpeg
echo Testing FFmpeg...
"ffmpeg\ffmpeg.exe" -version >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo FFmpeg test: PASSED
    echo.
    echo Version info:
    "ffmpeg\ffmpeg.exe" -version 2>nul | findstr "ffmpeg version"
) else (
    echo FFmpeg test: WARNING - Could not execute FFmpeg
)

echo.
echo ========================================
echo  Next Steps
echo ========================================
echo.
echo 1. TAREEK-Vis.exe will automatically use the bundled FFmpeg
echo 2. Test video recording: Tools ^> Record Video...
echo 3. After each build.bat, FFmpeg is already in build\ffmpeg\
echo 4. When distributing TAREEK-Vis, include the ffmpeg\ folder
echo.
echo Distribution structure:
echo   TAREEK-Vis\
echo   ├── TAREEK-Vis.exe
echo   ├── ffmpeg\
echo   │   └── ffmpeg.exe
echo   └── (Qt DLLs and other files)
echo.
echo NOTE: If you rebuild with build.bat, run this script again
echo       or manually copy ffmpeg\ to build\ffmpeg\
echo.

pause
