@echo off
setlocal enabledelayedexpansion

REM ============================================================================
REM RichEditor - MSVC Build Script
REM Builds RichEditor using Microsoft Visual C++ compiler (cl.exe)
REM ============================================================================
REM
REM Requirements:
REM   - Visual Studio 2019 or newer (Community Edition is free)
REM   - Or: Visual Studio Build Tools (smaller, command-line only)
REM
REM Usage:
REM   build_msvc.bat          - Build optimized release version
REM   build_msvc.bat debug    - Build debug version with symbols
REM   build_msvc.bat clean    - Clean build artifacts
REM
REM Output:
REM   msvc\RichEditor.exe     - Release executable (size-optimized, 252 KB)
REM   msvc\RichEditor_dbg.exe - Debug version (if built with "debug" option)
REM

REM Parse command-line arguments
set BUILD_TYPE=release
if "%1"=="debug" set BUILD_TYPE=debug
if "%1"=="clean" goto :clean

REM Ensure msvc directory exists
if not exist msvc mkdir msvc

REM ============================================================================
REM Find Visual Studio Installation
REM ============================================================================
echo Detecting Visual Studio installation...
echo.

REM Try to find vswhere.exe (comes with VS 2017+)
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"

if exist "%VSWHERE%" goto :vswhere_found

echo ERROR: vswhere.exe not found at:
echo   "%VSWHERE%"
echo.
echo Please install Visual Studio 2019 or newer with C++ support.
echo Download from: https://visualstudio.microsoft.com/downloads/
exit /b 1

:vswhere_found
REM Query for Visual Studio Build Tools with C++ compiler
set "TEMP_FILE=%TEMP%\vswhere_result.txt"
"%VSWHERE%" -latest -products Microsoft.VisualStudio.Product.BuildTools -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath > "%TEMP_FILE%" 2>nul

set /p VS_PATH=<"%TEMP_FILE%"
del "%TEMP_FILE%" 2>nul

if not "%VS_PATH%"=="" goto :vs_found

REM Try Community/Professional/Enterprise editions
"%VSWHERE%" -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath > "%TEMP_FILE%" 2>nul
set /p VS_PATH=<"%TEMP_FILE%"
del "%TEMP_FILE%" 2>nul

if not "%VS_PATH%"=="" goto :vs_found

REM Try hardcoded common paths
if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Community" (
    set "VS_PATH=%ProgramFiles%\Microsoft Visual Studio\2022\Community"
    goto :vs_found
)
if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools" (
    set "VS_PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools"
    goto :vs_found
)
if exist "%ProgramFiles%\Microsoft Visual Studio\2019\Community" (
    set "VS_PATH=%ProgramFiles%\Microsoft Visual Studio\2019\Community"
    goto :vs_found
)

echo ERROR: Visual Studio installation not found.
echo.
echo Please install Visual Studio 2019+ or Build Tools with C++ support.
echo Download from: https://visualstudio.microsoft.com/downloads/
exit /b 1

:vs_found
REM Check that VS_PATH is not empty
if "%VS_PATH%"=="" (
    echo ERROR: Could not determine Visual Studio installation path.
    exit /b 1
)

echo Found Visual Studio at: !VS_PATH!
echo.

REM ============================================================================
REM Initialize Visual Studio Environment
REM ============================================================================
echo Initializing Visual Studio environment...

REM Try VsDevCmd.bat first (preferred for VS 2017+)
if exist "!VS_PATH!\Common7\Tools\VsDevCmd.bat" (
    set "INIT_SCRIPT=!VS_PATH!\Common7\Tools\VsDevCmd.bat"
    set "INIT_ARGS=-arch=x64 -host_arch=x64"
    goto :run_init_script
)

if exist "!VS_PATH!\VC\Auxiliary\Build\vcvarsall.bat" (
    set "INIT_SCRIPT=!VS_PATH!\VC\Auxiliary\Build\vcvarsall.bat"
    set "INIT_ARGS=x64"
    goto :run_init_script
)

echo ERROR: Neither VsDevCmd.bat nor vcvarsall.bat found.
echo Visual Studio installation may be corrupted or incomplete.
exit /b 1

:run_init_script
pushd "!VS_PATH!" && (
    if exist "Common7\Tools\VsDevCmd.bat" (
        call "Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul 2>&1
    ) else if exist "VC\Auxiliary\Build\vcvarsall.bat" (
        call "VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
    )
    popd
)

REM Verify compiler is available
where cl.exe >nul 2>&1
if errorlevel 1 (
    echo ERROR: cl.exe not found in PATH after environment initialization.
    echo Visual Studio C++ tools may not be properly installed.
    exit /b 1
)

echo.
echo [SUCCESS] Environment initialized successfully  
echo Compiler: cl.exe found in PATH
echo.

REM ============================================================================
REM Build Configuration
REM ============================================================================

if /i "%BUILD_TYPE%"=="debug" goto :config_debug
if /i "%BUILD_TYPE%"=="release" goto :config_release
echo ERROR: Unknown BUILD_TYPE: %BUILD_TYPE%
exit /b 1

:config_debug
set OUTPUT=msvc\RichEditor_dbg.exe
set CFLAGS=/std:c++14 /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS /D_CRT_NON_CONFORMING_WCSTOK /Zi /Od /W3 /MTd /EHsc /DEBUG
set LDFLAGS=/DEBUG /SUBSYSTEM:WINDOWS /ENTRY:wWinMainCRTStartup
echo Building DEBUG version...
goto :config_done

:config_release
set OUTPUT=msvc\RichEditor.exe
set CFLAGS=/std:c++14 /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS /D_CRT_NON_CONFORMING_WCSTOK /O1 /Os /Gy /GL /W3 /MT /GS- /GR- /Zc:inline /Zc:threadSafeInit- /EHsc /DNDEBUG
set LDFLAGS=/LTCG /OPT:REF /OPT:ICF /SUBSYSTEM:WINDOWS /ENTRY:wWinMainCRTStartup
echo Building RELEASE version (size-optimized)...
goto :config_done

:config_done
set LIBS=user32.lib gdi32.lib kernel32.lib comctl32.lib comdlg32.lib ole32.lib oleaut32.lib shell32.lib shlwapi.lib version.lib

REM ============================================================================
REM Compile Resources
REM ============================================================================
echo.
echo Compiling resources...
rc.exe /nologo /fo msvc\resource.res src\resource.rc
if errorlevel 1 (
    echo ERROR: Resource compilation failed.
    exit /b 1
)

REM ============================================================================
REM Compile and Link
REM ============================================================================
echo.
echo Compiling source code...
echo.

cl.exe /nologo %CFLAGS% /c src\main.cpp /Fo:msvc\main.obj
if errorlevel 1 (
    echo ERROR: Compilation failed.
    exit /b 1
)

echo.
echo Linking...
link.exe /nologo %LDFLAGS% msvc\main.obj msvc\resource.res %LIBS% /OUT:%OUTPUT%
if errorlevel 1 (
    echo ERROR: Linking failed.
    exit /b 1
)

REM ============================================================================
REM Success - Show Results
REM ============================================================================
echo.
echo =========================================
echo Build complete: %OUTPUT%
if "%BUILD_TYPE%"=="release" (
    echo Optimized for size
)
if "%BUILD_TYPE%"=="debug" (
    echo Debug version with symbols
)
for %%A in (%OUTPUT%) do echo Size: %%~zA bytes
echo =========================================
echo.

REM Size comparison if MinGW version exists
if exist RichEditor.exe (
    echo Comparison with MinGW build:
    for %%A in (RichEditor.exe) do echo   MinGW:  %%~zA bytes
    for %%A in (%OUTPUT%) do echo   MSVC:   %%~zA bytes
    echo.
)

goto :eof

REM ============================================================================
REM Clean Target
REM ============================================================================
:clean
echo Cleaning MSVC build artifacts...
if exist msvc\main.obj del msvc\main.obj
if exist msvc\resource.res del msvc\resource.res
if exist msvc\RichEditor.exe del msvc\RichEditor.exe
if exist msvc\RichEditor_dbg.exe del msvc\RichEditor_dbg.exe
if exist msvc\RichEditor.pdb del msvc\RichEditor.pdb
if exist msvc\RichEditor_dbg.pdb del msvc\RichEditor_dbg.pdb
if exist msvc\vc*.pdb del msvc\vc*.pdb
echo Clean complete.
goto :eof
