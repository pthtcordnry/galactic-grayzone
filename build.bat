@echo off

set "BUILD_FLAG="
set "DEBUG_FLAG="
set "OUTPUT=build/game.exe"

:: Ensure build directory exists
if not exist "build" mkdir "build"

:: Check for editor mode
if /I "%1"=="editor" (
    set "BUILD_FLAG=-DEDITOR_BUILD"
    set "OUTPUT=%OUTPUT:.exe=-editor.exe%"
)

:: Check for debug mode (pass e.g. "debug" as second parameter)
if /I "%2"=="debug" (
    set "DEBUG_FLAG=-DDEBUG -g -O0"
    set "OUTPUT=%OUTPUT:.exe=-DEBUG.exe%"
)

echo Building with flags: %BUILD_FLAG% %DEBUG_FLAG%
echo Output file: %OUTPUT%

:: Use g++ for C++ linkage; source files are now in the src folder.
g++ %BUILD_FLAG% %DEBUG_FLAG% ^
    -o %OUTPUT% ^
    -I "%CD%\raylib\include" ^
    -I "%CD%\imgui" ^
    -I "%CD%\raylib-imgui" ^
    src/main.cpp src/memory_arena.cpp src/windows_file_io.cpp src/editor_mode.cpp src/game_storage.cpp src/game_rendering.cpp src/physics.cpp ^
    src/tile_editor.cpp src/ai.cpp src/bullet.cpp src/game_ui.cpp src/entity_helpers.cpp ^
    imgui\imgui.cpp imgui\imgui_draw.cpp imgui\imgui_tables.cpp imgui\imgui_widgets.cpp ^
    raylib-imgui\rlImGui.cpp ^
    -L "%CD%\raylib\lib" ^
    -lraylib ^
    -lstdc++ ^
    -lopengl32 ^
    -lgdi32 ^
    -lwinmm ^
    -static ^
    -static-libgcc ^
    -static-libstdc++

if %ERRORLEVEL% neq 0 (
    echo Build failed!
) else (
    echo Build succeeded!
    if not exist "build\res" mkdir "build\res"
    xcopy /E /I /Y "res" "build\res"
)