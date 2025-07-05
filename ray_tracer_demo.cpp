#include "RayTracer.h"
#include "include/math_utils.h"
#include "models/OFFReader.h"
#include <iostream>
#include <string>
#include <chrono>
#include <unistd.h> // For _exit function

// ############################################################################################
// Function to load a mesh from an OFF file and add it to the scene
void addMeshFromFile(RayTracer& rayTracer, const std::string& filename, const Vector3f& position, 
                     float scale, const Material& material) {
    // Add debug log to confirm function invocation
    std::cout << "addMeshFromFile called with filename: " << filename << ", position: " << position << ", scale: " << scale << std::endl;

    // Add debug logs to verify mesh loading
    std::cout << "Attempting to read OFF file: " << filename << std::endl;

    // Read the OFF file
    OffModel* model = readOffFile(const_cast<char*>(filename.c_str()));
    if (!model) {
        std::cerr << "Failed to read mesh file: " << filename << std::endl;
        return;
    }
    std::cout << "Successfully read OFF file: " << filename << std::endl;
    std::cout << "Number of vertices: " << model->numberOfVertices << ", Number of polygons: " << model->numberOfPolygons << std::endl;
    
    // ############################################################################################
    // Convert to vectors for processing
    std::vector<Vector3f> vertices;
    std::vector<unsigned int> indices;
    
    // ############################################################################################
    // Extract vertices
    for (int i = 0; i < model->numberOfVertices; i++) {
        float x = model->vertices[i].x;
        float y = model->vertices[i].y;
        float z = model->vertices[i].z;
        
        // Apply scale and translation
        Vector3f vertex(x * scale + position.x, 
                        y * scale + position.y, 
                        z * scale + position.z);
        vertices.push_back(vertex);
    }
    
    // ############################################################################################
    // Extract polygon indices (assuming triangles)
    for (int i = 0; i < model->numberOfPolygons; i++) {
        if (model->polygons[i].noSides >= 3) {
            // Add the first triangle (for triangles or n-gons)
            indices.push_back(model->polygons[i].v[0]);
            indices.push_back(model->polygons[i].v[1]);
            indices.push_back(model->polygons[i].v[2]);
            
            // If more than 3 sides, triangulate the n-gon (simple fan triangulation)
            for (int j = 3; j < model->polygons[i].noSides; j++) {
                indices.push_back(model->polygons[i].v[0]);
                indices.push_back(model->polygons[i].v[j-1]);
                indices.push_back(model->polygons[i].v[j]);
            }
        }
    }
    
    std::cout << "Mesh vertices and indices prepared. Adding to ray tracer..." << std::endl;
    // Add mesh to the ray tracer
    rayTracer.addMesh(vertices, indices, material);
    std::cout << "Mesh added successfully with " << vertices.size() << " vertices and " 
              << indices.size()/3 << " triangles." << std::endl;
    
    // Free the model
    FreeOffModel(model);
}

// ############################################################################################
// Simple test scene setup function
void setupSimpleScene(RayTracer& rayTracer) {
    // Add lights with different colors for more interesting illumination
    rayTracer.addLight(Vector3f(10, 10, 10), Vector3f(1.0f, 0.9f, 0.8f), 0.8f);    // Warm main light
    rayTracer.addLight(Vector3f(-10, 5, -5), Vector3f(0.4f, 0.5f, 0.9f), 0.6f);    // Blue fill light
    
    // ############################################################################################
    // Create materials with different properties
    Material redMaterial(Vector3f(0.9f, 0.2f, 0.2f), 0.1f, 0.7f, 0.4f, 32.0f, 0.1f);        // Slightly reflective red
    Material blueMaterial(Vector3f(0.2f, 0.3f, 0.9f), 0.1f, 0.7f, 0.6f, 48.0f, 0.0f);        // Shiny blue
    Material greenMaterial(Vector3f(0.2f, 0.8f, 0.2f), 0.1f, 0.8f, 0.2f, 16.0f, 0.0f);       // Matte green
    Material goldMaterial(Vector3f(0.9f, 0.7f, 0.2f), 0.2f, 0.5f, 0.8f, 64.0f, 0.3f);        // Gold with medium reflections
    Material mirrorMaterial(Vector3f(0.9f, 0.9f, 0.9f), 0.0f, 0.0f, 1.0f, 128.0f, 0.9f);     // Highly reflective mirror
    Material glassMaterial(Vector3f(0.8f, 0.9f, 1.0f), 0.1f, 0.2f, 0.8f, 64.0f, 0.6f);       // Glass-like material
    Material purpleMaterial(Vector3f(0.6f, 0.2f, 0.8f), 0.1f, 0.7f, 0.5f, 32.0f, 0.2f);      // Slightly reflective purple
    
    // ############################################################################################
    // Create a simpler floor - just one large green platform
    rayTracer.addBox(Vector3f(-10, -2, -10), Vector3f(10, -1.97f, 10), greenMaterial);
    
    // ############################################################################################
    // Add a central arrangement of spheres
    rayTracer.addSphere(Vector3f(0, 0, 0), 1.0f, redMaterial);                         // Center red sphere
    rayTracer.addSphere(Vector3f(2.2f, 0, -1), 0.7f, goldMaterial);                    // Gold sphere
    rayTracer.addSphere(Vector3f(-1.8f, -0.2f, -0.8f), 0.8f, blueMaterial);            // Blue sphere
    rayTracer.addSphere(Vector3f(0.8f, -0.9f, -2.5f), 1.1f, purpleMaterial);           // Purple sphere
    
    // ############################################################################################
    // Add a reflective sphere elevated on a pedestal
    rayTracer.addSphere(Vector3f(0, 2.5f, -2), 0.8f, mirrorMaterial);                  // Mirror sphere
    
    // Create a pedestal for the mirror sphere
    rayTracer.addBox(Vector3f(-0.4f, -2.0f, -2.4f), Vector3f(0.4f, 2.0f, -1.6f), blueMaterial);
    
    // ############################################################################################
    // Add a glass prism (tetrahedron)
    rayTracer.addTriangle(
        Vector3f(3.0f, -2.0f, -3.0f),
        Vector3f(4.5f, -2.0f, -4.0f),
        Vector3f(3.5f, 1.0f, -3.5f),
        glassMaterial);
    rayTracer.addTriangle(
        Vector3f(3.0f, -2.0f, -3.0f),
        Vector3f(3.5f, 1.0f, -3.5f),
        Vector3f(2.5f, -2.0f, -4.5f),
        glassMaterial);
    rayTracer.addTriangle(
        Vector3f(2.5f, -2.0f, -4.5f),
        Vector3f(3.5f, 1.0f, -3.5f),
        Vector3f(4.5f, -2.0f, -4.0f),
        glassMaterial);
    rayTracer.addTriangle(
        Vector3f(3.0f, -2.0f, -3.0f),
        Vector3f(2.5f, -2.0f, -4.5f),
        Vector3f(4.5f, -2.0f, -4.0f),
        glassMaterial);
    
    // ############################################################################################
    // Set a nice sky blue gradient background
    rayTracer.setBackgroundColor(Vector3f(0.3f, 0.5f, 0.8f));
    rayTracer.setReflectionsEnabled(true);
    rayTracer.setMaxReflectionDepth(4);
    
    // ############################################################################################
    // Position camera for a more interesting viewing angle
    rayTracer.setCamera(
        Vector3f(5, 3, 8),      // Look from
        Vector3f(0, 0, -1),     // Look at
        Vector3f(0, 1, 0),      // Up vector
        50.0f                   // Field of view
    );
}

// ############################################################################################
// Function to create a scene from one of the OFF models
void setupMeshScene(RayTracer& rayTracer, const std::string& modelName) {
    // Add lights
    rayTracer.addLight(Vector3f(20, 30, 20), Vector3f(1.0f, 1.0f, 1.0f), 1.0f);
    rayTracer.addLight(Vector3f(-20, 10, -10), Vector3f(0.5f, 0.5f, 0.7f), 0.5f);
    
    // Create materials
    Material modelMaterial(Vector3f(0.7f, 0.5f, 0.3f), 0.2f, 0.6f, 0.4f, 32.0f, 0.0f);
    Material floorMaterial(Vector3f(0.8f, 0.8f, 0.8f), 0.1f, 0.7f, 0.2f, 16.0f, 0.0f);
    
    // Add a floor
    rayTracer.addBox(Vector3f(-50, -10, -50), Vector3f(50, -9, 50), floorMaterial);
    
    // Create path to the model file
    std::string modelPath = "models/" + modelName + ".off";
    
    // Add the mesh from the OFF file
    addMeshFromFile(rayTracer, modelPath, Vector3f(0, 0, 0), 0.5f, modelMaterial);
    
    // Set background and reflections
    rayTracer.setBackgroundColor(Vector3f(0.2f, 0.3f, 0.4f));
    rayTracer.setReflectionsEnabled(true);
    rayTracer.setMaxReflectionDepth(2);
    
    // Position camera to get a good view of the model
    rayTracer.setCamera(
        Vector3f(0, 5, 20),   // Look from
        Vector3f(0, 0, 0),    // Look at
        Vector3f(0, 1, 0),    // Up vector
        45.0f                 // Field of view
    );
}

// ############################################################################################
// Function to load a scene from a file
void loadSceneFromFile(RayTracer& rayTracer, const std::string& filename) {
    if (!rayTracer.loadSceneFromFile(filename)) {
        std::cerr << "Failed to load scene from file: " << filename << std::endl;
        return;
    }
    std::cout << "Scene loaded from " << filename << std::endl;
}

// ############################################################################################
// Main function
int main(int argc, char** argv) {
    // Set resolution
    int imageWidth = 800;
    int imageHeight = 600;
    
    // Process command line arguments
    std::string outputFile = "render.ppm";
    std::string sceneType = "simple";
    std::string modelName = "1grm";
    std::string sceneFile = "";
    bool exitImmediately = false;  // New flag to bypass normal cleanup

    // ############################################################################################
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "--output" && i + 1 < argc) {
            outputFile = argv[++i];
        } 
        else if (arg == "--scene" && i + 1 < argc) {
            sceneType = argv[++i];
        }
        else if (arg == "--model" && i + 1 < argc) {
            modelName = argv[++i];
        }
        else if (arg == "--file" && i + 1 < argc) {
            sceneFile = argv[++i];
        }
        else if (arg == "--resolution" && i + 2 < argc) {
            imageWidth = std::stoi(argv[++i]);
            imageHeight = std::stoi(argv[++i]);
        }
        else if (arg == "--skip-cleanup") {
            exitImmediately = true;  // Set the flag if this option is provided
        }
        else if (arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
            std::cout << "Options:" << std::endl;
            std::cout << "  --output FILE       Output file name (default: render.ppm)" << std::endl;
            std::cout << "  --scene TYPE        Scene type: simple, mesh, file (default: simple)" << std::endl;
            std::cout << "  --model NAME        Model to use for mesh scene (default: 1grm)" << std::endl;
            std::cout << "  --file FILENAME     Scene description file for 'file' scene type" << std::endl;
            std::cout << "  --resolution W H    Image resolution (default: 800x600)" << std::endl;
            std::cout << "  --skip-cleanup      Skip memory cleanup to avoid potential issues" << std::endl;
            return 0;
        }
    }

    {
        // ############################################################################################
        // Create ray tracer in its own scope
        RayTracer rayTracer(imageWidth, imageHeight);

        // Setup the requested scene
        if (sceneType == "mesh") {
            setupMeshScene(rayTracer, modelName);
        } 
        else if (sceneType == "file" && !sceneFile.empty()) {
            loadSceneFromFile(rayTracer, sceneFile);
        }
        else {
            setupSimpleScene(rayTracer);
        }

        std::cout << "Rendering scene to " << outputFile << " at " 
                << imageWidth << "x" << imageHeight << " resolution..." << std::endl;
        
        // Start timing
        auto startTime = std::chrono::high_resolution_clock::now();
        
        // Render the image and save to file
        bool success = rayTracer.saveToFile(outputFile);
        
        // Calculate rendering time
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
        
        if (success) {
            std::cout << "Rendering completed in " << duration / 1000.0 << " seconds." << std::endl;
            std::cout << "Image saved to " << outputFile << std::endl;
        } else {
            std::cerr << "Failed to save image to " << outputFile << std::endl;
            return 1;
        }
        
        // If we're skipping cleanup, exit immediately before destructors run
        if (exitImmediately) {
            _exit(0);  // Force immediate exit without invoking destructors
        }
    }
    
    return 0;
}