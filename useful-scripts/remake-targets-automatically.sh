#!/usr/bin/env bash
set -e

# Copy this script to your 'build' folder and run it from there.
# Requires: entr (install via your package manager)

# Configuration
SOURCE_DIR=".."
BUILD_DIR="."

echo "Watching game library sources and assets..."
echo "Press Ctrl+C to stop."
echo ""

# Exit cleanly on Ctrl+C
trap 'echo ""; echo "Stopped."; exit 0' INT

# Function to build list of existing files to watch
build_watch_list() {
    SOURCE_FILES=$(find "$SOURCE_DIR" -maxdepth 1 -name "*.c" -type f 2>/dev/null || true)
    ASSET_FILES=$(find "$SOURCE_DIR/assets" -type f 2>/dev/null || true)
    # Filter to only files that currently exist (avoids race conditions with temp files)
    WATCH_FILES=""
    for f in $SOURCE_FILES $ASSET_FILES; do
        [ -f "$f" ] && WATCH_FILES="$WATCH_FILES$f"$'\n'
    done
}

build_watch_list

# Use entr to watch files. The /_ placeholder is replaced with the changed file path.
# -d flag makes entr exit when a new file is added, so we loop to restart it.
while true; do
    echo "$WATCH_FILES" | entr -d -c sh -c '
        CHANGED_FILE="$1"
        SOURCE_DIR=".."
        BUILD_DIR="."
        
        # Check if the changed file is an asset
        if echo "$CHANGED_FILE" | grep -q "/assets/"; then
            ASSET_NAME=$(basename "$CHANGED_FILE")
            echo "=== Asset changed: $ASSET_NAME ==="
            make copy_assets
        else
            echo "=== Source changed: $(basename "$CHANGED_FILE") ==="
            echo "Rebuilding libgame.so..."
            make game
            echo "Build complete."
        fi
    ' _ /_ 2>/dev/null || true
    
    echo ""
    echo "=== File added/removed, rebuilding... ==="
    
    # Brief pause to let filesystem settle (avoids temp file race conditions)
    sleep 0.5
    
    # Re-discover files
    build_watch_list
    
    # Rebuild everything since we don't know what changed
    make copy_assets
    make game
    echo "Build complete."
    echo ""
done

