@echo off
setlocal enabledelayedexpansion

echo Building...

:: Ensure build directory exists
if not exist "build" mkdir "build"

:: Determine architecture (default to x64)
set "ARCH_FLAG=-m64"
set "LIB_PATH=%CD%\raylib\lib\x64"
set "OUTPUT=build/game-x64.exe"

if /I "%1"=="x86" (
    set "ARCH_FLAG=-m32"
    set "LIB_PATH=%CD%\raylib\lib\x86"
    set "OUTPUT=build/game-x86.exe"
)

:: Check for editor mode
if /I "%2"=="editor" set "BUILD_FLAG=-DEDITOR_BUILD" & set "OUTPUT=%OUTPUT:.exe=-editor.exe%"

:: Check for debug mode
if /I "%3"=="debug" set "DEBUG_FLAG=-DDEBUG -g -O0" & set "OUTPUT=%OUTPUT:.exe=-DEBUG.exe%"

:: Check if g++ exists in the system path
where g++ >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo ERROR: g++ not found in system PATH. Install MinGW-w64 and add it to PATH.
    exit /b 1
)

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
    -lraylib -lstdc++ -lopengl32 -lgdi32 -lwinmm -static -static-libgcc -static-libstdc++ 

:: Check if the build was successful
if %ERRORLEVEL% neq 0 (
    echo Build failed!
    exit /b %ERRORLEVEL%
) else (
    echo Build succeeded!
    if not exist "build\res" mkdir "build\res"
    xcopy /E /I /Y "res" "build\res"
)
