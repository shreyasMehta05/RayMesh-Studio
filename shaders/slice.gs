#version 330 core

// ############################################################################################
// Geometry shader for mesh slicing
// Implements real-time mesh slicing against multiple clip planes
// ############################################################################################

layout (triangles) in;
layout (triangle_strip, max_vertices = 24) out;

// ############################################################################################
// Input from vertex shader - position and normal for each vertex of the triangle
in vec3 vertWorldPos[];
in vec3 vertNormal[];

// ############################################################################################
// Output to fragment shader - processed vertices and slice information
out vec3 geomPosition;       // Position in world space
out vec3 geomNormal;         // Normal in world space
out float VisibilityFactor;  // Determines if fragment should be visible (>0) or discarded (<=0)
out vec3 EdgeColor;          // Color for slice edges, zero for regular surface
flat out int DEBUG_VCount;   // Debug info - vertex count (not used)
flat out int DEBUG_TriStatus; // Debug info - triangle status (1=visible, 0=invisible)

// ############################################################################################
// Uniform variables for slice configuration
uniform bool sliceEnabled;             // Whether slicing is active
uniform int numActivePlanes;           // Number of planes to use for slicing (1-4)
uniform vec4 slicePlanes[4];           // Plane equations in form nx,ny,nz,d
uniform vec3 viewPos;                  // Camera position for view-dependent effects

// ############################################################################################
// Constants
const vec3 SLICE_COLOR = vec3(1.0, 0.3, 0.0); // Orange color for slice edges

// ############################################################################################
// Vertex structure to hold and process vertices during clipping
struct Vertex {
    vec3 position;  // Position in world space
    vec3 normal;    // Normal in world space
    vec4 clipPos;   // Position in clip space
    bool visible;   // Whether vertex is on visible side of all slice planes
};

// ############################################################################################
// Simple check if point is on positive side of plane (visible)
// Returns true if visible, false if clipped
bool checkVisibility(vec3 point, vec4 plane) {
    return (dot(plane.xyz, point) + plane.w) >= 0.0;
}

// ############################################################################################
// Find intersection point between edge and plane
// Uses linear interpolation to calculate position, normal, and clip position
Vertex calcIntersection(Vertex v1, Vertex v2, vec4 plane) {
    // Calculate distances to plane
    float d1 = dot(plane.xyz, v1.position) + plane.w;
    float d2 = dot(plane.xyz, v2.position) + plane.w;
    
    // Compute interpolation parameter
    float t = d1 / (d1 - d2);
    
    // Create new vertex at intersection point
    Vertex v;
    v.position = mix(v1.position, v2.position, t);
    v.normal = normalize(mix(v1.normal, v2.normal, t));
    v.clipPos = mix(v1.clipPos, v2.clipPos, t);
    v.visible = true;
    
    return v;
}

// ############################################################################################
// Emit a vertex to the geometry shader output
void outputVertex(Vertex v, bool isVisible, vec3 edgeColor) {
    geomPosition = v.position;
    geomNormal = v.normal;
    VisibilityFactor = isVisible ? 1.0 : 0.0;
    EdgeColor = edgeColor;
    DEBUG_VCount = 0; // Not used
    DEBUG_TriStatus = isVisible ? 1 : 0;
    gl_Position = v.clipPos;
    EmitVertex();
}

// ############################################################################################
// Main geometry shader processor - handles clipping against multiple planes
void geometryProcessor() {
    // ############################################################################################
    // If slicing is disabled, just pass through the triangle without modification
    if (!sliceEnabled || numActivePlanes <= 0) {
        for (int i = 0; i < 3; i++) {
            Vertex v;
            v.position = vertWorldPos[i];
            v.normal = vertNormal[i];
            v.clipPos = gl_in[i].gl_Position;
            v.visible = true;
            
            outputVertex(v, true, vec3(0.0));
        }
        EndPrimitive();
        return;
    }
    
    // ############################################################################################
    // Get input triangle vertices and check visibility against all planes
    Vertex triangle[3];
    for (int i = 0; i < 3; i++) {
        triangle[i].position = vertWorldPos[i];
        triangle[i].normal = vertNormal[i];
        triangle[i].clipPos = gl_in[i].gl_Position;
        triangle[i].visible = true;
        
        // Check visibility against all planes
        for (int p = 0; p < numActivePlanes; p++) {
            if (!checkVisibility(triangle[i].position, slicePlanes[p])) {
                triangle[i].visible = false;
                break;
            }
        }
    }
    
    // ############################################################################################
    // Quick check if triangle is completely invisible (all vertices clipped)
    if (!triangle[0].visible && !triangle[1].visible && !triangle[2].visible) {
        return; // Discard completely invisible triangles
    }
    
    // ############################################################################################
    // Quick check if triangle is completely visible (no clipping needed)
    if (triangle[0].visible && triangle[1].visible && triangle[2].visible) {
        // Output original triangle without modification
        for (int i = 0; i < 3; i++) {
            outputVertex(triangle[i], true, vec3(0.0));
        }
        EndPrimitive();
        return;
    }
    
    // ############################################################################################
    // Process triangle against each plane - Sutherland-Hodgman clipping
    Vertex inputVertices[12], outputVertices[12];
    int inputCount = 3, outputCount;
    
    // Initialize input with original triangle
    for (int i = 0; i < 3; i++) {
        inputVertices[i] = triangle[i];
    }
    
    // ############################################################################################
    // Clip against each plane sequentially
    for (int p = 0; p < numActivePlanes; p++) {
        vec4 plane = slicePlanes[p];
        outputCount = 0;
        
        // Process each edge of the current polygon
        for (int i = 0; i < inputCount; i++) {
            int j = (i + 1) % inputCount;
            
            Vertex current = inputVertices[i];
            Vertex next = inputVertices[j];
            
            bool currentVisible = checkVisibility(current.position, plane);
            bool nextVisible = checkVisibility(next.position, plane);
            
            // If current vertex is visible, include it
            if (currentVisible) {
                outputVertices[outputCount++] = current;
            }
            
            // If visibility changes, find and include intersection
            if (currentVisible != nextVisible) {
                Vertex intersect = calcIntersection(current, next, plane);
                outputVertices[outputCount++] = intersect;
            }
        }
        
        // Check if we have a valid polygon after clipping
        if (outputCount < 3) {
            return; // No valid polygon after clipping
        }
        
        // Copy output to input for next iteration
        for (int i = 0; i < outputCount; i++) {
            inputVertices[i] = outputVertices[i];
        }
        inputCount = outputCount;
    }
    
    // ############################################################################################
    // Output clipped polygon as triangle fan
    bool isSliced = true; // The triangle was sliced
    vec3 sliceEdgeColor = isSliced ? SLICE_COLOR : vec3(0.0);
    
    for (int i = 1; i < inputCount - 1; i++) {
        outputVertex(inputVertices[0], true, sliceEdgeColor);
        outputVertex(inputVertices[i], true, sliceEdgeColor);
        outputVertex(inputVertices[i+1], true, sliceEdgeColor);
        EndPrimitive();
    }
}

// Renamed main to geometryProcessor for uniqueness
void main() {
    geometryProcessor();
}