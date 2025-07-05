#!/bin/bash

# This script renders all scene files and saves the outputs to separate folders
# Navigate to the project directory
cd "$(dirname "$0")"

# Make sure the script is executable
chmod +x render_all_scenes.sh

# Ensure the ray tracer is compiled
make ray_tracer_demo

# Resolution setting (can be changed via command line arguments)
WIDTH=${1:-1920}
HEIGHT=${2:-1080}

# Function to render a single scene
render_scene() {
    SCENE_NAME=$1
    RESOLUTION_WIDTH=$2
    RESOLUTION_HEIGHT=$3
    
    echo "Rendering $SCENE_NAME scene at ${RESOLUTION_WIDTH}x${RESOLUTION_HEIGHT}..."
    
    # Create output directory if it doesn't exist
    mkdir -p "outputs/$SCENE_NAME"
    
    # Render the scene
    ./ray_tracer_demo --scene file --file "scenes/${SCENE_NAME}.txt" \
                     --output "outputs/$SCENE_NAME/${SCENE_NAME}_${RESOLUTION_WIDTH}x${RESOLUTION_HEIGHT}.ppm" \
                     --resolution $RESOLUTION_WIDTH $RESOLUTION_HEIGHT \
                     --skip-cleanup
    
    echo "Rendered $SCENE_NAME scene to outputs/$SCENE_NAME/${SCENE_NAME}_${RESOLUTION_WIDTH}x${RESOLUTION_HEIGHT}.ppm"
}

# Get all scene files
SCENES=($(ls scenes/*.txt | xargs -n1 basename | sed 's/\.txt$//'))

# Render all scenes, or the specified scene if given
if [ -n "$4" ]; then
    if [[ -f "scenes/$4.txt" ]]; then
        render_scene "$4" $WIDTH $HEIGHT
    else
        echo "Scene $4 not found!"
        exit 1
    fi
else
    for SCENE in "${SCENES[@]}"; do
        render_scene "$SCENE" $WIDTH $HEIGHT
    done

    # Add the new scene for sample.off rendering
    render_scene "sample_model" $WIDTH $HEIGHT

    # Also render the colorful octahedron scene
    render_scene "colorful_octahedron" $WIDTH $HEIGHT
fi

echo "All scenes rendered successfully!"
