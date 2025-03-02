```
# My Awesome Game

A fun and engaging game built in C/C++ with raylib for graphics and dearImGui for the user interface.

## Table of Contents
- [Introduction](#introduction)
- [Features](#features)
- [Prerequisites](#prerequisites)
- [Building](#building)
- [Usage](#usage)
- [Contributing](#contributing)
- [License](#license)
- [Contact](#contact)

## Introduction
My Awesome Game is a fast-paced, interactive game showcasing modern C/C++ programming. It leverages the power of raylib for rendering and dearImGui for the graphical user interface. The project includes both a standard game mode and an editor mode.

## Features
- **RayLib**: Powered by raylib for high-performance graphics.
- **Editor Interface**: Utilizes dearImGui for a modern editor interface.
- **Multiple Build Modes**: Choose between a standard game, an editor version, or enable debug options.

## Prerequisites
Before building the project, ensure you have:
- A C/C++ compiler (e.g., g++ from MinGW or a similar toolchain).
- Windows (since the provided build script is a batch file).

## Building
This project uses a Windows batch script (`build.bat`) to compile and link the game. The script supports several build configurations:

### 1. Standard Game Build
**Command:**  
Simply run the batch file without parameters:
```
build.bat
```
**Output:**  
Compiles the game and produces the executable at `build/game.exe`.

### 2. Editor Mode Build
**Command:**  
Run the batch file with the first parameter set to `editor`:
```
build.bat editor
```
**Output:**  
Sets the flag for editor mode and produces the executable at `build/game-editor.exe`.

### 3. Debug Build
The build script accepts a second parameter to enable debug options (compilation with `-DDEBUG -g -O0`).

- **Standard Game Debug Build:**  
Run the batch file with 'game' as the first parameter and `debug` as the second parameter:
```
build.bat game debug
```
This produces a debug version at `build/game-DEBUG.exe`.

- **Editor Debug Build:**  
Run the batch file with `editor` as the first parameter and `debug` as the second parameter:
```
build.bat editor debug
```
This produces a debug version of the editor build at `build/game-editor-DEBUG.exe`.

Additional Details:  
The script compiles multiple source files from the `src` folder along with required source files from the `imgui` and `raylib-imgui` directories.  
After a successful build, the script automatically copies resource files from the `res` directory to `build\res`.  
If there is an error during compilation, the script will output "Build failed!"

## Usage
After a successful build, run the generated executable. For example:

- For the standard game:
```
build\game.exe
```
- For the editor mode:
```
build\game-editor.exe
```
