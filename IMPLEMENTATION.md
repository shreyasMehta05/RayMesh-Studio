# MeshTracer 3D - Implementation Details

This document provides in-depth technical explanations of the algorithms and techniques implemented in MeshTracer 3D.

## Table of Contents
- [3D Mesh Viewer](#3d-mesh-viewer)
- [Mesh Slicing](#mesh-slicing)
- [Line Rasterization](#line-rasterization)
- [Scanline Fill](#scanline-fill)
- [Ray Tracing](#ray-tracing)

## 3D Mesh Viewer

The 3D Mesh Viewer serves as the main visualization component, allowing you to load and explore 3D models in the OFF format.

### Loading 3D Models

The viewer uses `OFFReader.h` to parse and load 3D models:

```cpp
bool LoadOFFModel(const char* filename)
{
    // Load the model from the OFF file
    model = readOffFile((char*)filename);
    if (!model) {
        std::cerr << "Failed to load model: " << filename << std::endl;
        return false;
    }
    
    // Process vertices, normals, and faces
    // ...
    
    return true;
}
```

After loading, the model is:
- Centered in the viewport
- Scaled to a consistent size
- Given proper normal vectors for lighting calculations

### Lighting and Rendering

The rendering pipeline uses OpenGL shaders with:
- Customizable lighting parameters
- Adjustable object color
- Automatic normal calculation for proper lighting
- Camera position control for different viewing angles

## Mesh Slicing

The mesh slicing feature allows you to cut 3D models with arbitrary planes, revealing their internal structure.

### Implementation Overview: Mesh Slicing

The mesh slicing implementation is contained in `MeshSlicer.h` and provides both CPU-based and GPU-based approaches:

```cpp
class MeshSlicer {
public:
    struct Plane {
        Vector3f normal;  // Normalized plane normal
        float d;          // Distance from origin
        
        // Constructors and utility methods
        // ...
    };
    
    // Core slicing method
    void SliceMesh(
        const std::vector<Vector3f>& inVertices,
        const std::vector<Vector3f>& inNormals,
        const std::vector<unsigned int>& inIndices,
        std::vector<Vector3f>& outVertices,
        std::vector<Vector3f>& outNormals,
        std::vector<unsigned int>& outIndices
    );
    
    // Other methods for plane management
    // ...
};
```

The slicing algorithm works by:
1. Processing each triangle in the mesh
2. Testing against each slicing plane
3. Calculating intersection points when triangles cross planes
4. Reconstructing new polygons from the intersection results
5. Triangulating the resulting polygons for rendering

### CPU vs GPU Slicing

The application offers two slicing modes:

**CPU-based Slicing:**
- Implemented in the `MeshSlicer` class
- Precise polygon reconstruction
- Handles multiple slicing planes in sequence
- Creates new geometry that preserves proper normals

**GPU-based Slicing:**
- Implemented in the `slice.gs` geometry shader
- Real-time performance even for complex models
- Uses the discard pattern to remove geometry outside the slicing planes
- Maintains original vertex attributes

## Line Rasterization

The Line Rasterization module provides implementations of classic algorithms for drawing 2D lines on a raster display.

### Implementation Overview: Line Rasterization

The `LineRasterizer.h` file implements three fundamental line drawing algorithms:

```cpp
class LineRasterizer {
public:
    // Bresenham's line algorithm
    static std::vector<Pixel> BresenhamLine(int x0, int y0, int x1, int y1);
    
    // Digital Differential Analyzer
    static std::vector<Pixel> DDALine(int x0, int y0, int x1, int y1);
    
    // Midpoint line algorithm
    static std::vector<Pixel> MidpointLine(int x0, int y0, int x1, int y1);
};
```

### Algorithms Implemented

**Bresenham's Line Algorithm:**
- Integer-based calculations for efficiency
- Minimizes error by choosing optimal pixels
- Handles all line cases with consistent behavior

**DDA Line Algorithm:**
- Uses floating-point calculations for higher accuracy
- Iteratively steps along the line using consistent increments
- Simple implementation but potentially slower than Bresenham

**Midpoint Line Algorithm:**
- Decision-based approach for pixel selection
- Optimized for different slope cases
- Handles vertical and horizontal lines as special cases

## Scanline Fill

The Scanline Fill module implements a classic polygon filling algorithm that efficiently converts vector polygons into raster representations.

### Implementation Overview: Scanline Fill

The implementation in `ScanlineFill.h` focuses on the edge-tracking scanline algorithm:

```cpp
class ScanlineFill {
public:
    // Main polygon filling method
    static std::vector<Pixel> FillPolygon(const std::vector<std::pair<int, int>>& vertices);
    
    // Advanced method for handling polygons with holes
    static std::vector<Pixel> FillPolygonWithHoles(
        const std::vector<std::pair<int, int>>& outerPolygon,
        const std::vector<std::vector<std::pair<int, int>>>& holes);
};
```

### Algorithm Explained

The scanline fill algorithm works through these steps:

1. **Edge Table Construction:**
   - Collect all edges of the polygon
   - Store their endpoints, slopes, and y-ranges
   - Sort edges by their minimum y-coordinate

2. **Active Edge List (AEL) Processing:**
   - Maintain a list of edges that intersect the current scanline
   - For each scanline (from top to bottom):
     - Add edges from the edge table that start at this scanline
     - Remove edges that end at this scanline
     - Sort edges by their x-intersection with the scanline
     - Fill pixels between pairs of edges
     - Update x-intersections for the next scanline

3. **Special Case Handling:**
   - Horizontal edges
   - Vertices that connect multiple edges
   - Self-intersecting polygons
   - Numerical stability issues

## Ray Tracing

The ray tracing module is a complete rendering system capable of producing photorealistic images by simulating light paths.

### Core Concepts

The ray tracer is built around these fundamental components:

- **Rays**: Mathematical representations of light paths defined by an origin point and direction vector
- **Camera**: Generates primary rays through image pixels based on a viewing frustum
- **Hittable Objects**: Geometric primitives that rays can intersect with, providing surface information
- **Materials**: Define how surfaces interact with light through reflection, color, and shininess
- **Lights**: Sources of illumination in the scene with position, color, and intensity properties

### Implementation Details

The `RayTracer.h` file implements the complete ray tracing system with several key classes:

```cpp
// Ray structure representing a ray with origin and direction
struct Ray {
    Vector3f origin;
    Vector3f direction;
};

// Material class defining surface properties
class Material {
    Vector3f color;        // Base color
    float ambientCoef;     // Ambient light coefficient
    float diffuseCoef;     // Diffuse reflection coefficient
    float specularCoef;    // Specular highlight coefficient
    float shininess;       // Specular highlight sharpness
    float reflectivity;    // Reflectivity factor (0-1)
};

// Camera class for generating primary rays
class Camera {
    Vector3f origin;
    Vector3f lowerLeftCorner;
    Vector3f horizontal;
    Vector3f vertical;
};

// Primitive classes for ray-object intersection
class Sphere : public Hittable { ... };
class Box : public Hittable { ... };
class Triangle : public Hittable { ... };

// Main ray tracer class
class RayTracer { ... };
```

### Rendering Pipeline

The ray tracing process follows this detailed pipeline:

1. **Camera Setup**: 
   - Define camera position, look direction, and field of view
   - Calculate the view frustum parameters for ray generation

2. **Primary Ray Generation**: 
   - For each pixel (x,y) in the output image:
     - Calculate normalized device coordinates (u,v) in [0,1] range
     - Generate a ray from camera through the pixel into the scene

3. **Ray-Scene Intersection**: 
   - For each ray, find the closest intersection with any scene object
   - Optimized intersection algorithms for each primitive type:
     - Spheres: Analytical solution to ray-sphere equation
     - Boxes: Fast slab method for axis-aligned boxes
     - Triangles: Möller–Trumbore algorithm for efficient testing

4. **Shading Calculation**: 
   - For each intersection point, evaluate the Phong illumination model:
     - Ambient: Global illumination factor
     - Diffuse: Lambert's cosine law for surface orientation
     - Specular: Blinn-Phong model for highlights
   - Calculate shadow rays to determine light visibility

5. **Recursive Ray Tracing**: 
   - For reflective materials, cast secondary rays in reflection direction
   - Apply recursive color contribution with depth limit and attenuation

6. **Pixel Color Determination**:
   - Combine all lighting components and reflection contributions
   - Apply gamma correction for perceptual accuracy
   - Convert final color to RGB byte values for the output image

### Material System

The ray tracer implements a physically-inspired material system where each material defines how light interacts with a surface:

```cpp
Material redMaterial(Vector3f(0.9f, 0.2f, 0.2f), 0.1f, 0.7f, 0.4f, 32.0f, 0.1f);
//                    |------ color -----|  |amb|  |dif|  |spc|  |shin|  |refl|
```

The parameters control:

- **Base Color**: The intrinsic color of the object under white light
- **Ambient Coefficient**: How much ambient (indirect) light the surface reflects
- **Diffuse Coefficient**: How much diffuse (scattered) light the surface reflects
- **Specular Coefficient**: How much specular (mirror-like) light creates highlights
- **Shininess**: The tightness of specular highlights (higher = smaller, sharper highlights)
- **Reflectivity**: How mirror-like the surface is (0 = no reflection, 1 = perfect mirror)

### Light and Shadow

Shadow calculation uses ray casting to determine light visibility:

1. For each light source and intersection point:
   - Cast a "shadow ray" from the intersection point toward the light
   - If the shadow ray hits any object before reaching the light, the point is in shadow
   - Apply only ambient lighting to shadowed areas
   - Apply full Phong illumination to lit areas

This creates realistic shadows that depend on scene geometry and light positions.

### Reflection

Reflections are implemented through recursive ray tracing:

```cpp
Vector3f rayColorWithReflection(const Ray& ray, const HittableList& world, int depth) {
    if (depth <= 0) return Vector3f(0.0f); // Base case for recursion
    
    if (world.hit(ray, 0.001f, INFINITY, rec)) {
        // Calculate direct lighting
        Vector3f directColor = calculateLighting(rec, ray, world);
        
        // Add reflection if material is reflective
        if (rec.material.reflectivity > 0.0f) {
            // Calculate reflection direction
            Vector3f reflected = reflect(ray.direction, rec.normal);
            
            // Cast reflection ray and get color recursively
            Ray reflectionRay(rec.point + rec.normal * 0.001f, reflected);
            Vector3f reflectionColor = rayColorWithReflection(reflectionRay, world, depth - 1);
            
            // Mix direct and reflection colors based on material reflectivity
            return directColor * (1.0f - rec.material.reflectivity) + 
                   reflectionColor * rec.material.reflectivity;
        }
        
        return directColor;
    }
    
    // Background color for rays that hit nothing
    return backgroundColor;
}
```

The reflection system includes:

- **Adaptive Recursion Depth**: Limits to prevent excessive ray bounces
- **Perfect Mirror Reflection**: Calculated using the reflection law (angle of incidence = angle of reflection)
- **Fresnel-like Blending**: Mixing direct and reflected colors based on material reflectivity
- **Self-intersection Prevention**: Small offsets to avoid numerical precision issues
