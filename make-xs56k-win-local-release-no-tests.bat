@echo off
rem XS56K - Editeur pour les sampleurs AKAI S5000/S6000
rem Copyright (C) 2026 https://github.com/xplorer2716
rem SPDX-License-Identifier: AGPL-3.0-or-later
rem Local Windows x64 GUI build. [RQ-BLD-002, RQ-BLD-007]
rem
rem Deliberately Release with BUILD_TESTS=OFF: this script exists to
rem produce a binary quickly, so it fetches and compiles neither Catch2 nor the
rem test suites (not yet ported anyway - see PLAN-BLD-001). Two consequences
rem worth stating out loud to whoever runs it, since both are silent otherwise:
rem   * no test is built or run here;
rem   * XS56KApp is a minimal placeholder GUI (juce::DocumentWindow), not the
rem     real editor UI - see AGENTS.md.
rem The CI covers the full matrix: the windows-x64-*-canary/preprod/prod
rem workflows build with -DBUILD_APP=ON across Debug/Release.
rem [RQ-BLD-002, RQ-BLD-007, ADR-BLD-003]
rem
rem Usage: make-xs56k-win-local-release-no-tests.bat [clean]
rem   clean - wipe the local build directory before reconfiguring/building.

setlocal

set "CLEAN="
if /i "%~1"=="clean" set "CLEAN=1"

echo.
echo ============================================================
echo  Release build, tests DISABLED.
echo.
echo  This script only produces XS56K.exe. It does not build or
echo  run any test, and XS56KApp is a minimal placeholder GUI,
echo  not the real editor UI.
echo.
echo  For the full matrix, push the branch or run the
echo  windows-x64-*-canary / preprod / prod workflows from the
echo  Actions tab.
echo ============================================================
echo.

set "SOURCE_DIR=%~dp0juce"
set "BUILD_DIR=%SOURCE_DIR%\build-win-local"
set "ARCHITECTURE=x64"
set "CONFIGURATION=Release"
set "CMAKE_EXE="
set "VSWHERE_EXE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"

where cmake.exe >nul 2>&1
if not errorlevel 1 set "CMAKE_EXE=cmake.exe"

if not defined CMAKE_EXE if exist "%ProgramFiles%\CMake\bin\cmake.exe" (
    set "CMAKE_EXE=%ProgramFiles%\CMake\bin\cmake.exe"
)

if not defined CMAKE_EXE (
    if exist "%VSWHERE_EXE%" (
        for /f "usebackq delims=" %%I in (`"%VSWHERE_EXE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.CMake.Project -find Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`) do set "CMAKE_EXE=%%I"
    )
)

if not defined CMAKE_EXE (
    echo Error: CMake was not found.
    echo Install CMake 3.22 or later, or add the Visual Studio CMake component.
    exit /b 1
)

if defined CLEAN (
    if exist "%BUILD_DIR%" (
        echo Cleaning "%BUILD_DIR%"...
        rmdir /s /q "%BUILD_DIR%"
    )
)

echo Configuring XS56K for %ARCHITECTURE% %CONFIGURATION%...
"%CMAKE_EXE%" -S "%SOURCE_DIR%" -B "%BUILD_DIR%" -A %ARCHITECTURE% -DBUILD_APP=ON -DBUILD_TESTS=OFF
if errorlevel 1 exit /b %errorlevel%

echo Building XS56K...
"%CMAKE_EXE%" --build "%BUILD_DIR%" --config %CONFIGURATION% --parallel
if errorlevel 1 exit /b %errorlevel%

echo Build completed. XS56K.exe is under "%BUILD_DIR%\app\XS56KApp_artefacts\%CONFIGURATION%".
exit /b 0
