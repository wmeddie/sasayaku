@echo off
echo === Sasayaku Windows Build ===
echo.

set PROJECT_ROOT=%~dp0..
set WHISPER_DIR=%PROJECT_ROOT%\whisper.cpp

REM Step 1: Build whisper.cpp
if not exist "%WHISPER_DIR%\build\src\whisper.lib" (
    echo --- Building whisper.cpp ---
    if not exist "%WHISPER_DIR%" (
        echo Error: whisper.cpp not found at %WHISPER_DIR%
        echo Please run: git submodule update --init --recursive
        exit /b 1
    )
    mkdir "%WHISPER_DIR%\build" 2>nul
    cd /d "%WHISPER_DIR%\build"
    cmake .. -DCMAKE_BUILD_TYPE=Release
    cmake --build . --config Release -j %NUMBER_OF_PROCESSORS%
    cd /d "%~dp0"
    echo whisper.cpp built
) else (
    echo whisper.cpp already built, skipping
)

echo.

REM Step 2: Build Sasayaku
echo --- Building Sasayaku ---
mkdir build 2>nul
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release -j %NUMBER_OF_PROCESSORS%
cd ..

if exist "build\Release\sasayaku.exe" (
    echo.
    echo === Build successful ===
    echo Executable: build\Release\sasayaku.exe
    echo.
    echo Before first run, download a whisper model to:
    echo   %%APPDATA%%\Sasayaku\models\
) else (
    echo Build may have failed. Check output above.
)
