#version 330

// ############################################################################################
// Vertex shader for the 3D mesh renderer
// Processes vertex positions and normals for the geometry shader
// ############################################################################################

layout(location = 0) in vec3 Position;
layout(location = 1) in vec3 Normal;

// ############################################################################################
// Transformation matrices for vertex processing
uniform mat4 gWorld;        // Combined model-view-projection matrix
uniform mat4 gModel;        // Model matrix for world-space transformation
uniform mat4 gNormalMatrix; // Normal transformation matrix (inverse transpose of model matrix)

// ############################################################################################
// Output to geometry shader
out vec3 vertWorldPos;      // Vertex position in world space for lighting calculations
out vec3 vertNormal;        // Transformed normal vector for lighting calculations

void vertexProcessor()
{
    // ############################################################################################
    // Transform the normal correctly using normal matrix
    vertNormal = normalize(mat3(gNormalMatrix) * Normal);
    
    // ############################################################################################
    // Get position in world space (for lighting and clipping calculations)
    vertWorldPos = vec3(gModel * vec4(Position, 1.0));
    
    // ############################################################################################
    // Standard position transformation for the rendering pipeline
    gl_Position = gWorld * vec4(Position, 1.0);
}

// Renamed main to vertexProcessor for uniqueness
void main()
{
    vertexProcessor();
}