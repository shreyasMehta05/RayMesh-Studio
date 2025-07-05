#include <stdio.h>
#include <iostream>
#include <fstream>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

// Add GLM includes
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "file_utils.h"
#include "math_utils.h"
#include "models/OFFReader.h"
#include <vector>

// Include our new components
#include "MeshSlicer.h"
#include "LineRasterizer.h"
#include "ScanlineFill.h"
#include "RayTracer.h"

#define GL_SILENCE_DEPRECATION

/********************************************************************/
/*   Variables */

char theProgramTitle[] = "CG Assignment - 3D Graphics";
int theWindowWidth = 800, theWindowHeight = 600;
int theWindowPositionX = 40, theWindowPositionY = 40;
bool isFullScreen = false;
bool isAnimating = true;
float rotation = 0.0f;
GLuint VBO, VAO, IBO;
GLuint ShaderProgram; // Make ShaderProgram a global variable

// ############################################################################################
// ############################################################################################
/*			 Model variables			 */
OffModel* model = nullptr;
std::vector<Vector3f> modelVertices;        // --> store the loaded vertices
std::vector<Vector3f> modelNormals;         // --> store the model normals for each vertex
std::vector<Vector3f> faceNormals;          // --> calculate the face normals
std::vector<unsigned int> modelIndices;     // --> store the indices for the triangles
int vertexCount = 0;                        // --> number of vertices
int indexCount = 0;                         // --> number of indices
char modelPath[256] = "models/2oar.off";    // --> path name
bool modelLoaded = false;                   // --> flag to check if model is loaded || model == nullptr

// Render mode (0 = solid, 1 = wireframe)
int renderMode = 0;
// Orthographic projection size
float orthoSize = 2.0f;

// Shader file names
const char *pVSFileName = "shaders/shader.vs";
const char *pFSFileName = "shaders/shader.fs";
const char *pGSFileName = "shaders/slice.gs"; // Add explicit reference to geometry shader

// Standard camera position
Vector3f cameraPosition = Vector3f(0.0f, 0.0f, 5.0f);   // --> Camera position (eye) point
Vector3f cameraTarget = Vector3f(0.0f, 0.0f, 0.0f);     // --> Camera target (look-at) point
Vector3f cameraUp = Vector3f(0.0f, 1.0f, 0.0f);         // --> Camera up vector (Y-axis)

// Key state tracking
bool keys[1024] = {false};

// Shader uniform locations
GLuint gWorldLocation;            // To pass the world matrix
GLuint gModelMatrixLocation;      // To pass the model matrix
GLuint gNormalMatrixLocation;     // To pass the normal transformation matrix
GLuint gViewPosLocation;          // To pass camera position for specular calculation

// Material properties
Vector3f objectColor = Vector3f(0.8f, 0.8f, 0.8f);  // Object color --> Default is gray
GLuint objectColorLocation;       // Object color uniform location

// Slice-related uniform locations
GLuint sliceEnabledLocation;
GLuint numActivePlanesLocation;
GLuint slicePlanesLocation;

// GPU-based slicing variables
static bool useGPUSlicing = false;  // Toggle between CPU and GPU-based slicing - make it static
static bool slicingActive = false;  // Track when slicing should be active
int numSlicePlanes = 0;      // Number of slice planes being used
std::vector<Vector4f> slicePlanes = {
    {1.0f, 0.0f, 0.0f, 0.0f}, // Normal along X-axis, offset 0
    {0.0f, 1.0f, 0.0f, 0.0f}, // Normal along Y-axis, offset 0
    {0.0f, 0.0f, 1.0f, 0.0f}, // Normal along Z-axis, offset 0
    {0.0f, 0.0f, 0.0f, 0.0f}  // Default unused plane
};

// Add these global variables for slice planes
static int activePlaneCount = 1;
static Vector3f planeNormals[4] = { 
    Vector3f(0.0f, 1.0f, 0.0f),   // Default: Y-axis
    Vector3f(1.0f, 0.0f, 0.0f),   // X-axis
    Vector3f(0.0f, 0.0f, 1.0f),   // Z-axis
    Vector3f(1.0f, 1.0f, 1.0f)    // Diagonal
};
static float planeDistances[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

// Our new components
MeshSlicer meshSlicer;
RayTracer* rayTracer = nullptr;
std::vector<unsigned char> rayTracedImage;

// UI state for our new features
bool showMeshSlicingUI = false;
bool showLineRasterizerUI = false;
bool showScanlineFillUI = false;
bool showRayTracerUI = false;

// Line rasterization parameters
int startX = 100, startY = 100, endX = 300, endY = 300;
std::vector<Pixel> rasterizedLine;
int lineAlgorithm = 0; // 0: Bresenham, 1: DDA, 2: Midpoint

// Scanline fill parameters
std::vector<std::pair<int, int>> polygonVertices;
std::vector<Pixel> filledPolygonPixels;
bool editingPolygon = false;

// Ray tracing parameters
int rayTracerWidth = 320;
int rayTracerHeight = 240;
Vector3f lightPosition(5.0f, 5.0f, 5.0f);
bool rayTraceGenerateImage = false;
GLuint rayTraceTextureID = 0;



/********************************************************************
  Utility functions
 */

/* post: compute frames per second and display in window's title bar */
void computeFPS()
{
    static int frameCount = 0;
    static int lastFrameTime = 0;
    static char *title = NULL;
    int currentTime;

    if (!title)
        title = (char *)malloc((strlen(theProgramTitle) + 20) * sizeof(char));
    frameCount++;
    currentTime = 0;
    if (currentTime - lastFrameTime > 1000)
    {
        sprintf(title, "%s [ FPS: %4.2f ]",
                theProgramTitle,
                frameCount * 1000.0 / (currentTime - lastFrameTime));
        lastFrameTime = currentTime;
        frameCount = 0;
    }
}

// Calculate the normal of a face defined by three vertices
Vector3f CalculateFaceNormal(const Vector3f& v1, const Vector3f& v2, const Vector3f& v3) {
    Vector3f edge1 = v2 - v1;
    Vector3f edge2 = v3 - v1;
    Vector3f normal = edge1.Cross(edge2);
    normal.Normalize();
    return normal;
}

// Load the OFF model and process vertices, normals, and faces
bool LoadOFFModel(const char* filename)
{
    // Check if the model is already loaded and free it 
    if (model) {
        FreeOffModel(model);
        model = nullptr;
    }
    
    // Load the model from the OFF file
    model = readOffFile((char*)filename);
    if (!model) {
        std::cerr << "Failed to load model: " << filename << std::endl;
        return false;
    }
    
    // Clear Previous vertices, normals, and indices
    modelVertices.clear();
    modelNormals.clear();
    faceNormals.clear();
    modelIndices.clear();
    
    // Store original vertex positions without modification
    for (int i = 0; i < model->numberOfVertices; i++) {
        Vector3f vertex;
        vertex.x = model->vertices[i].x;
        vertex.y = model->vertices[i].y;
        vertex.z = model->vertices[i].z;
        modelVertices.push_back(vertex);
        
        // Initialize vertex normals to zero
        modelNormals.push_back(Vector3f(0.0f, 0.0f, 0.0f));
    }
    
    // Process faces and calculate face normals for each triangle
    for (int i = 0; i < model->numberOfPolygons; i++) {
        // Get the polygon (face) data
        Polygon& poly = model->polygons[i];
        
        if (poly.noSides >= 3) {
            // Get vertices for this face
            Vector3f& v0 = modelVertices[poly.v[0]];
            
            // For each triangle in the polygon
            for (int j = 1; j < poly.noSides - 1; j++) {
                Vector3f& v1 = modelVertices[poly.v[j]];
                Vector3f& v2 = modelVertices[poly.v[j + 1]];
                
                // Calculate face normal for this triangle
                Vector3f normal = CalculateFaceNormal(v0, v1, v2);
                faceNormals.push_back(normal);
                
                // Add indices for the triangle
                modelIndices.push_back(poly.v[0]);
                modelIndices.push_back(poly.v[j]);
                modelIndices.push_back(poly.v[j + 1]);
                
                // Accumulate normal to each vertex of this triangle
                modelNormals[poly.v[0]] += normal;
                modelNormals[poly.v[j]] += normal;
                modelNormals[poly.v[j + 1]] += normal;
                
                // Increment face count for vertices
                model->vertices[poly.v[0]].numIcidentTri++;
                model->vertices[poly.v[j]].numIcidentTri++;
                model->vertices[poly.v[j + 1]].numIcidentTri++;
            }
        }
    }
    
    // Normalize all vertex normals
    for (int i = 0; i < model->numberOfVertices; i++) {
        if (model->vertices[i].numIcidentTri > 0) {
            modelNormals[i].Normalize();
        }
        // Store normals in the model for reference
        model->vertices[i].normal = modelNormals[i];
    }
    
    vertexCount = modelVertices.size();
    indexCount = modelIndices.size();
    
    std::cout << "Model loaded: " << filename << std::endl;
    std::cout << "Vertices: " << vertexCount << std::endl;
    std::cout << "Faces: " << faceNormals.size() << std::endl;
    std::cout << "Indices: " << indexCount << std::endl;
    
    return true;
}

// Create a normalization matrix to center and scale the model
Matrix4f CreateNormalizationMatrix() {
    if (!model) return Matrix4f(); // Identity matrix if no model
    
    // Calculate center of the model
    float centerX = (model->minX + model->maxX) / 2.0f;
    float centerY = (model->minY + model->maxY) / 2.0f;
    float centerZ = (model->minZ + model->maxZ) / 2.0f;
    
    // Calculate scale factor
    float scale = 2.0f / model->extent;
    
    // Create translation matrix to center the model
    Matrix4f translationMatrix;
    translationMatrix.InitIdentity();
    translationMatrix.m[0][0] = 1.0f;
    translationMatrix.m[0][3] = -centerX;
    translationMatrix.m[1][1] = 1.0f;
    translationMatrix.m[1][3] = -centerY;
    translationMatrix.m[2][2] = 1.0f;
    translationMatrix.m[2][3] = -centerZ;
    translationMatrix.m[3][3] = 1.0f;
    
    // Create scaling matrix
    Matrix4f scaleMatrix;
    scaleMatrix.InitIdentity();
    scaleMatrix.m[0][0] = scale;
    scaleMatrix.m[1][1] = scale;
    scaleMatrix.m[2][2] = scale;
    scaleMatrix.m[3][3] = 1.0f;
    
    // Combine matrices: scale after translation
    return scaleMatrix * translationMatrix;
}

// Create an interleaved VBO with positions and normals
static void CreateVertexBuffer()
{
    // Create default triangle if no model is loaded
    if (!modelLoaded) {
        modelVertices.clear();
        modelNormals.clear();
        faceNormals.clear();
        modelIndices.clear();
        
        // Default triangle
        modelVertices.push_back(Vector3f(-1.0f, -1.0f, 0.0f));
        modelVertices.push_back(Vector3f(1.0f, -1.0f, 0.0f));
        modelVertices.push_back(Vector3f(0.0f, 1.0f, 0.0f));
        
        // Default normals pointing outward
        modelNormals.push_back(Vector3f(0.0f, 0.0f, 1.0f));
        modelNormals.push_back(Vector3f(0.0f, 0.0f, 1.0f));
        modelNormals.push_back(Vector3f(0.0f, 0.0f, 1.0f));
        
        modelIndices.push_back(0);
        modelIndices.push_back(1);
        modelIndices.push_back(2);
        
        // Default face normal
        faceNormals.push_back(Vector3f(0.0f, 0.0f, 1.0f));
        
        vertexCount = 3;
        indexCount = 3;
    }

    // Create interleaved vertex data (position, normal)
    std::vector<float> vertexData;
    for (int i = 0; i < vertexCount; i++) {
        // Position
        vertexData.push_back(modelVertices[i].x);
        vertexData.push_back(modelVertices[i].y);
        vertexData.push_back(modelVertices[i].z);
        
        // Normal
        vertexData.push_back(modelNormals[i].x);
        vertexData.push_back(modelNormals[i].y);
        vertexData.push_back(modelNormals[i].z);
    }

    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    // Create and populate the VBO with interleaved data
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertexData.size() * sizeof(float), vertexData.data(), GL_STATIC_DRAW);

    // Create and populate the IBO
    glGenBuffers(1, &IBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexCount * sizeof(unsigned int), modelIndices.data(), GL_STATIC_DRAW);

    // Position attribute
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    
    // Normal attribute
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

// Function to read shader files
std::string readFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filePath << std::endl;
        return "";
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// Update the shader initialization function to include the geometry shader
GLuint initShaderProgram() {
    // Create and compile vertex shader
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    std::string vertexShaderSource = readFile("shaders/shader.vs");
    const char* vertexShaderSourceCStr = vertexShaderSource.c_str();
    glShaderSource(vertexShader, 1, &vertexShaderSourceCStr, NULL);
    glCompileShader(vertexShader);
    checkShaderCompilationErrors(vertexShader, "vertex");

    // Create and compile fragment shader
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    std::string fragmentShaderSource = readFile("shaders/shader.fs");
    const char* fragmentShaderSourceCStr = fragmentShaderSource.c_str();
    glShaderSource(fragmentShader, 1, &fragmentShaderSourceCStr, NULL);
    glCompileShader(fragmentShader);
    checkShaderCompilationErrors(fragmentShader, "fragment");

    // Create and compile geometry shader
    GLuint geometryShader = glCreateShader(GL_GEOMETRY_SHADER);
    std::string geometryShaderSource = readFile("shaders/slice.gs");
    const char* geometryShaderSourceCStr = geometryShaderSource.c_str();
    glShaderSource(geometryShader, 1, &geometryShaderSourceCStr, NULL);
    glCompileShader(geometryShader);
    checkShaderCompilationErrors(geometryShader, "geometry");

    // Create shader program
    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glAttachShader(shaderProgram, geometryShader);  // Attach geometry shader
    glLinkProgram(shaderProgram);
    
    // Check for linking errors
    GLint success;
    GLchar infoLog[512];
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        std::cerr << "ERROR: Shader program linking failed\n" << infoLog << std::endl;
    }

    // Delete shaders as they're linked into the program and no longer needed
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    glDeleteShader(geometryShader);

    return shaderProgram;
}

using namespace std;

static void CompileShaders()
{
    ShaderProgram = initShaderProgram();

    if (ShaderProgram == 0)
    {
        fprintf(stderr, "Error creating shader program\n");
        exit(1);
    }

    glUseProgram(ShaderProgram);

    // Get uniform locations
    gWorldLocation = glGetUniformLocation(ShaderProgram, "gWorld");
    gModelMatrixLocation = glGetUniformLocation(ShaderProgram, "gModel");
    gNormalMatrixLocation = glGetUniformLocation(ShaderProgram, "gNormalMatrix");
    gViewPosLocation = glGetUniformLocation(ShaderProgram, "viewPos");
    objectColorLocation = glGetUniformLocation(ShaderProgram, "objectColor");
    
    // Get slice-related uniform locations
    sliceEnabledLocation = glGetUniformLocation(ShaderProgram, "sliceEnabled");
    numActivePlanesLocation = glGetUniformLocation(ShaderProgram, "numActivePlanes");
    slicePlanesLocation = glGetUniformLocation(ShaderProgram, "slicePlanes");
}

void onInit(int argc, char *argv[])
{
    // Set background color
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    
    // Try to load default model
    modelLoaded = LoadOFFModel(modelPath);
    
    CreateVertexBuffer();
    CompileShaders();
    
    // Set up depth testing
    glEnable(GL_DEPTH_TEST);
}

// Function to calculate normal matrix (inverse transpose of the model matrix)
Matrix4f CalculateNormalMatrix(const Matrix4f& modelMatrix) {
    // Create a copy of the model matrix for manipulation
    Matrix4f normalMatrix = modelMatrix;
    
    // We only need the 3x3 portion for normals, so set the translation part to zero
    normalMatrix.m[0][3] = 0.0f;
    normalMatrix.m[1][3] = 0.0f;
    normalMatrix.m[2][3] = 0.0f;
    
    // Calculate the inverse transpose
    normalMatrix.Inverse();
    normalMatrix.Transpose();
    
    return normalMatrix;
}

// Function to generate an orthographic projection matrix
Matrix4f CreateOrthographicMatrix(float left, float right, float bottom, float top, float nearZ, float farZ) {
    Matrix4f result;
    result.SetZero();
    
    // Calculate matrix elements for orthographic projection
    result.m[0][0] = 2.0f / (right - left);
    result.m[1][1] = 2.0f / (top - bottom);
    result.m[2][2] = -2.0f / (farZ - nearZ);
    result.m[0][3] = -(right + left) / (right - left);
    result.m[1][3] = -(top + bottom) / (top - bottom);
    result.m[2][3] = -(farZ + nearZ) / (farZ - nearZ);
    result.m[3][3] = 1.0f;
    
    return result;
}

// Function to create a rotation matrix
Matrix4f CreateRotationMatrix(float angle) {
    Matrix4f rotationX, rotationY;
    
    // X-axis rotation
    rotationX.SetZero();
    rotationX.m[0][0] = 1.0f;
    rotationX.m[1][1] = cosf(angle);
    rotationX.m[1][2] = -sinf(angle);
    rotationX.m[2][1] = sinf(angle);
    rotationX.m[2][2] = cosf(angle);
    rotationX.m[3][3] = 1.0f;
    
    // Y-axis rotation
    rotationY.SetZero();
    rotationY.m[0][0] = cosf(angle);
    rotationY.m[0][2] = sinf(angle);
    rotationY.m[1][1] = 1.0f;
    rotationY.m[2][0] = -sinf(angle);
    rotationY.m[2][2] = cosf(angle);
    rotationY.m[3][3] = 1.0f;
    
    // Combine rotations
    return rotationY * rotationX;
}
static void onDisplay()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Set rendering mode based on user selection
    if (renderMode == 1) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); // Wireframe
    } else {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); // Solid
    }

    // Create orthographic projection
    Matrix4f Projection = CreateOrthographicMatrix(-orthoSize, orthoSize, -orthoSize, orthoSize, -10.0f, 10.0f);
    
    // Simple view matrix (identity for now)
    Matrix4f View;
    View.InitIdentity();
    
    // Create model matrix with rotation and normalization
    Matrix4f Model = CreateRotationMatrix(rotation) * CreateNormalizationMatrix();
    
    // Calculate normal matrix
    Matrix4f NormalMatrix = CalculateNormalMatrix(Model);
    
    // Combine matrices for the final transformation
    Matrix4f WorldMatrix = Projection * View * Model;

    // Send matrices to shaders
    glUniformMatrix4fv(gWorldLocation, 1, GL_TRUE, &WorldMatrix.m[0][0]);
    glUniformMatrix4fv(gModelMatrixLocation, 1, GL_TRUE, &Model.m[0][0]);
    glUniformMatrix4fv(gNormalMatrixLocation, 1, GL_TRUE, &NormalMatrix.m[0][0]);
    
    // Send object color
    glUniform3f(objectColorLocation, objectColor.x, objectColor.y, objectColor.z);
    
    // Send camera position for lighting
    glUniform3f(gViewPosLocation, cameraPosition.x, cameraPosition.y, cameraPosition.z);

    // Set up GPU-based slicing - only apply if slicingActive is true
    if (useGPUSlicing && slicingActive) {
        // Enable GPU-based slicing
        glUniform1i(sliceEnabledLocation, 1); // Enable slicing
        
        // Use the global activePlaneCount directly
        glUniform1i(numActivePlanesLocation, activePlaneCount);
        
        // Set each slice plane from UI
        for (int i = 0; i < activePlaneCount; i++) {
            // Create the plane equation: normal and distance
            Vector4f plane(planeNormals[i].x, planeNormals[i].y, planeNormals[i].z, planeDistances[i]);
            
            // Normalize the normal component
            float length = sqrt(plane.x*plane.x + plane.y*plane.y + plane.z*plane.z);
            if (length > 0.001f) {
                plane.x /= length;
                plane.y /= length;
                plane.z /= length;
            }
            
            // Get uniform location for this specific plane
            char uniformName[32];
            sprintf(uniformName, "slicePlanes[%d]", i);
            GLint location = glGetUniformLocation(ShaderProgram, uniformName);
            
            if (location != -1) {
                // Set the plane uniform
                glUniform4f(location, plane.x, plane.y, plane.z, plane.w);
            } else {
                std::cerr << "ERROR: Could not find uniform location for " << uniformName << std::endl;
            }
        }
    } else {
        // Disable GPU-based slicing when not active
        glUniform1i(sliceEnabledLocation, 0);
    }

    // Draw the model
    glBindVertexArray(VAO);
    
    if (indexCount > 0) {
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
    } else {
        glDrawArrays(GL_TRIANGLES, 0, vertexCount);
    }
    
    glBindVertexArray(0);
    
    // Reset polygon mode to fill
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    // Check for rendering errors
    GLenum errorCode = glGetError();
    if (errorCode != GL_NO_ERROR) {
        fprintf(stderr, "OpenGL rendering error %d\n", errorCode);
    }
}

// Key callback function
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    // Forward keyboard input to ImGui
    ImGuiIO& io = ImGui::GetIO();
    
    // Only handle keyboard input if ImGui doesn't want to capture it
    if (!io.WantCaptureKeyboard) 
    {
        if (action == GLFW_PRESS) {
            keys[key] = true;
            
            // Handle single-press keys
            if (key == GLFW_KEY_R) {
                rotation = 0;
            }
            if (key == GLFW_KEY_Q || key == GLFW_KEY_ESCAPE) {
                glfwSetWindowShouldClose(window, true);
            }
        }
        else if (action == GLFW_RELEASE) {
            keys[key] = false;
        }
    }
}

// Mouse button callback
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    // Forward mouse data to ImGui
    ImGuiIO& io = ImGui::GetIO();
    bool down = (action == GLFW_PRESS);
    io.AddMouseButtonEvent(button, down);
}

// Mouse position callback
void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    // Forward mouse position to ImGui
    ImGuiIO& io = ImGui::GetIO();
    io.AddMousePosEvent((float)xpos, (float)ypos);
}

// Scroll callback
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    // Forward scroll event to ImGui
    ImGuiIO& io = ImGui::GetIO();
    io.AddMouseWheelEvent((float)xoffset, (float)yoffset);
    
    // Only process if ImGui doesn't want to capture it
    if (!io.WantCaptureMouse) {
        // Adjust orthographic zoom
        orthoSize -= (float)yoffset * 0.1f;
        if (orthoSize < 0.1f) orthoSize = 0.1f;
        if (orthoSize > 10.0f) orthoSize = 10.0f;
    }
}

// Initialize ImGui
void InitImGui(GLFWwindow *window)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
}

// Initialize a texture for ray tracing results
void InitRayTraceTexture() {
    if (rayTraceTextureID != 0) {
        glDeleteTextures(1, &rayTraceTextureID);
    }
    
    glGenTextures(1, &rayTraceTextureID);
    glBindTexture(GL_TEXTURE_2D, rayTraceTextureID);
    
    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    // Initialize with empty texture
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, rayTracerWidth, rayTracerHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    
    glBindTexture(GL_TEXTURE_2D, 0);
}

// Render ImGui UI
void RenderImGui()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // 3D Model Viewer panel
    ImGui::Begin("3D Model Viewer");
    
    ImGui::Text("Current model: %s", modelPath);
    ImGui::Text("Vertices: %d, Faces: %d", vertexCount, faceNormals.size());
    
    ImGui::InputText("Model Path", modelPath, sizeof(modelPath));
    
    if (ImGui::Button("Load Model")) {
        bool success = LoadOFFModel(modelPath);
        if (success) {
            modelLoaded = true;
            // Recreate vertex buffers
            glDeleteBuffers(1, &VBO);
            glDeleteBuffers(1, &IBO);
            glDeleteVertexArrays(1, &VAO);
            CreateVertexBuffer();
        }
    }
    
    ImGui::Separator();
    
    ImGui::Text("Rendering Options:");
    const char* renderModes[] = { "Solid", "Wireframe" };
    ImGui::Combo("Render Mode", &renderMode, renderModes, IM_ARRAYSIZE(renderModes));
    
    ImGui::ColorEdit3("Object Color", &objectColor.x);
    
    ImGui::Checkbox("Auto Rotate", &isAnimating);
    ImGui::SliderFloat("Rotation", &rotation, 0.0f, 6.28f);
    if (ImGui::Button("Reset Rotation")) {
        rotation = 0.0f;
    }
    
    ImGui::SliderFloat("Zoom", &orthoSize, 0.5f, 10.0f);
    
    ImGui::End();

    // Mesh Slicing UI
    if (ImGui::Begin("Mesh Slicing", &showMeshSlicingUI)) {
        // Store original model data
        static std::vector<Vector3f> originalVertices;
        static std::vector<Vector3f> originalNormals;
        static std::vector<unsigned int> originalIndices;
        static bool originalModelStored = false;
        
        // Store the original model on first open
        if (!originalModelStored && !modelVertices.empty()) {
            originalVertices = modelVertices;
            originalNormals = modelNormals;
            originalIndices = modelIndices;
            originalModelStored = true;
        }

        ImGui::Text("Slice the model with up to 4 planes");
        ImGui::Separator();
        
        // Toggle between CPU and GPU slicing - now using the global variable
        if (ImGui::Checkbox("Use GPU Slicing", &useGPUSlicing)) {
            // If switching to GPU mode, reset the model to original
            if (useGPUSlicing && originalModelStored) {
                modelVertices = originalVertices;
                modelNormals = originalNormals;
                modelIndices = originalIndices;
                
                vertexCount = modelVertices.size();
                indexCount = modelIndices.size();
                
                // Regenerate vertex buffers
                glDeleteBuffers(1, &VBO);
                glDeleteBuffers(1, &IBO);
                glDeleteVertexArrays(1, &VAO);
                CreateVertexBuffer();
            }
        }
        
        // Manage slicing planes
        static int selectedPlane = 0;
        static bool showPlaneVisuals = true;
        
        // Number of active planes slider
        if (ImGui::SliderInt("Number of Planes", &activePlaneCount, 1, 4)) {
            // Clamp selected plane to active plane count
            selectedPlane = std::min(selectedPlane, activePlaneCount - 1);
        }
        
        // Plane selection and visualization controls
        ImGui::Separator();
        ImGui::Text("Plane Controls");
        
        // Create radio buttons for plane selection
        for (int i = 0; i < activePlaneCount; i++) {
            char label[32];
            sprintf(label, "Plane %d", i + 1);
            ImGui::RadioButton(label, &selectedPlane, i);
            // Add colors for different planes
            ImGui::SameLine();
            ImGui::ColorButton("##color", 
                ImVec4(i == 0 ? 1.0f : 0.2f, i == 1 ? 1.0f : 0.2f, i == 2 ? 1.0f : 0.2f, 0.7f),
                ImGuiColorEditFlags_NoLabel, ImVec2(15, 15));
        }
        
        // Show the plane equation
        Vector3f& normal = planeNormals[selectedPlane];
        float& distance = planeDistances[selectedPlane];
        
        ImGui::Separator();
        ImGui::Text("Plane %d Equation: %.2fx + %.2fy + %.2fz + %.2f = 0", 
                   selectedPlane + 1, normal.x, normal.y, normal.z, distance);
        
        // Controls for the selected plane
        ImGui::Text("Adjust Normal Vector:");
        bool normalChanged = false;
        normalChanged |= ImGui::SliderFloat("X##normal", &normal.x, -1.0f, 1.0f);
        normalChanged |= ImGui::SliderFloat("Y##normal", &normal.y, -1.0f, 1.0f);
        normalChanged |= ImGui::SliderFloat("Z##normal", &normal.z, -1.0f, 1.0f);
        
        if (normalChanged) {
            // Normalize the normal vector after changes
            float length = sqrtf(normal.x * normal.x + 
                                normal.y * normal.y + 
                                normal.z * normal.z);
            if (length > 0.001f) {
                normal.x /= length;
                normal.y /= length;
                normal.z /= length;
            }
        }
        
        ImGui::Text("Adjust Distance:");
        ImGui::SliderFloat("D##distance", &distance, -3.0f, 3.0f);
        
        // Preset plane orientations
        if (ImGui::Button("Align with X-axis")) {
            normal = Vector3f(1.0f, 0.0f, 0.0f);
            distance = 0.0f;
        }
        ImGui::SameLine();
        if (ImGui::Button("Align with Y-axis")) {
            normal = Vector3f(0.0f, 1.0f, 0.0f);
            distance = 0.0f;
        }
        ImGui::SameLine();
        if (ImGui::Button("Align with Z-axis")) {
            normal = Vector3f(0.0f, 0.0f, 1.0f);
            distance = 0.0f;
        }
        
        // Advanced plane definition options
        if (ImGui::CollapsingHeader("Advanced Plane Definition")) {
            static Vector3f pointOnPlane(0.0f, 0.0f, 0.0f);
            
            ImGui::Text("Define plane using a point:");
            ImGui::SliderFloat("Point X", &pointOnPlane.x, -2.0f, 2.0f);
            ImGui::SliderFloat("Point Y", &pointOnPlane.y, -2.0f, 2.0f);
            ImGui::SliderFloat("Point Z", &pointOnPlane.z, -2.0f, 2.0f);
            
            if (ImGui::Button("Update Plane from Point and Normal")) {
                // Recalculate distance based on point and normal
                distance = -normal.Dot(pointOnPlane);
            }
            
            ImGui::Separator();
            static Vector3f planePoints[3] = {
                Vector3f(0.0f, 0.0f, 0.0f),
                Vector3f(1.0f, 0.0f, 0.0f),
                Vector3f(0.0f, 1.0f, 0.0f)
            };
            
            ImGui::Text("Or define plane using three points:");
            ImGui::InputFloat3("Point 1", &planePoints[0].x);
            ImGui::InputFloat3("Point 2", &planePoints[1].x);
            ImGui::InputFloat3("Point 3", &planePoints[2].x);
            
            if (ImGui::Button("Create Plane from 3 Points")) {
                // Calculate normal from three points
                Vector3f edge1 = planePoints[1] - planePoints[0];
                Vector3f edge2 = planePoints[2] - planePoints[0];
                normal = edge1.Cross(edge2);
                normal.Normalize();
                    
                // Calculate distance
                distance = -normal.Dot(planePoints[0]);
            }
        }
        
        ImGui::Separator();
        
        // Apply slicing button
        if (ImGui::Button("Apply Slicing", ImVec2(150, 0))) {
            // Set the slicingActive flag to true when button is clicked
            slicingActive = true;

            if (useGPUSlicing) {
                // For GPU-based slicing, we'll set uniforms and render with the geometry shader
                glUseProgram(ShaderProgram);
                
                // Enable slicing
                glUniform1i(sliceEnabledLocation, 1);
                
                // Set number of active planes
                glUniform1i(numActivePlanesLocation, activePlaneCount);
                
                // Set each slicing plane
                for (int i = 0; i < activePlaneCount; i++) {
                    // Create the plane equation: normal and distance
                    Vector4f plane(planeNormals[i].x, planeNormals[i].y, planeNormals[i].z, planeDistances[i]);
                    
                    // Normalize the normal component
                    float length = sqrt(plane.x*plane.x + plane.y*plane.y + plane.z*plane.z);
                    if (length > 0.001f) {
                        plane.x /= length;
                        plane.y /= length;
                        plane.z /= length;
                    }
                    
                    // Get uniform location for this specific plane
                    char uniformName[32];
                    sprintf(uniformName, "slicePlanes[%d]", i);
                    GLint location = glGetUniformLocation(ShaderProgram, uniformName);
                    
                    if (location != -1) {
                        // Set the plane uniform
                        glUniform4f(location, plane.x, plane.y, plane.z, plane.w);
                    } else {
                        std::cerr << "ERROR: Could not find uniform location for " << uniformName << std::endl;
                    }
                }
                
                // Reset vertex buffers but keep original vertices
                // This way the geometry shader can work with the original mesh
                if (originalModelStored) {
                    modelVertices = originalVertices;
                    modelNormals = originalNormals;
                    modelIndices = originalIndices;
                    
                    vertexCount = modelVertices.size();
                    indexCount = modelIndices.size();
                    
                    // Regenerate vertex buffers
                    glDeleteBuffers(1, &VBO);
                    glDeleteBuffers(1, &IBO);
                    glDeleteVertexArrays(1, &VAO);
                    CreateVertexBuffer();
                }
            } else {
                // CPU-based slicing
                // Create planes for slicing
                meshSlicer.ClearPlanes();
                
                // Add active planes
                for (int i = 0; i < activePlaneCount; i++) {
                    MeshSlicer::Plane slicePlane(planeNormals[i], planeDistances[i]);
                    meshSlicer.AddPlane(slicePlane);
                }
                
                // Slice the mesh using original model data
                std::vector<Vector3f> slicedVertices;
                std::vector<Vector3f> slicedNormals;
                std::vector<unsigned int> slicedIndices;
                
                meshSlicer.SliceMesh(
                    originalVertices, 
                    originalNormals, 
                    originalIndices, 
                    slicedVertices, 
                    slicedNormals, 
                    slicedIndices
                );
                
                // Update the model with sliced mesh
                modelVertices = slicedVertices;
                modelNormals = slicedNormals;
                modelIndices = slicedIndices;
                
                // Update counts
                vertexCount = modelVertices.size();
                indexCount = modelIndices.size();
                
                // Regenerate vertex buffers
                glDeleteBuffers(1, &VBO);
                glDeleteBuffers(1, &IBO);
                glDeleteVertexArrays(1, &VAO);
                CreateVertexBuffer();
                
                // Disable geometry shader slicing
                glUseProgram(ShaderProgram);
                glUniform1i(sliceEnabledLocation, 0); // Disable slicing in shader
            }
        }
        
        // Button to reset to original model
        ImGui::SameLine();
        if (ImGui::Button("Reset Model", ImVec2(120, 0))) {
            // Disable slicing when resetting the model
            slicingActive = false;

            // Reset plane parameters
            planeNormals[0] = Vector3f(0.0f, 1.0f, 0.0f);
            planeNormals[1] = Vector3f(1.0f, 0.0f, 0.0f);
            planeNormals[2] = Vector3f(0.0f, 0.0f, 1.0f);
            planeNormals[3] = Vector3f(1.0f, 1.0f, 1.0f);
            
            planeDistances[0] = planeDistances[1] = planeDistances[2] = planeDistances[3] = 0.0f;
            
            // Restore original model
            if (originalModelStored) {
                modelVertices = originalVertices;
                modelNormals = originalNormals;
                modelIndices = originalIndices;
                
                vertexCount = modelVertices.size();
                indexCount = modelIndices.size();
                
                // Regenerate vertex buffers
                glDeleteBuffers(1, &VBO);
                glDeleteBuffers(1, &IBO);
                glDeleteVertexArrays(1, &VAO);
                CreateVertexBuffer();
            }
            
            // Disable geometry shader slicing
            glUseProgram(ShaderProgram);
            glUniform1i(sliceEnabledLocation, 0); // Disable slicing in shader
        }

        // Show vertex and face counts
        ImGui::Text("Vertices: %d, Triangles: %d", vertexCount, indexCount / 3);
    }
    ImGui::End();

    // Line Rasterization UI
    if (ImGui::Begin("Line Rasterization", &showLineRasterizerUI)) {
        ImGui::Text("Draw lines between two points");
        
        ImGui::InputInt("X1", &startX);
        ImGui::InputInt("Y1", &startY);
        ImGui::InputInt("X2", &endX);
        ImGui::InputInt("Y2", &endY);
        
        const char* algorithms[] = { "Bresenham", "DDA", "Midpoint" };
        ImGui::Combo("Algorithm", &lineAlgorithm, algorithms, IM_ARRAYSIZE(algorithms));
        
        if (ImGui::Button("Rasterize Line")) {
            // Choose algorithm and rasterize line
            switch (lineAlgorithm) {
                case 0:
                    rasterizedLine = LineRasterizer::BresenhamLine(startX, startY, endX, endY);
                    break;
                case 1:
                    rasterizedLine = LineRasterizer::DDALine(startX, startY, endX, endY);
                    break;
                case 2:
                    rasterizedLine = LineRasterizer::MidpointLine(startX, startY, endX, endY);
                    break;
            }
        }
        
        // Draw the rasterized line
        if (!rasterizedLine.empty()) {
            ImGui::Text("Rasterized Line (%d pixels):", (int)rasterizedLine.size());
            
            ImVec2 canvasPos = ImGui::GetCursorScreenPos();
            ImVec2 canvasSize(400, 400);
                
            ImGui::InvisibleButton("canvas", canvasSize);
            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            
            draw_list->AddRectFilled(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), IM_COL32(50, 50, 50, 255));
            
            // Plot points
            for (const auto& pixel : rasterizedLine) {
                float x = canvasPos.x + pixel.x;
                float y = canvasPos.y + pixel.y;
                draw_list->AddRectFilled(ImVec2(x, y), ImVec2(x+1, y+1), IM_COL32(255, 255, 255, 255));
            }
            
            // Draw original line for comparison
            draw_list->AddLine(
                ImVec2(canvasPos.x + startX, canvasPos.y + startY),
                ImVec2(canvasPos.x + endX, canvasPos.y + endY),
                IM_COL32(0, 255, 0, 128),
                1.0f
            );
        }
        
    }
    ImGui::End();

    // Scanline Fill UI
    if (ImGui::Begin("Scanline Fill", &showScanlineFillUI)) {
        ImGui::Text("Define a polygon to fill");
        
        static int newX = 100, newY = 100;
        ImGui::InputInt("X", &newX);
        ImGui::InputInt("Y", &newY);
        
        if (ImGui::Button("Add Vertex")) {
            polygonVertices.push_back(std::make_pair(newX, newY));
        }
        
        ImGui::SameLine();
        
        if (ImGui::Button("Clear Polygon")) {
            polygonVertices.clear();
            filledPolygonPixels.clear();
        }
        
        if (polygonVertices.size() >= 3 && ImGui::Button("Fill Polygon")) {
            filledPolygonPixels = ScanlineFill::FillPolygon(polygonVertices);
        }
        
        // Draw the polygon and fill
        if (!polygonVertices.empty()) {
            ImGui::Text("Polygon with %d vertices", (int)polygonVertices.size());
            
            ImVec2 canvasPos = ImGui::GetCursorScreenPos();
            ImVec2 canvasSize(400, 400);
                
            ImGui::InvisibleButton("canvas", canvasSize);
            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            
            draw_list->AddRectFilled(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), IM_COL32(50, 50, 50, 255));
            
            // Draw filled pixels
            for (const auto& pixel : filledPolygonPixels) {
                float x = canvasPos.x + pixel.x;
                float y = canvasPos.y + pixel.y;
                draw_list->AddRectFilled(ImVec2(x, y), ImVec2(x+1, y+1), IM_COL32(0, 128, 255, 255));
            }
            
            // Draw polygon outline
            for (size_t i = 0; i < polygonVertices.size(); i++) {
                size_t j = (i + 1) % polygonVertices.size();
                draw_list->AddLine(
                    ImVec2(canvasPos.x + polygonVertices[i].first, canvasPos.y + polygonVertices[i].second),
                    ImVec2(canvasPos.x + polygonVertices[j].first, canvasPos.y + polygonVertices[j].second),
                    IM_COL32(255, 255, 0, 255),
                    1.0f
                );
            }
            
            // Draw vertices with indices
            for (size_t i = 0; i < polygonVertices.size(); i++) {
                float x = canvasPos.x + polygonVertices[i].first;
                float y = canvasPos.y + polygonVertices[i].second;
                
                // Draw point
                draw_list->AddCircleFilled(
                    ImVec2(x, y),
                    3.0f,
                    IM_COL32(255, 0, 0, 255)
                );
                
                // Draw point index and coordinates
                char buf[32];
                sprintf(buf, "P%zu (%d,%d)", i, polygonVertices[i].first, polygonVertices[i].second);
                draw_list->AddText(
                    ImVec2(x + 5, y + 5),
                    IM_COL32(255, 255, 255, 255),
                    buf
                );
            }
        }
        
    }
    ImGui::End();

    // Ray Tracer UI
    // if (ImGui::Begin("Ray Tracer", &showRayTracerUI)) {
    //     ImGui::Text("Simple Ray Tracer");
        
    //     ImGui::SliderInt("Width", &rayTracerWidth, 128, 800);
    //     ImGui::SliderInt("Height", &rayTracerHeight, 128, 600);
                
    //     ImGui::SliderFloat3("Light Position", &lightPosition.x, -10.0f, 10.0f);
        
    //     if (ImGui::Button("Initialize Ray Tracer")) {
    //         if (rayTracer) {
    //             delete rayTracer;
    //         }
                
    //         rayTracer = new RayTracer(rayTracerWidth, rayTracerHeight);
    //         InitRayTraceTexture();
            
    //         // Create materials for different objects
    //         Material redMaterial(Vector3f(1.0f, 0.2f, 0.2f), 0.1f, 0.7f, 0.3f, 32.0f, 0.0f);
    //         Material blueMaterial(Vector3f(0.2f, 0.2f, 0.8f), 0.1f, 0.7f, 0.3f, 32.0f, 0.0f);
    //         Material greenMaterial(Vector3f(0.2f, 0.8f, 0.2f), 0.1f, 0.7f, 0.3f, 32.0f, 0.0f);
    //         Material mirrorMaterial(Vector3f(0.9f, 0.9f, 0.9f), 0.1f, 0.1f, 0.9f, 64.0f, 0.8f);
            
    //         // Add some objects to the scene with materials
    //         rayTracer->addSphere(Vector3f(0, 0, 0), 1.0f, redMaterial);
    //         rayTracer->addBox(Vector3f(-1.5f, -0.5f, -1.0f), Vector3f(-0.5f, 0.5f, 1.0f), blueMaterial);
            
    //         // Add the loaded model to the ray tracer if available
    //         if (modelVertices.size() > 0 && modelIndices.size() > 0) {
    //             // rayTracer->addMesh(modelVertices, modelIndices, greenMaterial);
    //         }
            
    //         // Add a light
    //         rayTracer->addLight(lightPosition);
            
    //         // Enable reflections
    //         rayTracer->setReflectionsEnabled(true);
    //         rayTracer->setMaxReflectionDepth(3);
    //     }
        
    //     ImGui::SameLine();
        
    //     if (rayTracer && ImGui::Button("Render Scene")) {
    //         // Generate ray-traced image
    //         rayTracedImage = rayTracer->render();
               
    //         // Update the texture with new image data
    //         glBindTexture(GL_TEXTURE_2D, rayTraceTextureID);
    //         glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, rayTracerWidth, rayTracerHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, rayTracedImage.data());
    //         glBindTexture(GL_TEXTURE_2D, 0);
    //     }
        
    //     // Display ray-traced image if available
    //     if (rayTraceTextureID != 0) {
    //         ImGui::Text("Ray-traced result:");
    //         ImGui::Image((ImTextureID)(intptr_t)rayTraceTextureID, ImVec2(rayTracerWidth, rayTracerHeight));
            
    //         // Add save button
    //         if (ImGui::Button("Save Image")) {
    //             if (rayTracer) {
    //                 bool saved = rayTracer->saveToFile("render.ppm");
    //                 if (!saved) {
    //                     std::cerr << "Failed to save ray traced image" << std::endl;
    //                 }
    //             }
    //         }
    //     }
    // }
    // ImGui::End();

    // Feature Selection UI
    ImGui::Begin("Features");
    ImGui::Checkbox("Mesh Slicing", &showMeshSlicingUI);
    ImGui::Checkbox("Line Rasterization", &showLineRasterizerUI);
    ImGui::Checkbox("Scanline Fill", &showScanlineFillUI);
    // ImGui::Checkbox("Ray Tracer", &showRayTracerUI);
    ImGui::End();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

int main(int argc, char *argv[])
{
    // Initialize GLFW
    glfwInit();

    // Define version and compatibility settings
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

    // Create OpenGL window and context
    GLFWwindow *window = glfwCreateWindow(theWindowWidth, theWindowHeight, theProgramTitle, NULL, NULL);
    
    // Check for window creation failure
    if (!window)
    {
        glfwTerminate();
        return 1;
    }
    
    // Make the OpenGL context current
    glfwMakeContextCurrent(window);

    // Initialize GLEW
    glewExperimental = GL_TRUE;
    glewInit();
    printf("GL version: %s\n", glGetString(GL_VERSION));
    
    // Initialize the scene
    onInit(argc, argv);

    // Initialize ImGui
    InitImGui(window);
    
    // Set callback functions
    glfwSetKeyCallback(window, key_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    
    // Main event loop
    while (!glfwWindowShouldClose(window))
    {
        // Update rotation if auto-rotation is enabled
        if (isAnimating) {
            rotation += 0.01f;
        }
        
        // Render the 3D scene
        onDisplay();
        
        // Render UI
        RenderImGui();
        
        // Swap buffers and poll events
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    
    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    
    // Free resources
    if (model) {
        FreeOffModel(model);
    }
    
    if (rayTracer) {
        delete rayTracer;
    }
    
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &IBO);
    glDeleteVertexArrays(1, &VAO);
    
    if (rayTraceTextureID != 0) {
        glDeleteTextures(1, &rayTraceTextureID);
    }

    // Terminate GLFW
    glfwTerminate();
    return 0;
}
