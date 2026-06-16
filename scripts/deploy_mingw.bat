@echo off
chcp 65001 >nul
setlocal EnableExtensions EnableDelayedExpansion

rem ============================================================
rem One-click MinGW release package script for ExceptionDumpAnalyzer.
rem Double-click this file to package a portable runtime folder.
rem It reuses a Qt Creator release build under packages\build-* first;
rem if no release executable is found, it builds the project itself.
rem Output folder name: ProjectName_yyyyMMdd_HHmmss
rem ============================================================

set "PROJECT_NAME=ExceptionDumpAnalyzer"
set "QT_BIN=C:\Qt\Qt5.9.0\5.9\mingw53_32\bin"
set "MINGW_BIN=C:\Qt\Qt5.9.0\Tools\mingw530_32\bin"

for %%I in ("%~dp0..") do set "PROJECT_ROOT=%%~fI"
for %%I in ("%PROJECT_ROOT%\..") do set "PROJECT_PARENT=%%~fI"
set "PROJECT_FILE=%PROJECT_ROOT%\%PROJECT_NAME%.pro"
set "PACKAGE_ROOT=%PROJECT_ROOT%\packages"

for /f %%I in ('powershell -NoProfile -ExecutionPolicy Bypass -Command "Get-Date -Format yyyyMMdd_HHmmss"') do set "STAMP=%%I"
if not defined STAMP (
    echo Failed to generate package timestamp.
    goto :error
)

set "BUILD_DIR=%PACKAGE_ROOT%\build-%PROJECT_NAME%-script-%STAMP%"
set "PACKAGE_DIR=%PACKAGE_ROOT%\%PROJECT_NAME%_%STAMP%"
set "QMAKE=%QT_BIN%\qmake.exe"
set "WINDEPLOYQT=%QT_BIN%\windeployqt.exe"
set "MAKE=%MINGW_BIN%\mingw32-make.exe"
set "ARM_ADDR2LINE=arm-none-eabi-addr2line.exe"
set "ARM_TOOLCHAIN_BIN="
set "BUILT_EXE="
set "NEED_BUILD=1"

echo ============================================================
echo Project : %PROJECT_NAME%
echo Root    : %PROJECT_ROOT%
echo Output  : %PACKAGE_DIR%
echo ============================================================

if not exist "%PROJECT_FILE%" (
    echo Project file not found: %PROJECT_FILE%
    goto :error
)

if not exist "%QMAKE%" (
    echo qmake not found: %QMAKE%
    goto :error
)

if not exist "%MAKE%" (
    echo mingw32-make not found: %MAKE%
    goto :error
)

if not exist "%WINDEPLOYQT%" (
    echo windeployqt not found: %WINDEPLOYQT%
    goto :error
)

if exist "D:\D-install\arm-none-eabi\14.3 rel1\bin\%ARM_ADDR2LINE%" (
    set "ARM_TOOLCHAIN_BIN=D:\D-install\arm-none-eabi\14.3 rel1\bin"
)

if not defined ARM_TOOLCHAIN_BIN if exist "C:\NXP\S32DS_ARM_v2.2\S32DS\build_tools\gcc_v4.9\gcc-arm-none-eabi-4_9\bin\%ARM_ADDR2LINE%" (
    set "ARM_TOOLCHAIN_BIN=C:\NXP\S32DS_ARM_v2.2\S32DS\build_tools\gcc_v4.9\gcc-arm-none-eabi-4_9\bin"
)

if not defined ARM_TOOLCHAIN_BIN (
    for /f "delims=" %%I in ('where %ARM_ADDR2LINE% 2^>nul') do (
        if not defined ARM_TOOLCHAIN_BIN for %%J in ("%%~fI") do set "ARM_TOOLCHAIN_BIN=%%~dpJ"
    )
)

if not defined ARM_TOOLCHAIN_BIN (
    echo %ARM_ADDR2LINE% not found.
    echo Please install Arm GNU Toolchain or add its bin directory to PATH.
    goto :error
)

echo.
echo [1/5] Preparing package folder and checking Qt Creator release output...
mkdir "%PACKAGE_ROOT%" 2>nul
if exist "%PACKAGE_DIR%" rmdir /s /q "%PACKAGE_DIR%"
mkdir "%PACKAGE_DIR%" || goto :error

for /d %%D in ("%PACKAGE_ROOT%\build-%PROJECT_NAME%-*-Release") do (
    if exist "%%~fD\release\%PROJECT_NAME%.exe" (
        set "BUILT_EXE=%%~fD\release\%PROJECT_NAME%.exe"
        set "NEED_BUILD=0"
    )
)

if "%NEED_BUILD%"=="0" (
    echo Found Qt Creator release executable:
    echo %BUILT_EXE%
    echo Skip script build.
) else (
    echo No Qt Creator release executable found under:
    echo %PACKAGE_ROOT%\build-%PROJECT_NAME%-*-Release

    echo.
    echo [2/5] Running qmake...
    if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
    mkdir "%BUILD_DIR%" || goto :error
    pushd "%BUILD_DIR%" || goto :error
    "%QMAKE%" "%PROJECT_FILE%" CONFIG+=release CONFIG-=debug
    if errorlevel 1 (
        popd
        goto :error
    )

    echo.
    echo [3/5] Building release executable...
    "%MAKE%" -j4
    if errorlevel 1 (
        popd
        goto :error
    )
    popd

    set "BUILT_EXE="
    for /d %%D in ("%PACKAGE_ROOT%\build-%PROJECT_NAME%-*-Release") do (
        if exist "%%~fD\release\%PROJECT_NAME%.exe" (
            set "BUILT_EXE=%%~fD\release\%PROJECT_NAME%.exe"
        )
    )
)

if not exist "%BUILT_EXE%" (
    echo Built executable not found.
    goto :error
)

copy /Y "%BUILT_EXE%" "%PACKAGE_DIR%\" >nul || goto :error

echo.
echo [4/5] Deploying Qt runtime and plugins...
"%WINDEPLOYQT%" --release --compiler-runtime "%PACKAGE_DIR%\%PROJECT_NAME%.exe"
if errorlevel 1 goto :error

if exist "%PROJECT_ROOT%\lib" (
    echo Copying project runtime DLLs from lib...
    for /r "%PROJECT_ROOT%\lib" %%F in (*.dll) do (
        set "DLL_PATH=%%~fF"
        if /I "!DLL_PATH:%PROJECT_ROOT%\lib\zlg\kerneldlls\=!"=="!DLL_PATH!" (
            copy /Y "%%~fF" "%PACKAGE_DIR%\" >nul
        )
    )
)

if exist "%PROJECT_ROOT%\lib\zlg\kerneldlls" (
    echo Copying ZLG kernel runtime files...
    xcopy /E /I /Y /Q "%PROJECT_ROOT%\lib\zlg\kerneldlls" "%PACKAGE_DIR%\kerneldlls" >nul
)

echo Copying bundled addr2line tool...
set "BUNDLED_TOOL_DIR=%PACKAGE_DIR%\tools\arm-none-eabi\bin"
mkdir "%BUNDLED_TOOL_DIR%" 2>nul
copy /Y "%ARM_TOOLCHAIN_BIN%\%ARM_ADDR2LINE%" "%BUNDLED_TOOL_DIR%\" >nul || goto :error
if exist "%ARM_TOOLCHAIN_BIN%\*.dll" (
    copy /Y "%ARM_TOOLCHAIN_BIN%\*.dll" "%BUNDLED_TOOL_DIR%\" >nul
)

echo.
echo [5/5] Removing compile-time files from package...
for /r "%PACKAGE_DIR%" %%F in (*.o *.obj *.c *.cc *.cpp *.cxx *.h *.hh *.hpp *.moc *.pdb *.ilk *.exp *.lib *.a *.prl *.map) do (
    if exist "%%F" del /f /q "%%F" >nul 2>nul
)

echo Cleaning Qt Creator/script build folders under packages...
for /d %%D in ("%PACKAGE_ROOT%\build-%PROJECT_NAME%-*") do (
    if exist "%%~fD" (
        attrib -R "%%~fD\*" /S /D >nul 2>nul
        rmdir /s /q "%%~fD" >nul 2>nul
        if exist "%%~fD" (
            powershell -NoProfile -ExecutionPolicy Bypass -Command "Remove-Item -LiteralPath '%%~fD' -Recurse -Force -ErrorAction Stop" >nul 2>nul
        )
        if exist "%%~fD" (
            echo Failed to remove build folder:
            echo %%~fD
            goto :error
        )
    )
)

echo Cleaning Qt Creator shadow build folders beside project...
for /d %%D in ("%PROJECT_PARENT%\build-%PROJECT_NAME%-*") do (
    if exist "%%~fD" (
        attrib -R "%%~fD\*" /S /D >nul 2>nul
        rmdir /s /q "%%~fD" >nul 2>nul
        if exist "%%~fD" (
            powershell -NoProfile -ExecutionPolicy Bypass -Command "Remove-Item -LiteralPath '%%~fD' -Recurse -Force -ErrorAction Stop" >nul 2>nul
        )
        if exist "%%~fD" (
            echo Failed to remove Qt Creator shadow build folder:
            echo %%~fD
            goto :error
        )
    )
)

echo.
echo Package completed:
echo %PACKAGE_DIR%
echo.
pause
exit /b 0

:error
echo.
echo Package failed.
if "%NEED_BUILD%"=="1" if exist "%BUILD_DIR%" (
    echo Temporary build folder kept for inspection:
    echo %BUILD_DIR%
)
echo.
pause
exit /b 1
