@echo off

set "BUILD_FLAG="
set "OUTPUT=game.exe"

if /I "%1"=="editor" (
    set "BUILD_FLAG=-DEDITOR_BUILD"
    set "OUTPUT=game-editor.exe"
)

echo Building with flag: %BUILD_FLAG%
echo Output file: %OUTPUT%

gcc %BUILD_FLAG% -o %OUTPUT% main.cpp memory_arena.cpp windows_file_io.cpp editor_mode.cpp game_ui.cpp -I "%CD%\raylib\include" -L "%CD%\raylib\lib" -lraylib -lopengl32 -lgdi32 -lwinmm

if %ERRORLEVEL% neq 0 (
    echo Build failed!
) else (
    echo Build succeeded!
)