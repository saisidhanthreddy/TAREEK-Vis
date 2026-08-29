@echo off
REM TAREEK-Vis Build Script for Windows Command Prompt/PowerShell
REM Ensures correct MSYS2 UCRT64 environment for building

setlocal

REM Set UCRT64 as primary in PATH (required for Qt tools and runtime)
set PATH=C:\msys64\ucrt64\bin;%PATH%

REM Project directory
set PROJECT_DIR=%~dp0
set BUILD_DIR=%PROJECT_DIR%build

REM Parse arguments
set CLEAN=false
set CONFIG=Release

:parse_args
if "%~1"=="" goto done_args
if "%~1"=="--clean" set CLEAN=true
if "%~1"=="--debug" set CONFIG=Debug
if "%~1"=="--help" goto show_help
shift
goto parse_args

:show_help
echo Usage: build.bat [options]
echo.
echo Options:
echo   --clean    Remove build directory and rebuild from scratch
echo   --debug    Build in Debug mode (default: Release)
echo   --help     Show this help message
exit /b 0

:done_args

REM Clean build if requested
if "%CLEAN%"=="true" (
    echo Cleaning build directory...
    if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
)

REM A CMake cache records the absolute source path it was generated for. If the
REM checkout has since been moved or renamed, CMake aborts with "The source
REM directory ... does not exist" instead of reconfiguring. Detect the mismatch
REM and discard the stale cache so the build recovers on its own.
REM PROJECT_DIR from %~dp0 carries a trailing backslash; strip it to compare.
set "SOURCE_DIR=%PROJECT_DIR:~0,-1%"
if exist "%BUILD_DIR%\CMakeCache.txt" call :check_stale_cache

REM Create build directory if needed
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
cd /d "%BUILD_DIR%"

REM Configure if needed
if not exist "Makefile" (
    echo Configuring CMake...
    cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=%CONFIG% "%SOURCE_DIR%"
    if errorlevel 1 (
        echo CMake configuration failed!
        exit /b 1
    )
)

REM Build
echo Building TAREEK-Vis (%CONFIG%)...
mingw32-make -j4

if errorlevel 1 (
    echo.
    echo Build failed!
    exit /b 1
) else (
    echo.
    echo Build successful!
    echo Executable: %BUILD_DIR%\TAREEK-Vis.exe
)

endlocal
exit /b 0

REM ---------------------------------------------------------------------------
REM Discard the build directory when its CMake cache was generated for a
REM different source path. CMake stores forward slashes; %~dp0 gives
REM backslashes, so normalize before comparing. Comparison is case-insensitive
REM (/i) because Windows paths are.
:check_stale_cache
for /f "tokens=1,* delims==" %%a in (
    'findstr /b /c:"CMAKE_HOME_DIRECTORY:INTERNAL=" "%BUILD_DIR%\CMakeCache.txt"'
) do set "CACHED_SOURCE_DIR=%%b"
if not defined CACHED_SOURCE_DIR exit /b 0
set "CACHED_NORM=%CACHED_SOURCE_DIR:/=\%"
if /i "%CACHED_NORM%"=="%SOURCE_DIR%" exit /b 0
echo Build directory was configured for a different source path:
echo   cached: %CACHED_SOURCE_DIR%
echo   actual: %SOURCE_DIR%
echo Discarding stale CMake cache and reconfiguring...
rmdir /s /q "%BUILD_DIR%"
exit /b 0
