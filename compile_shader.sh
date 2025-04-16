#!/bin/bash

# compile_shader.sh - Compiles GLSL shaders to SPIR-V for Vulkan/MoltenVK
# Created: April 15, 2025

# Configuration
SHADER_DIR="./shaders"
OUTPUT_DIR="./shaders"

# Attempt to find glslangValidator in common Vulkan SDK locations
GLSLANG_VALIDATOR=""
VULKAN_SDK_LOCATIONS=(
    "/usr/local/bin/glslangValidator"
    "$HOME/VulkanSDK/*/macOS/bin/glslangValidator"
    "/opt/homebrew/bin/glslangValidator"
    "/opt/vulkan/bin/glslangValidator"
    "/Library/Application Support/VulkanSDK/*/macOS/bin/glslangValidator"
    "$VULKAN_SDK/bin/glslangValidator"
)

# Check standard path first
if command -v glslangValidator >/dev/null 2>&1; then
    GLSLANG_VALIDATOR="glslangValidator"
else
    # Try to find in common locations
    for LOCATION in "${VULKAN_SDK_LOCATIONS[@]}"; do
        # Expand glob patterns
        for EXPANDED_PATH in $LOCATION; do
            if [ -f "$EXPANDED_PATH" ] && [ -x "$EXPANDED_PATH" ]; then
                GLSLANG_VALIDATOR="$EXPANDED_PATH"
                echo "Found glslangValidator at: $GLSLANG_VALIDATOR"
                break 2
            fi
        done
    done
fi

# Check if we found glslangValidator
if [ -z "$GLSLANG_VALIDATOR" ]; then
    echo "Error: Could not find glslangValidator in standard locations."
    echo "Please install the Vulkan SDK from https://vulkan.lunarg.com/sdk/home"
    echo "or specify the path manually by setting the VULKAN_SDK environment variable."
    exit 1
fi

# Make sure the output directory exists
mkdir -p $OUTPUT_DIR

# Print header
echo "===================================="
echo "GLSL to SPIR-V Shader Compiler"
echo "===================================="

# Count of shaders to compile
SHADER_COUNT=$(find $SHADER_DIR -type f \( -name "*.vert" -o -name "*.frag" \) | wc -l)
echo "Found $SHADER_COUNT shader(s) to compile"
echo ""

# Compile all vertex shaders
for SHADER in $SHADER_DIR/*.vert; do
    if [ -f "$SHADER" ]; then
        BASENAME=$(basename "$SHADER" .vert)
        OUTPUT="$OUTPUT_DIR/${BASENAME}.spv"
        
        echo "Compiling vertex shader: $SHADER"
        "$GLSLANG_VALIDATOR" -V "$SHADER" -o "$OUTPUT"
        
        if [ $? -eq 0 ]; then
            echo "✓ Successfully compiled to $OUTPUT"
        else
            echo "✗ Failed to compile $SHADER"
        fi
        echo ""
    fi
done

# Compile all fragment shaders
for SHADER in $SHADER_DIR/*.frag; do
    if [ -f "$SHADER" ]; then
        BASENAME=$(basename "$SHADER" .frag)
        OUTPUT="$OUTPUT_DIR/${BASENAME}.spv"
        
        echo "Compiling fragment shader: $SHADER"
        "$GLSLANG_VALIDATOR" -V "$SHADER" -o "$OUTPUT"
        
        if [ $? -eq 0 ]; then
            echo "✓ Successfully compiled to $OUTPUT"
        else
            echo "✗ Failed to compile $SHADER"
        fi
        echo ""
    fi
done

# Compile instanced shaders
for SHADER in $SHADER_DIR/*_instanced.vert $SHADER_DIR/*_instanced.frag; do
    if [ -f "$SHADER" ]; then
        BASENAME=$(basename "$SHADER")
        EXT="${BASENAME##*.}"
        NAME="${BASENAME%.*}"
        OUTPUT="$OUTPUT_DIR/${NAME}.spv"
        
        echo "Compiling instanced shader: $SHADER"
        "$GLSLANG_VALIDATOR" -V "$SHADER" -o "$OUTPUT"
        
        if [ $? -eq 0 ]; then
            echo "✓ Successfully compiled to $OUTPUT"
        else
            echo "✗ Failed to compile $SHADER"
        fi
        echo ""
    fi
done

echo "===================================="
echo "Shader compilation complete"
echo "===================================="