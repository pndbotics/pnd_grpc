#!/bin/bash

# Define directories to clean
DIRECTORIES=("build" "bin" "lib")

# Loop through directories and remove them if they exist
for DIR in "${DIRECTORIES[@]}"; do
    if [[ -d "$DIR" ]]; then
        echo "Removing directory: $DIR"
        rm -rf "$DIR"
    else
        echo "Directory $DIR does not exist. Skipping..."
    fi
done

echo "Cleanup completed!"