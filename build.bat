@echo off

set "BUILD_FLAG="
set "DEBUG_FLAG="
set "OUTPUT=build/game.exe"

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

:: Use g++ instead of gcc so C++ linkage and libs are automatically included
g++ %BUILD_FLAG% %DEBUG_FLAG% ^
    -o %OUTPUT% ^
    -I "%CD%\raylib\include" ^
    -I "%CD%/imgui" ^
    -I "%CD%/raylib-imgui" ^
    main.cpp memory_arena.cpp windows_file_io.cpp editor_mode.cpp game_storage.cpp game_rendering.cpp physics.cpp ^
    tile_editor.cpp ai.cpp animation.cpp game_ui.cpp ^
	imgui/imgui.cpp imgui/imgui_draw.cpp imgui/imgui_tables.cpp imgui/imgui_widgets.cpp ^
    raylib-imgui/rlImGui.cpp ^
    -L "%CD%\raylib\lib" ^
    -lraylib ^
    -lstdc++ ^
    -lopengl32 ^
    -lgdi32 ^
    -lwinmm

if %ERRORLEVEL% neq 0 (
    echo Build failed!
) else (
    echo Build succeeded!
)
