# Bringing it All Together - C, CMake, SDL3, Callbacks, Emscripten, Images, Sounds, Fonts

This project is meant to help others understand a valid way of bringing many
related SDL technologies together. These are:

- C
- CMake
- SDL3 (using the Callbacks system)
- SDL_Mixer
- SDL_TTF
- SDL_Image
- Web builds - Emscripten
- Native builds
- Hot Reloading (code and assets)

## Overview

This project is a C project that is built with CMake. All of the SDL dependencies are
downloaded into your build folder. Further, the project works with the Emscripten platform as well as native.

## Architecture

The project is split into two parts:

- **bralltog.c** - The host application that handles SDL initialization and hot reload detection
- **game.c / game.h** - The game logic, compiled as a shared library (`libgame.so`)

In development mode (`DEV_MODE`), the host dynamically loads the game library and monitors for changes to both code and assets. When changes are detected, the library and/or assets are reloaded without restarting the application.

## Build Instructions

For Native builds:

1. make a `build` folder, and `cd` to it.
2. run `cmake ..`
3. run `make`
4. run the executable `./bralltog`

For browser builds using Emscripten

0. ensure `emsdk` is installed and sourced to your shell.
1. make an `emscripten` folder.
2. copy the `runserver.sh` script from `useful-scripts/` to it. This assumes python is present.
3. navigate to your new `emscripten` folder.
4. run `emcmake cmake ..`
5. run `emmake make && ./runserver.sh`

## Hot Reloading (Development Mode)

The project supports hot reloading of both code and assets during development. This allows you to make changes and see them immediately without restarting the application.

### How It Works

1. **Code changes**: When `game.c` or `game.h` is modified and rebuilt, the host detects the change and reloads `libgame.so`
2. **Asset changes**: When files in the `assets/` folder change, the host detects this and notifies the game code to reload assets
3. **Throttled checking**: File modification times are checked every 500ms to minimize overhead

### Using Hot Reload

1. Build the project normally and run `./bralltog`
2. In another terminal, navigate to the `build` folder
3. Copy `useful-scripts/recompile-targets-automatically.sh` to the build folder
4. Run the script: `./recompile-targets-automatically.sh`

The script requires `entr` (install via your package manager). It watches for changes to source files and assets, then automatically:
- Runs `make game` when source files change
- Runs `make copy_assets` when asset files change

### Adding New Assets

To add new assets to the hot reload watch list, edit the `ASSET_FILES` array in `recompile-targets-automatically.sh`:

```bash
ASSET_FILES=(
    "font.ttf"
    "image.png"
    "music.wav"
    "sound.wav"
    "your_new_asset.png"  # Add new assets here
)
```

### Disabling Hot Reload

To build without hot reload support (for release builds), comment out or remove `#define DEV_MODE` in `bralltog.c`. The game will then be statically linked.

## CMake Targets

- `make` or `make bralltog` - Build the main executable
- `make game` - Build only the game library (`libgame.so`)
- `make copy_assets` - Copy assets to the build directory

## Final Words

I hope this helps newcomers to SDL development in C get started more quickly.

## Credits

Credit where credit is due.

1. The `SDL Enthusiasts` Discord. What a great community. Be sure to sign up.
2. This project for helping me get started with Emscripten and CMake.
  (https://github.com/TechnicJelle/TetsawSDL3/blob/main/CMakeLists.txt)
3. This project, whose source helped many to get audio to work with Emscripten.
  (https://github.com/libsdl-org/SDL/issues/6385)
