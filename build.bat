@echo off
setlocal enabledelayedexpansion

:: Default settings
set "BUILD_FLAG="
set "DEBUG_FLAG="
set "ARCH_FLAG="
set "OUTPUT=build/game.exe"

:: Ensure build directory exists
if not exist "build" mkdir "build"

:: Determine build architecture (default to x64)
if /I "%1"=="x86" (
    set "ARCH_FLAG=-m32"
    set "MINGW_PATH=C:\mingw32\bin"
    set "LIB_PATH=%CD%\raylib\lib\x86"
    set "OUTPUT=build/game-x86.exe"
) else (
    set "ARCH_FLAG=-m64"
    set "MINGW_PATH=C:\mingw64\bin"
    set "LIB_PATH=%CD%\raylib\lib\x64"
    set "OUTPUT=build/game-x64.exe"
)

:: Check for editor mode (pass "editor" as second argument)
if /I "%2"=="editor" (
    set "BUILD_FLAG=-DEDITOR_BUILD"
    set "OUTPUT=%OUTPUT:.exe=-editor.exe%"
)

:: Check for debug mode (pass "debug" as third argument)
if /I "%3"=="debug" (
    set "DEBUG_FLAG=-DDEBUG -g -O0"
    set "OUTPUT=%OUTPUT:.exe=-DEBUG.exe%"
)

echo Using MinGW from: %MINGW_PATH%
echo Building for architecture: %ARCH_FLAG%
echo Build flags: %BUILD_FLAG% %DEBUG_FLAG%
echo Output file: %OUTPUT%

:: Set MinGW path
set "PATH=%MINGW_PATH%;%PATH%"

:: Compile
g++ %BUILD_FLAG% %DEBUG_FLAG% %ARCH_FLAG% ^
    -o %OUTPUT% ^
    -I "%CD%\raylib\include" ^
    -I "%CD%\imgui" ^
    -I "%CD%\raylib-imgui" ^
    src/main.cpp src/memory_arena.cpp src/windows_file_io.cpp src/editor_mode.cpp src/game_storage.cpp src/game_rendering.cpp src/physics.cpp ^
    src/tile_editor.cpp src/ai.cpp src/bullet.cpp src/game_ui.cpp src/entity_helpers.cpp ^
    imgui\imgui.cpp imgui\imgui_draw.cpp imgui\imgui_tables.cpp imgui\imgui_widgets.cpp ^
    raylib-imgui\rlImGui.cpp ^
    -L "%LIB_PATH%" ^
    -lraylib ^
    -lstdc++ ^
    -lopengl32 ^
    -lgdi32 ^
    -lwinmm ^
    -static ^
    -static-libgcc ^
    -static-libstdc++

:: Check if the build was successful
if %ERRORLEVEL% neq 0 (
    echo Build failed!
    exit /b %ERRORLEVEL%
) else (
    echo Build succeeded!
    if not exist "build\res" mkdir "build\res"
    xcopy /E /I /Y "res" "build\res"
)
