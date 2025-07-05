#version 330

// ############################################################################################
// Fragment shader for mesh rendering and slicing visualization
// Handles lighting calculations and sliced edge highlighting
// ############################################################################################

// Input from geometry shader - position, normal, and slice information
in vec3 geomPosition;       // Fragment position in world space
in vec3 geomNormal;         // Fragment normal in world space
in float VisibilityFactor;  // Controls fragment visibility
in vec3 EdgeColor;          // Color for slice edges
flat in int DEBUG_VCount;   // Debug counter for vertices (unused)
flat in int DEBUG_TriStatus; // Debug flag for triangle status

// ############################################################################################
// Uniform variables for lighting and material properties
uniform vec3 viewPos;       // Camera position for specular calculation
uniform vec3 objectColor;   // Base color of the object
uniform bool sliceEnabled;  // Whether slicing is active

// ############################################################################################
// Output color for the fragment
layout(location = 0) out vec4 FragColor;

// ############################################################################################
// Main fragment processing function - handles lighting and slicing visualization
void fragmentProcessor()
{
    // ############################################################################################
    // Critical check - discard fragments that should be invisible due to slicing
    if (sliceEnabled && VisibilityFactor <= 0.0) {
        discard;
    }
    
    // ############################################################################################
    // Simple Phong lighting calculation
    vec3 normal = normalize(geomNormal);
    vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0)); // Light from top-right
    float diff = max(dot(normal, lightDir), 0.0);   // Diffuse factor
    
    // Basic lighting components
    vec3 ambient = vec3(0.2);                      // Ambient component
    vec3 diffuse = diff * vec3(0.8);               // Diffuse component
    
    vec3 color;
    
    // ############################################################################################
    // If it's a slice edge, use the slice color (orange highlight)
    if (length(EdgeColor) > 0.0) {
        color = ambient * EdgeColor + diffuse * EdgeColor;
    } else {
        // Otherwise use the object's base color with lighting
        color = (ambient + diffuse) * objectColor;
    }
    
    // ############################################################################################
    // Output final color with full opacity
    FragColor = vec4(color, 1.0);
}

// Renamed main to fragmentProcessor for uniqueness
void main()
{
    fragmentProcessor();
}