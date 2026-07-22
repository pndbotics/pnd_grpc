#!/bin/bash

# Get the directory where the script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR"

# Define paths
BIN_CLIENT="$PROJECT_ROOT/bin/adam_command_client"  # Path to the C++ executable
PYTHON_CLIENT="$PROJECT_ROOT/python/adam_command_client.py"  # Path to the Python script

# Default mode (python if no argument is provided)
DEFAULT_MODE="python"

# Check if an argument is provided
if [[ $# -gt 0 ]]; then
    RUN_MODE="$1"
else
    RUN_MODE="$DEFAULT_MODE"
fi

# Run the selected mode
if [[ "$RUN_MODE" == "bin" ]]; then
    # Run the C++ executable
    if [[ -f "$BIN_CLIENT" ]]; then
        echo "Running C++ client: $BIN_CLIENT"
        cd "$PROJECT_ROOT/bin" || exit 1  
        "./$(basename "$BIN_CLIENT")" 
    else
        echo "Error: C++ client not found at $BIN_CLIENT"
        echo "Please ensure that the C++ client is built and located in the correct directory."
        exit 1
    fi
elif [[ "$RUN_MODE" == "python" ]]; then
    # Run the Python script
    if [[ -f "$PYTHON_CLIENT" ]]; then
        echo "Running Python client: $PYTHON_CLIENT"
        cd "$PROJECT_ROOT/python" || exit 1  
        python3 "./$(basename "$PYTHON_CLIENT")"  
    else
        echo "Error: Python client not found at $PYTHON_CLIENT"
        exit 1
    fi
else
    # Invalid argument
    echo "Error: Invalid argument. Use 'bin' to run the C++ client or 'python' to run the Python client."
    exit 1
fi