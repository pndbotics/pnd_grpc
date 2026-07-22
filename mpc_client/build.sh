#!/bin/bash

# Define paths
WORK_DIR="./include"  
PROTO_DIR="../proto"   
PROTO_FILE="adam_control.proto"  
PYTHON_DIR="../python"  
BUILD_DIR="./build"    

# Navigate to the working directory (include)
cd "$WORK_DIR" || exit

# Check protoc and grpc versions
PROTOC_VERSION=$(protoc --version | awk '{print $2}')
GRPC_VERSION=$(pkg-config --modversion grpc++)

echo "protoc version: $PROTOC_VERSION"
echo "grpc version  : $GRPC_VERSION"

# Generate C++ code
echo "Generating C++ code..."
protoc --proto_path="$PROTO_DIR" --grpc_out=. --plugin=protoc-gen-grpc=$(which grpc_cpp_plugin) "$PROTO_DIR/$PROTO_FILE"
protoc --proto_path="$PROTO_DIR" --cpp_out=. "$PROTO_DIR/$PROTO_FILE"

# Generate Python code
echo "Generating Python code..."
python3 -m grpc_tools.protoc -I"$PROTO_DIR" --python_out="$PYTHON_DIR" --grpc_python_out="$PYTHON_DIR" "$PROTO_DIR/$PROTO_FILE"

echo "Code generation completed successfully!"


# Navigate back to the project root directory
cd .. || exit

# Create build directory if it doesn't exist
mkdir -p "$BUILD_DIR"

# Navigate to the build directory
cd "$BUILD_DIR" || exit

# Run cmake to generate Makefile
echo "Running cmake..."
cmake ..  

# Build the project using all available CPU cores
echo "Building project..."
make -j"$(nproc)"

# Return to the previous directory
cd ..

echo "Build completed successfully!"