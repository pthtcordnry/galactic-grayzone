@echo off

set "BUILD_FLAG="
set "DEBUG_FLAG="
set "OUTPUT=game.exe"

:: Check for editor mode
if /I "%1"=="editor" (
    set "BUILD_FLAG=-DEDITOR_BUILD"
    set "OUTPUT=game-editor.exe"
)

:: Check for debug mode (pass e.g. "debug" as second parameter)
if /I "%2"=="debug" (
    set "DEBUG_FLAG=-DDEBUG -g -O0"
    set "OUTPUT=%OUTPUT:.exe=-debug.exe%"
)

echo Building with flags: %BUILD_FLAG% %DEBUG_FLAG%
echo Output file: %OUTPUT%

gcc %BUILD_FLAG% %DEBUG_FLAG% -o %OUTPUT% -I "%CD%\raylib\include" -I "%CD%/imgui" -I "%CD%/raylib-imgui" main.cpp memory_arena.cpp windows_file_io.cpp editor_mode.cpp imgui/imgui.cpp imgui/imgui_draw.cpp imgui/imgui_tables.cpp imgui/imgui_widgets.cpp raylib-imgui/rlImGui.cpp -L "%CD%\raylib\lib" -lraylib -lstdc++ -lopengl32 -lgdi32 -lwinmm

if %ERRORLEVEL% neq 0 (
    echo Build failed!
) else (
    echo Build succeeded!
)