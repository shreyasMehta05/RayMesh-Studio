#version 330 core

// ###################################################################################
// this shader is used to create an explosion effect on a 3D model
// by displacing the vertices of the model along their normals
// input is from the vertex shader and output is to the fragment shader
// input contains the world space position and normal of each vertex
// output contains the displaced position and normal of each vertex
// ###################################################################################
layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;


// ###################################################################################
// Input from vertex shader
// The input is a triangle strip with 3 vertices
// Each vertex has a world-space position and normal
// The input is passed as an array of vec3s
// ####################################################################################
in vec3 VS_FragPos[];
in vec3 VS_FragNormal[];

// ###################################################################################
// Output to fragment shader
// The output is a triangle strip with 3 vertices
// Each vertex has a world-space position and normal
// The output is passed as an array of vec3s
// #####################################################################################
out vec3 FragPos;
out vec3 FragNormal;

// ###################################################################################
// Uniform variable for blow up effect
uniform bool explodeViewEnabled;
uniform float explodeAmount;
uniform vec3 modelCenter;
// ###################################################################################


// ###################################################################################
// 
void main() {
    if (explodeViewEnabled) {
        
        vec3 faceCenter = (VS_FragPos[0] + VS_FragPos[1] + VS_FragPos[2]) / 3.0;                // Calculate face center by averaging the three vertices
        
        vec3 edge1 = VS_FragPos[1] - VS_FragPos[0];                                             // Calculate edge vectors of the triangle                   
        vec3 edge2 = VS_FragPos[2] - VS_FragPos[0];                                             // Calculate edge vectors of the triangle
        vec3 faceNormal = normalize(cross(edge1, edge2));                                       // Calculate face normal using cross product of edge vectors
        
        vec3 directionFromCenter = faceCenter - modelCenter;                                    // Calculate direction from model center to face center
        float distanceFromCenter = length(directionFromCenter);                                 // Calculate distance from model center to face center
        
        vec3 explodeDirection;
        if (distanceFromCenter > 0.001) {                                                       // if its far from the center, use the direction from center
            directionFromCenter = normalize(directionFromCenter);
            explodeDirection = normalize(directionFromCenter + faceNormal * 0.5);
        } else {
            explodeDirection = faceNormal;                                                      // if its close to the center, use the face normal
        }
        
        float visibleExplodeAmount = explodeAmount * 0.5;                                       // Calculate visible explosion amount
        
        vec3 displacement = explodeDirection * visibleExplodeAmount;                            // Calculate displacement vector based on explosion amount and direction
        
        
        for (int i = 0; i < 3; i++) {                                                           // Emit displaced vertices - the entire face moves together
            vec3 displacedWorldPos = VS_FragPos[i] + displacement;                              // Calculate displaced world position
            
            // Transform the displaced position back to clip space
            // We need to modify gl_Position directly for the explosion to be visible
            gl_Position = gl_in[i].gl_Position + vec4(displacement, 0.0);
            
            
            FragPos = displacedWorldPos;                                                        // Pass the displaced position for lighting calculations
            FragNormal = VS_FragNormal[i];
            
            EmitVertex();
        }
        
        EndPrimitive();
    } 
    else {
        // No explosion - just pass through vertices
        for (int i = 0; i < 3; i++) {
            gl_Position = gl_in[i].gl_Position;
            FragPos = VS_FragPos[i];
            FragNormal = VS_FragNormal[i];
            EmitVertex();
        }
        
        EndPrimitive();
    }
}