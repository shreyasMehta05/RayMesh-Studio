#ifndef RAY_TRACER_H
#define RAY_TRACER_H

#include "./include/math_utils.h"
#include <vector>
#include <cmath>
#include <algorithm>
#include <limits>
#include <string>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>

// ############################################################################################
// Ray structure for ray tracing
struct Ray {
    Vector3f origin;
    Vector3f direction;
    
    Ray(const Vector3f& o, const Vector3f& d) : origin(o), direction(d) {
        direction.Normalize();
    }
};

// ############################################################################################
// Material class for object appearance
class Material {
public:
    Vector3f color;
    float ambientCoef;
    float diffuseCoef;
    float specularCoef;
    float shininess;
    float reflectivity;

    // ############################################################################################
    // Default constructor - creates a gray material with standard properties
    Material() 
        : color(Vector3f(0.8f, 0.8f, 0.8f)), 
          ambientCoef(0.1f), 
          diffuseCoef(0.7f), 
          specularCoef(0.3f), 
          shininess(32.0f), 
          reflectivity(0.0f) {}

    // ############################################################################################
    // Parameterized constructor - creates a material with specific properties
    Material(const Vector3f& col, float ambient, float diffuse, float specular, 
             float shine, float reflect)
        : color(col),
          ambientCoef(ambient),
          diffuseCoef(diffuse),
          specularCoef(specular),
          shininess(shine),
          reflectivity(reflect) {}
};

// Forward declaration
class RayTracer;
void addMeshFromFile(RayTracer& rayTracer, const std::string& filename, const Vector3f& position, 
                     float scale, const Material& material);

// ############################################################################################
// Hit record to store intersection information
struct HitRecord {
    float t;             // Distance along the ray
    Vector3f point;      // Intersection point
    Vector3f normal;     // Surface normal at intersection
    bool frontFace;      // Whether the ray hit the front face
    Material material;   // Material of the hit object
    
    // ############################################################################################
    // Set the normal and determine front face
    void setFaceNormal(const Ray& ray, const Vector3f& outwardNormal) {
        frontFace = ray.direction.Dot(outwardNormal) < 0;
        normal = frontFace ? outwardNormal : outwardNormal * -1.0f;
    }
};

// ############################################################################################
// Abstract base class for objects that can be intersected by rays
class Hittable {
public:
    Material material;
    
    Hittable() : material() {}
    Hittable(const Material& mat) : material(mat) {}
    
    virtual bool hit(const Ray& ray, float tMin, float tMax, HitRecord& rec) const = 0;
    virtual ~Hittable() {}
};

// ############################################################################################
// Sphere class - represents a sphere in 3D space
class Sphere : public Hittable {
public:
    Vector3f center;
    float radius;
    
    Sphere(const Vector3f& c, float r) : center(c), radius(r) {}
    Sphere(const Vector3f& c, float r, const Material& mat) : Hittable(mat), center(c), radius(r) {}
    
    // ############################################################################################
    // Ray-sphere intersection test
    virtual bool hit(const Ray& ray, float tMin, float tMax, HitRecord& rec) const override {
        Vector3f oc = ray.origin - center;
        float a = ray.direction.Dot(ray.direction);
        float half_b = oc.Dot(ray.direction);
        float c = oc.Dot(oc) - radius * radius;
        
        float discriminant = half_b * half_b - a * c;
        if (discriminant < 0) {
            return false;
        }
        
        float sqrtd = sqrt(discriminant);
        
        // Find the nearest root that lies in the acceptable range
        float root = (-half_b - sqrtd) / a;
        if (root < tMin || root > tMax) {
            root = (-half_b + sqrtd) / a;
            if (root < tMin || root > tMax) {
                return false;
            }
        }
        
        rec.t = root;
        rec.point = ray.origin + ray.direction * rec.t;
        Vector3f outwardNormal = (rec.point - center) * (1.0f / radius);
        rec.setFaceNormal(ray, outwardNormal);
        
        // If hit, set the material
        rec.material = material;
        
        return true;
    }
};

// ############################################################################################
// Axis-aligned box class - represents a box with sides parallel to the axes
class Box : public Hittable {
public:
    Vector3f boxMin;
    Vector3f boxMax;
    
    Box(const Vector3f& min, const Vector3f& max) : boxMin(min), boxMax(max) {}
    Box(const Vector3f& min, const Vector3f& max, const Material& mat) 
        : Hittable(mat), boxMin(min), boxMax(max) {}
    
    // ############################################################################################
    // Ray-box intersection test using slab method
    virtual bool hit(const Ray& ray, float tMin, float tMax, HitRecord& rec) const override {
        float tNear = tMin;
        float tFar = tMax;
        int hitAxis = -1;
        bool hitIsMin = false;
        
        // Check intersection with each pair of planes (slabs)
        for (int i = 0; i < 3; i++) {
            if (std::abs(ray.direction[i]) < 1e-8) {
                // Ray is parallel to this axis
                if (ray.origin[i] < boxMin[i] || ray.origin[i] > boxMax[i]) {
                    return false;
                }
            } else {
                // Calculate intersections with planes perpendicular to this axis
                float invD = 1.0f / ray.direction[i];
                float t0 = (boxMin[i] - ray.origin[i]) * invD;
                float t1 = (boxMax[i] - ray.origin[i]) * invD;
                
                if (t0 > t1) std::swap(t0, t1);
                
                if (t0 > tNear) {
                    tNear = t0;
                    hitAxis = i;
                    hitIsMin = true;
                }
                if (t1 < tFar) {
                    tFar = t1;
                    if (t1 < t0) {
                        hitAxis = i;
                        hitIsMin = false;
                    }
                }
                
                if (tNear > tFar) return false;
                if (tFar < tMin) return false;
            }
        }
        
        if (tNear > tMax) return false;
        
        rec.t = tNear;
        rec.point = ray.origin + ray.direction * rec.t;
        
        // Calculate normal based on which face was hit
        Vector3f outwardNormal;
        outwardNormal.SetZero();
        if (hitAxis >= 0) {
            // Set the normal based on the hit axis
            if (hitAxis == 0) {
                outwardNormal.x = hitIsMin ? -1.0f : 1.0f;
            } else if (hitAxis == 1) {
                outwardNormal.y = hitIsMin ? -1.0f : 1.0f;
            } else if (hitAxis == 2) {
                outwardNormal.z = hitIsMin ? -1.0f : 1.0f;
            }
        }
        
        rec.setFaceNormal(ray, outwardNormal);
        
        // If hit, set the material
        rec.material = material;
        
        return true;
    }
};

// ############################################################################################
// Triangle class - represents a triangle in 3D space
class Triangle : public Hittable {
public:
    Vector3f v0, v1, v2;  // Vertices
    Vector3f normal;      // Face normal
    
    // ############################################################################################
    // Constructor that calculates the face normal from vertices
    Triangle(const Vector3f& _v0, const Vector3f& _v1, const Vector3f& _v2)
        : v0(_v0), v1(_v1), v2(_v2) {
        // Calculate face normal
        Vector3f edge1 = v1 - v0;
        Vector3f edge2 = v2 - v0;
        normal = edge1.Cross(edge2);
        normal.Normalize();
    }
    
    // ############################################################################################
    // Constructor with material
    Triangle(const Vector3f& _v0, const Vector3f& _v1, const Vector3f& _v2, const Material& mat)
        : Hittable(mat), v0(_v0), v1(_v1), v2(_v2) {
        // Calculate face normal
        Vector3f edge1 = v1 - v0;
        Vector3f edge2 = v2 - v0;
        normal = edge1.Cross(edge2);
        normal.Normalize();
    }
    
    // ############################################################################################
    // Ray-triangle intersection test using Möller–Trumbore algorithm
    virtual bool hit(const Ray& ray, float tMin, float tMax, HitRecord& rec) const override {
        // Implementation of Möller–Trumbore algorithm for ray-triangle intersection
        Vector3f edge1 = v1 - v0;
        Vector3f edge2 = v2 - v0;
        Vector3f h = ray.direction.Cross(edge2);
        float a = edge1.Dot(h);
        
        // If determinant is near zero, ray lies in plane of triangle
        if (std::abs(a) < 1e-8) {
            return false;
        }
        
        float f = 1.0f / a;
        Vector3f s = ray.origin - v0;
        float u = f * s.Dot(h);
        
        // If u is outside [0, 1], ray doesn't intersect triangle
        if (u < 0.0f || u > 1.0f) {
            return false;
        }
        
        Vector3f q = s.Cross(edge1);
        float v = f * ray.direction.Dot(q);
        
        // If v is outside [0, 1] or u+v > 1, ray doesn't intersect triangle
        if (v < 0.0f || u + v > 1.0f) {
            return false;
        }
        
        // Calculate t, the distance along the ray to the intersection
        float t = f * edge2.Dot(q);
        
        // Check if t is within the allowed range
        if (t < tMin || t > tMax) {
            return false;
        }
        
        // Intersection found, fill the record
        rec.t = t;
        rec.point = ray.origin + ray.direction * t;
        rec.setFaceNormal(ray, normal);
        rec.material = material;
        
        return true;
    }
};

// ############################################################################################
// Light source class - represents a point light in the scene
class Light {
public:
    Vector3f position;
    Vector3f color;
    float intensity;
    
    Light(const Vector3f& pos, const Vector3f& col, float intens) 
        : position(pos), color(col), intensity(intens) {}
};

// ############################################################################################
// A collection of hittable objects - represents the scene
class HittableList : public Hittable {
public:
    std::vector<Hittable*> objects;
    
    HittableList() {}
    
    // ############################################################################################
    // Add an object to the scene
    void add(Hittable* object) {
        objects.push_back(object);
    }
    
    // ############################################################################################
    // Clear all objects from the scene - frees memory
    void clear() {
        // Delete all objects in the list
        for (auto* obj : objects) {
            delete obj;
        }
        objects.clear();
    }
    
    // ############################################################################################
    // Ray-scene intersection test - checks all objects and returns the closest hit
    virtual bool hit(const Ray& ray, float tMin, float tMax, HitRecord& rec) const override {
        HitRecord tempRec;
        bool hitAnything = false;
        float closestSoFar = tMax;
        
        for (const auto* object : objects) {
            if (object->hit(ray, tMin, closestSoFar, tempRec)) {
                hitAnything = true;
                closestSoFar = tempRec.t;
                rec = tempRec;
            }
        }
        
        return hitAnything;
    }
    
    // ############################################################################################
    // Destructor - ensures all objects are properly deleted
    ~HittableList() {
        // Just delete the objects directly without calling clear() to avoid any potential issues
        for (auto* obj : objects) {
            delete obj;
        }
        // No need to call objects.clear() as the vector will be destroyed anyway
    }
};

// ############################################################################################
// Simple camera class for ray tracing
class Camera {
public:
    Vector3f origin;
    Vector3f lowerLeftCorner;
    Vector3f horizontal;
    Vector3f vertical;
    
    // ############################################################################################
    // Constructor - sets up camera parameters
    Camera(
        Vector3f lookFrom,
        Vector3f lookAt,
        Vector3f vup,
        float vfov, // vertical field-of-view in degrees
        float aspect // aspect ratio
    ) {
        float theta = vfov * 3.1415926f / 180.0f;
        float h = tan(theta / 2);
        float viewport_height = 2.0f * h;
        float viewport_width = aspect * viewport_height;
        
        Vector3f w = lookFrom - lookAt;
        w.Normalize();
        Vector3f u = vup.Cross(w);
        u.Normalize();
        Vector3f v = w.Cross(u);
        
        origin = lookFrom;
        horizontal = u * viewport_width;
        vertical = v * viewport_height;
        lowerLeftCorner = origin - horizontal * 0.5f - vertical * 0.5f - w;
    }
    
    // ############################################################################################
    // Generate a ray from the camera through pixel coordinates (s,t)
    Ray getRay(float s, float t) const {
        return Ray(origin, lowerLeftCorner + horizontal * s + vertical * t - origin);
    }
};

// ############################################################################################
// Ray tracer class - main rendering engine
class RayTracer {
public:
    // ############################################################################################
    // Constructor - initializes the ray tracer with image dimensions
    RayTracer(int width, int height)
        : imageWidth(width), imageHeight(height), maxReflectionDepth(3) {
        // Initialize the camera
        camera = new Camera(
            Vector3f(0, 0, 5),   // Look from
            Vector3f(0, 0, 0),    // Look at
            Vector3f(0, 1, 0),    // Up vector
            60.0f,               // Field of view
            static_cast<float>(width) / height  // Aspect ratio
        );
        
        // Initialize the scene
        world.clear();
    }
    
    // ############################################################################################
    // Destructor - cleans up memory
    ~RayTracer() {
        delete camera;
    }
    
    // ############################################################################################
    // Set camera position and properties
    void setCamera(const Vector3f& lookFrom, const Vector3f& lookAt, const Vector3f& up, 
                   float fov) {
        delete camera;
        camera = new Camera(lookFrom, lookAt, up, fov, 
                            static_cast<float>(imageWidth) / imageHeight);
    }

    // ############################################################################################
    // Add lights to the scene
    void addLight(const Vector3f& position, const Vector3f& color = Vector3f(1.0f, 1.0f, 1.0f), 
                 float intensity = 1.0f) {
        lights.push_back(Light(position, color, intensity));
    }
    
    // ############################################################################################
    // Add objects to the scene with materials
    void addSphere(const Vector3f& center, float radius, const Material& material) {
        world.add(new Sphere(center, radius, material));
    }
    
    void addBox(const Vector3f& min, const Vector3f& max, const Material& material) {
        world.add(new Box(min, max, material));
    }
    
    void addTriangle(const Vector3f& v0, const Vector3f& v1, const Vector3f& v2, 
                     const Material& material) {
        world.add(new Triangle(v0, v1, v2, material));
    }
    
    // ############################################################################################
    // Add a triangle mesh to the scene with a material
    void addMesh(const std::vector<Vector3f>& vertices, const std::vector<unsigned int>& indices,
                const Material& material) {
        for (size_t i = 0; i < indices.size(); i += 3) {
            if (i + 2 < indices.size()) {
                const Vector3f& v0 = vertices[indices[i]];
                const Vector3f& v1 = vertices[indices[i + 1]];
                const Vector3f& v2 = vertices[indices[i + 2]];
                addTriangle(v0, v1, v2, material);
            }
        }
    }
    
    // ############################################################################################
    // Set maximum reflection depth
    void setMaxReflectionDepth(int depth) {
        maxReflectionDepth = depth;
    }

    // ############################################################################################
    // Enable/disable reflections
    void setReflectionsEnabled(bool enabled) {
        reflectionsEnabled = enabled;
    }
    
    // ############################################################################################
    // Set background color
    void setBackgroundColor(const Vector3f& color) {
        backgroundColor = color;
    }
    
    // ############################################################################################
    // Clear the scene
    void clearScene() {
        world.clear();
        lights.clear();
    }
    
    // ############################################################################################
    // Render the scene and return pixel data
    std::vector<unsigned char> render() {
        std::vector<unsigned char> pixels(imageWidth * imageHeight * 3);
        
        #pragma omp parallel for // OpenMP parallelization for faster rendering
        for (int y = 0; y < imageHeight; ++y) {
            for (int x = 0; x < imageWidth; ++x) {
                float u = static_cast<float>(x) / (imageWidth - 1);
                float v = 1.0f - static_cast<float>(y) / (imageHeight - 1); // Flip y for correct orientation
                
                Ray ray = camera->getRay(u, v);
                Vector3f color;
                
                if (reflectionsEnabled) {
                    color = rayColorWithReflection(ray, world, maxReflectionDepth);
                } else {
                    color = rayColor(ray, world);
                }
                
                // Convert color to RGB bytes with gamma correction
                int idx = (y * imageWidth + x) * 3;
                // Apply simple gamma correction (gamma = 2.0)
                pixels[idx] = static_cast<unsigned char>(255.99 * sqrt(std::min(color.x, 1.0f)));
                pixels[idx + 1] = static_cast<unsigned char>(255.99 * sqrt(std::min(color.y, 1.0f)));
                pixels[idx + 2] = static_cast<unsigned char>(255.99 * sqrt(std::min(color.z, 1.0f)));
            }
        }
        
        return pixels;
    }
    
    // ############################################################################################
    // Save rendered image to a file (PPM format - simple binary format)
    bool saveToFile(const std::string& filename) {
        std::vector<unsigned char> pixels = render();
        
        // Open the file for writing
        std::ofstream file(filename, std::ios::binary);
        if (!file) {
            std::cerr << "Error: Could not open file for writing: " << filename << std::endl;
            return false;
        }
        
        // Write PPM header
        file << "P6\n" << imageWidth << " " << imageHeight << "\n255\n";
        
        // Write pixel data
        file.write(reinterpret_cast<char*>(pixels.data()), pixels.size());
        
        if (!file) {
            std::cerr << "Error: Failed to write image data" << std::endl;
            return false;
        }
        
        return true;
    }
    
    // ############################################################################################
    // Save rendered image to a simpler format (P3 PPM - ASCII format)
    bool saveToTextFile(const std::string& filename) {
        std::vector<unsigned char> pixels = render();
        
        // Open the file for writing
        std::ofstream file(filename);
        if (!file) {
            std::cerr << "Error: Could not open file for writing: " << filename << std::endl;
            return false;
        }
        
        // Write PPM header
        file << "P3\n" << imageWidth << " " << imageHeight << "\n255\n";
        
        // Write pixel data
        for (int y = 0; y < imageHeight; ++y) {
            for (int x = 0; x < imageWidth; ++x) {
                int idx = (y * imageWidth + x) * 3;
                file << static_cast<int>(pixels[idx]) << " "
                     << static_cast<int>(pixels[idx + 1]) << " "
                     << static_cast<int>(pixels[idx + 2]) << "\n";
            }
        }
        
        if (!file) {
            std::cerr << "Error: Failed to write image data" << std::endl;
            return false;
        }
        
        return true;
    }
    
    // ############################################################################################
    // Load scene from a simple text file format
    bool loadSceneFromFile(const std::string& filename) {
        std::ifstream file(filename);
        if (!file) {
            std::cerr << "Error: Could not open scene file: " << filename << std::endl;
            return false;
        }
        
        clearScene();
        
        std::string line;
        while (std::getline(file, line)) {
            std::stringstream ss(line);
            std::string type;
            ss >> type;
            
            if (type == "camera") {
                Vector3f lookFrom, lookAt, up;
                float fov;
                ss >> lookFrom.x >> lookFrom.y >> lookFrom.z
                   >> lookAt.x >> lookAt.y >> lookAt.z
                   >> up.x >> up.y >> up.z >> fov;
                setCamera(lookFrom, lookAt, up, fov);
            }
            else if (type == "light") {
                Vector3f position, color;
                float intensity;
                ss >> position.x >> position.y >> position.z
                   >> color.x >> color.y >> color.z >> intensity;
                addLight(position, color, intensity);
            }
            else if (type == "sphere") {
                Vector3f center, color;
                float radius, ambient, diffuse, specular, shininess, reflectivity;
                ss >> center.x >> center.y >> center.z >> radius
                   >> color.x >> color.y >> color.z
                   >> ambient >> diffuse >> specular >> shininess >> reflectivity;
                Material mat(color, ambient, diffuse, specular, shininess, reflectivity);
                addSphere(center, radius, mat);
            }
            else if (type == "box") {
                Vector3f min, max, color;
                float ambient, diffuse, specular, shininess, reflectivity;
                ss >> min.x >> min.y >> min.z
                   >> max.x >> max.y >> max.z
                   >> color.x >> color.y >> color.z
                   >> ambient >> diffuse >> specular >> shininess >> reflectivity;
                Material mat(color, ambient, diffuse, specular, shininess, reflectivity);
                addBox(min, max, mat);
            }
            else if (type == "triangle") {
                Vector3f v0, v1, v2, color;
                float ambient, diffuse, specular, shininess, reflectivity;
                ss >> v0.x >> v0.y >> v0.z
                   >> v1.x >> v1.y >> v1.z
                   >> v2.x >> v2.y >> v2.z
                   >> color.x >> color.y >> color.z
                   >> ambient >> diffuse >> specular >> shininess >> reflectivity;
                Material mat(color, ambient, diffuse, specular, shininess, reflectivity);
                addTriangle(v0, v1, v2, mat);
            }
            else if (type == "background") {
                Vector3f color;
                ss >> color.x >> color.y >> color.z;
                setBackgroundColor(color);
            }
            else if (type == "reflections") {
                int enabled, depth;
                ss >> enabled >> depth;
                setReflectionsEnabled(enabled != 0);
                setMaxReflectionDepth(depth);
            }
            else if (type == "off_model") {
                std::string filePath;
                Vector3f color;
                float ambient, diffuse, specular, shininess, reflectivity;
                ss >> filePath >> color.x >> color.y >> color.z
                   >> ambient >> diffuse >> specular >> shininess >> reflectivity;
                Material mat(color, ambient, diffuse, specular, shininess, reflectivity);
                
                // Use the first color component (red) as the scale factor
                float scale = color.x * 5.0f; // Scale between 0-5 based on the red component
                addMeshFromFile(*this, filePath, Vector3f(0, 0, 0), scale, mat);
            }
        }
        
        // Add a default light if none specified
        if (lights.empty()) {
            addLight(Vector3f(10, 10, 10));
        }
        
        return true;
    }
    
private:
    int imageWidth;
    int imageHeight;
    int maxReflectionDepth;
    bool reflectionsEnabled = false;
    Camera* camera;
    HittableList world;
    std::vector<Light> lights;
    Vector3f backgroundColor = Vector3f(0.2f, 0.2f, 0.4f);
    
    // ############################################################################################
    // Calculate color for a ray
    Vector3f rayColor(const Ray& ray, const HittableList& world) {
        HitRecord rec;
        
        // Check if ray hits anything in the world
        if (world.hit(ray, 0.001f, std::numeric_limits<float>::infinity(), rec)) {
            // Calculate lighting with shadows
            return calculateLighting(rec, ray, world);
        }
        
        // Ray didn't hit anything, return background color (gradient)
        Vector3f unitDir = ray.direction;
        float t = 0.5f * (unitDir.y + 1.0f);
        return Vector3f(1.0f, 1.0f, 1.0f) * (1.0f - t) + backgroundColor * t;
    }
    
    // ############################################################################################
    // Calculate lighting at a point with shadows
    Vector3f calculateLighting(const HitRecord& rec, const Ray& ray, const HittableList& world) {
        Vector3f resultColor(0.0f, 0.0f, 0.0f);
        
        // Ambient component
        Vector3f ambient = rec.material.color * rec.material.ambientCoef;
        resultColor = ambient;
        
        // For each light in the scene
        for (const auto& light : lights) {
            // Calculate direction from intersection point to light
            Vector3f lightDir = light.position - rec.point;
            float lightDistance = lightDir.length();
            lightDir.Normalize();
            
            // Check for shadows
            Ray shadowRay(rec.point + rec.normal * 0.001f, lightDir);
            HitRecord shadowRec;
            bool inShadow = world.hit(shadowRay, 0.001f, lightDistance - 0.001f, shadowRec);
            
            if (!inShadow) {
                // Diffuse component
                float diffuseFactor = std::max(rec.normal.Dot(lightDir), 0.0f);
                Vector3f diffuse = rec.material.color * light.color * diffuseFactor * 
                                   rec.material.diffuseCoef * light.intensity;
                
                // Specular component
                Vector3f viewDir = -ray.direction; // Already normalized
                Vector3f halfVector = (viewDir + lightDir);
                halfVector.Normalize();
                float specularFactor = std::pow(
                    std::max(rec.normal.Dot(halfVector), 0.0f), 
                    rec.material.shininess);
                Vector3f specular = light.color * specularFactor * 
                                   rec.material.specularCoef * light.intensity;
                
                // Add diffuse and specular to result
                resultColor = resultColor + diffuse + specular;
            }
        }
        
        // Clamp color values to [0, 1]
        resultColor.x = std::min(resultColor.x, 1.0f);
        resultColor.y = std::min(resultColor.y, 1.0f);
        resultColor.z = std::min(resultColor.z, 1.0f);
        
        return resultColor;
    }
    
    // ############################################################################################
    // Calculate color with reflection for a ray
    Vector3f rayColorWithReflection(const Ray& ray, const HittableList& world, int depth) {
        if (depth <= 0) {
            return Vector3f(0.0f, 0.0f, 0.0f);
        }
        
        HitRecord rec;
        
        // Check if ray hits anything in the world
        if (world.hit(ray, 0.001f, std::numeric_limits<float>::infinity(), rec)) {
            // Calculate direct lighting
            Vector3f directColor = calculateLighting(rec, ray, world);
            
            // Calculate reflection if needed
            if (rec.material.reflectivity > 0.0f) {
                Vector3f reflected = reflect(ray.direction, rec.normal);
                Ray reflectionRay(rec.point + rec.normal * 0.001f, reflected);
                Vector3f reflectionColor = rayColorWithReflection(reflectionRay, world, depth - 1);
                
                // Combine with reflection based on material reflectivity
                return directColor * (1.0f - rec.material.reflectivity) + 
                       reflectionColor * rec.material.reflectivity;
            }
            
            return directColor;
        }
        
        // Ray didn't hit anything, return background color (gradient)
        Vector3f unitDir = ray.direction;
        float t = 0.5f * (unitDir.y + 1.0f);
        return Vector3f(1.0f, 1.0f, 1.0f) * (1.0f - t) + backgroundColor * t;
    }
    
    // ############################################################################################
    // Calculate reflection vector
    Vector3f reflect(const Vector3f& v, const Vector3f& n) {
        return v - n * 2.0f * v.Dot(n);
    }
};

#endif // RAY_TRACER_H
