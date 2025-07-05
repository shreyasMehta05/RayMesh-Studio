#ifndef MESH_SLICER_H
#define MESH_SLICER_H

#include "./include/math_utils.h"
#include <vector>
#include <algorithm>
#include <unordered_map>

// ##################################################################################
// This class implements the mesh slicing algorithm
// It allows slicing a mesh with up to 4 arbitrary planes
class MeshSlicer {
public:
    
    // ############################################################################################
    // Structure to represent a plane: Ax + By + Cz + D = 0
    struct Plane {              
        Vector3f normal;        // (A, B, C) - the normal vector [normalized]
        float d;                // D - distance from origin 

        // ############################################################################################
        // Default constructor - creates a Y-axis slicing plane
        Plane() : normal(Vector3f(0, 1, 0)), d(0) {}            
        
        // ############################################################################################
        // Create a plane from normal and distance
        Plane(const Vector3f& n, float d) : normal(n), d(d) {   
            // Normalize the normal vector
            float length = normal.length();
            if (length > 0.0001f) {                             // Avoid division by zero
                normal = normal * (1.0f / length); 
                this->d = d / length;  
            }
        }

        // ############################################################################################
        // Create a plane from normal and point
        Plane(const Vector3f& n, const Vector3f& point) {
            // Use the normal vector and a point to calculate d
            // d = -(Ax + By + Cz) ==> normal DOT point
            normal = n;
            // Normalize the normal vector
            float length = normal.length();
            if (length > 0.0001f) {
                normal = normal * (1.0f / length);
            }
            // Calculate d
            d = -normal.Dot(point);
        }

        // ############################################################################################
        // Create a plane from 3 points
        Plane(const Vector3f& p1, const Vector3f& p2, const Vector3f& p3) {
            // Use vector edges and cross product to find the normal
            // normalize the normal vector
            // then calculate d using Ax + By + Cz + D = 0 ==> D = -(Ax + By + Cz) ===> normal DOT point
            Vector3f v1 = p2 - p1;
            Vector3f v2 = p3 - p1;
            normal = v1.Cross(v2);
            
            // Normalize the normal vector
            float length = normal.length();
            if (length > 0.0001f) {
                normal = normal * (1.0f / length);
            }
            // Calculate d
            d = -normal.Dot(p1);
        }

        // ############################################################################################
        // Calculate the signed distance from a point to the plane
        float SignedDistance(const Vector3f& point) const {
            // by definition ==> Normal DOT point + D
            return normal.Dot(point) + d;
        }

        // ############################################################################################
        // Get the plane equation coefficients as a vector (A, B, C, D)
        Vector4f GetEquation() const {
            return Vector4f(normal.x, normal.y, normal.z, d);
        }
    };

private:
    std::vector<Plane> planes;
    const float EPSILON = 1e-6f; // Epsilon for numerical stability
    
    // ############################################################################################
    // Edge-plane intersection calculation with improved numerical stability
    Vector3f CalculateIntersection(const Vector3f& p1, const Vector3f& p2, const Plane& plane) {
        float dist1 = plane.SignedDistance(p1);
        float dist2 = plane.SignedDistance(p2);
        
        // Handle special cases
        if (std::abs(dist1) < EPSILON) return p1; // p1 is on the plane
        if (std::abs(dist2) < EPSILON) return p2; // p2 is on the plane
        if (std::abs(dist1 - dist2) < EPSILON) return (p1 + p2) * 0.5f; // Avoid division by zero
        
        // Interpolation parameter
        float t = dist1 / (dist1 - dist2);
        
        // Ensure t is in the valid range [0, 1]
        t = std::max(0.0f, std::min(1.0f, t));
        
        // Calculate the intersection point using linear interpolation
        return p1 + (p2 - p1) * t;
    }

    // ############################################################################################
    // Interpolate attributes (e.g., normals) at an intersection point
    Vector3f InterpolateAttribute(const Vector3f& attr1, const Vector3f& attr2, 
                                 const Vector3f& p1, const Vector3f& p2, 
                                 const Vector3f& intersection) {
        // Calculate distance from p1 to intersection as a percentage of total edge length
        float totalLength = (p2 - p1).length();
        if (totalLength < EPSILON) return attr1; // Points are too close, just use attr1
        
        float t = (intersection - p1).length() / totalLength;
        t = std::max(0.0f, std::min(1.0f, t)); // Clamp to [0, 1]
        
        // Linearly interpolate between attr1 and attr2
        return attr1 + (attr2 - attr1) * t;
    }

    // ############################################################################################
    // Determine if a point is inside all planes
    bool IsInsidePlanes(const Vector3f& point) {
        for (const auto& plane : planes) {
            if (plane.SignedDistance(point) > EPSILON) {
                return false;  // Outside of at least one plane
            }
        }
        return true;  // Inside all planes
    }

public:
    MeshSlicer() {}

    // ############################################################################################
    // Add a plane for slicing
    void AddPlane(const Plane& plane) {
        if (planes.size() < 4) {
            planes.push_back(plane);
        }
    }

    // ############################################################################################
    // Clear all planes
    void ClearPlanes() {
        planes.clear();
    }

    // ############################################################################################
    // Get the number of active planes
    int GetPlaneCount() const {
        return planes.size();
    }

    // ############################################################################################
    // Get a plane by index
    Plane GetPlane(int index) const {
        if (index >= 0 && index < planes.size()) {
            return planes[index];
        }
        return Plane();  // Return default plane if index is out of range
    }

    // ############################################################################################
    // Apply slicing to a mesh
    void SliceMesh(
        const std::vector<Vector3f>& inVertices,
        const std::vector<Vector3f>& inNormals,
        const std::vector<unsigned int>& inIndices,
        std::vector<Vector3f>& outVertices,
        std::vector<Vector3f>& outNormals,
        std::vector<unsigned int>& outIndices
    ) {
        if (planes.empty()) {
            // No planes, just copy the input mesh
            outVertices = inVertices;
            outNormals = inNormals;
            outIndices = inIndices;
            return;
        }

        outVertices.clear();
        outNormals.clear();
        outIndices.clear();

        // Reserve space in the output vectors (approximate)
        size_t estimatedVertexCount = inIndices.size(); // Rough estimate
        outVertices.reserve(estimatedVertexCount);
        outNormals.reserve(estimatedVertexCount);
        outIndices.reserve(estimatedVertexCount);

        // Process each triangle
        for (size_t i = 0; i < inIndices.size(); i += 3) {
            // Get the three vertices of the triangle
            Vector3f v0 = inVertices[inIndices[i]];
            Vector3f v1 = inVertices[inIndices[i + 1]];
            Vector3f v2 = inVertices[inIndices[i + 2]];

            // Get the three normals of the triangle
            Vector3f n0 = inNormals[inIndices[i]];
            Vector3f n1 = inNormals[inIndices[i + 1]];
            Vector3f n2 = inNormals[inIndices[i + 2]];

            // Skip triangles where all vertices are outside any plane
            bool anyInside = false;
            bool allOutside = false;
            
            for (const auto& plane : planes) {
                // Check if all vertices are outside the same plane
                if (plane.SignedDistance(v0) > EPSILON &&
                    plane.SignedDistance(v1) > EPSILON &&
                    plane.SignedDistance(v2) > EPSILON) {
                    allOutside = true;
                    break;
                }
                
                // Check if any vertex is inside or on all planes
                if (IsInsidePlanes(v0) || IsInsidePlanes(v1) || IsInsidePlanes(v2)) {
                    anyInside = true;
                }
            }
            
            if (allOutside) continue; // Skip this triangle
            
            // Process the triangle against all planes
            std::vector<Vector3f> triangleVertices = { v0, v1, v2 };
            std::vector<Vector3f> triangleNormals = { n0, n1, n2 };

            // Apply each plane to the triangle
            for (const auto& plane : planes) {
                SliceTriangleWithPlane(triangleVertices, triangleNormals, plane);
            }

            // Add the resulting polygon to the output mesh
            if (triangleVertices.size() >= 3) {
                // Triangulate the polygon (simple fan triangulation)
                unsigned int baseIndex = outVertices.size();
                
                // Add all vertices and normals
                for (size_t j = 0; j < triangleVertices.size(); j++) {
                    outVertices.push_back(triangleVertices[j]);
                    outNormals.push_back(triangleNormals[j]);
                }
                
                // Create triangles - fan triangulation
                for (size_t j = 1; j < triangleVertices.size() - 1; j++) {
                    outIndices.push_back(baseIndex);
                    outIndices.push_back(baseIndex + j);
                    outIndices.push_back(baseIndex + j + 1);
                }
            }
        }
    }

private:
    // ############################################################################################
    // Slice a polygon with a plane
    void SliceTriangleWithPlane(
        std::vector<Vector3f>& vertices,
        std::vector<Vector3f>& normals,
        const Plane& plane
    ) {
        if (vertices.empty() || vertices.size() != normals.size()) return;
        
        std::vector<Vector3f> resultVertices;
        std::vector<Vector3f> resultNormals;
        
        // Classify all vertices (inside/outside/on the plane)
        std::vector<int> classification;
        classification.reserve(vertices.size());
        
        for (size_t i = 0; i < vertices.size(); i++) {
            float dist = plane.SignedDistance(vertices[i]);
            if (std::abs(dist) < EPSILON) {
                classification.push_back(0);  // On the plane
            } else if (dist < 0) {
                classification.push_back(-1); // Inside
            } else {
                classification.push_back(1);  // Outside
            }
        }
        
        // Process each edge
        for (size_t i = 0; i < vertices.size(); i++) {
            size_t j = (i + 1) % vertices.size();
            
            Vector3f currentVertex = vertices[i];
            Vector3f nextVertex = vertices[j];
            Vector3f currentNormal = normals[i];
            Vector3f nextNormal = normals[j];
            
            int currentClass = classification[i];
            int nextClass = classification[j];
            
            // Current vertex processing
            if (currentClass <= 0) { // Inside or on the plane
                resultVertices.push_back(currentVertex);
                resultNormals.push_back(currentNormal);
            }
            
            // Intersection calculation if vertices are on opposite sides
            if ((currentClass == -1 && nextClass == 1) || (currentClass == 1 && nextClass == -1)) {
                // Calculate intersection point
                Vector3f intersection = CalculateIntersection(currentVertex, nextVertex, plane);
                
                // Calculate interpolated normal
                Vector3f interpolatedNormal = InterpolateAttribute(
                    currentNormal, nextNormal, currentVertex, nextVertex, intersection);
                interpolatedNormal.Normalize();
                
                resultVertices.push_back(intersection);
                resultNormals.push_back(interpolatedNormal);
            }
        }
        
        // Update the input vectors with the result
        vertices = resultVertices;
        normals = resultNormals;
    }

    // ############################################################################################
    // Advanced triangulation methods (earcut algorithm could be used for better results)
    // For complex polygons, a more sophisticated triangulation algorithm would be needed,
    // but for most mesh slicing cases, fan triangulation works well enough
};

#endif // MESH_SLICER_H
