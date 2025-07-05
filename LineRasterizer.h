#ifndef LINE_RASTERIZER_H
#define LINE_RASTERIZER_H

#include <vector>
#include <cmath>
#include <algorithm>

using namespace std;

// ############################################################################################
// Pixel structure to represent a pixel in 2D space ( because of the 2D line rasterization )
// It contains x and y coordinates of the pixel
struct Pixel {
    int x, y;                                                   // (x, y) coordinates of the pixel
    Pixel(int inputX, int inputY) : x(inputX), y(inputY) {}     // Constructor to initialize pixel coordinates
    Pixel() : x(0), y(0) {}                                     // Default constructor
};


// ############################################################################################
// LineRasterizer class to implement various line rasterization algorithms
// includes Bresenham's, DDA, and Midpoint line algorithms
class LineRasterizer {
public:
    // ##############################
    // Bresenham's line algorithm
    static std::vector<Pixel> BresenhamLine(int x0, int y0, int x1, int y1) {
        int pixelCount = std::max(abs(x1 - x0), abs(y1 - y0)) + 1;      // Pre-calculate the maximum number of pixels needed
        std::vector<Pixel> pixels;  pixels.reserve(pixelCount);                                     
        
        int deltaX = abs(x1 - x0);      
        int deltaY = abs(y1 - y0);      
        
        int stepX = (x0 < x1) ? 1 : -1;
        int stepY = (y0 < y1) ? 1 : -1;
        
        
        int error = deltaX - deltaY;  

        
        // Loop until end point is reached
        while (true) {
            // Add current pixel to result
            pixels.push_back(Pixel(x0, y0));
            
            // Check if end point is reached
            if (x0 == x1 && y0 == y1) {
                break;
            }
            
            // Calculate error for next position
            int error_next = 2 * error;     // 2*deltaX - 2*deltaY;
            
            // Adjust error and current position horizontally
            if (error_next > -deltaY) {     // decision parameter is 2*deltaX - deltaY > 0
                error -= deltaY;            // update error with deltaY
                x0 += stepX;
            }
            
            // Adjust error and current position vertically
            if (error_next < deltaX) {      // decision parameter is 2*deltaY - deltaX > 0
                error += deltaX;
                y0 += stepY;
            }
        }
        
        return pixels;
    }

    // ############################################################################################
    // Implementation of DDA (Digital Differential Analyzer) Line Algorithm
    static std::vector<Pixel> DDALine(int x0, int y0, int x1, int y1) {
        std::vector<Pixel> pixels;
        
        // Calculate differences
        int deltaX = x1 - x0;
        int deltaY = y1 - y0;
        
        // Determine number of steps
        int steps = std::max(abs(deltaX), abs(deltaY));
        
        // Calculate increments
        float xIncrement = static_cast<float>(deltaX) / steps;
        float yIncrement = static_cast<float>(deltaY) / steps;
        
        // Initialize starting point
        float x = x0;
        float y = y0;
        
        // Add starting point
        pixels.push_back(Pixel(x0, y0));
        
        // Draw line by calculating each point
        for (int i = 0; i < steps; i++) {
            x += xIncrement;
            y += yIncrement;
            
            // Round to nearest pixel
            pixels.push_back(Pixel(round(x), round(y)));
        }
        
        return pixels;
    }
    
    // ###################################################################################
    // Midpoint line algorithm - returns the pixels of the line
    static std::vector<Pixel> MidpointLine(int x0, int y0, int x1, int y1) {
        std::vector<Pixel> pixels;

        // ######################
        // horizontal line
        if (y0 == y1) {
            int startX = std::min(x0, x1);
            int endX = std::max(x0, x1);
            
            for (int x = startX; x <= endX; x++) {
                pixels.push_back(Pixel(x, y0));
            }
            
            return pixels;
        }
        
        // ######################
        // vertical line
        if (x0 == x1) {
            int startY = std::min(y0, y1);
            int endY = std::max(y0, y1);
            
            for (int y=startY; y<=endY; y++) {
                pixels.push_back(Pixel(x0, y));
            }
            
            return pixels;
        }
        
        
        // We want to always draw from left to right
        if (x0 > x1) {
            std::swap(x0, x1);
            std::swap(y0, y1);
        }
        
        int deltaY = y1 - y0;
        int deltaX = x1 - x0;

        int ystep = 1;
        if (deltaY < 0) {
            ystep = -1;
        }

        deltaY = abs(deltaY);
        
        // ####################################
        // -1 <= m <= 1
        if (deltaY <= deltaX) {
            int decisionParameter = 2 * deltaY - deltaX;            // decision parameter = 2deltaY - deltaX
            int decisionNegative = 2 * deltaY;             
            int decisionPositive = 2 * (deltaY - deltaX);  
            
            int x = x0, y = y0;
            
            pixels.push_back(Pixel(x, y));
            
            while (x < x1) {
                if (decisionParameter <= 0) {
                    decisionParameter += decisionNegative;
                } 
                else {
                    decisionParameter += decisionPositive;
                    y += ystep;
                }
                x++;
                pixels.push_back(Pixel(x, y));
            }
        } 
        else {
            // Slope less than -1 or greater than 1     --> swap x and y
            int decisionParameter = 2 * deltaX - deltaY;        
            int decisionNegative = 2 * deltaX;
            int decisionPositive = 2 * (deltaX - deltaY);
            
            int x = x0;
            int y = y0;
            int endY = y1;
            
            pixels.push_back(Pixel(x, y));
            
            // We already know that x0 < x1 coz of our previous swap so now in this case we just need to decide wheter to increment x or stay at the same x
            // and increment y based on the slope
            int ystep = 1;
            if (y0 > y1) { 
                ystep = -1;         // slope is negative
            }
            
            while (y != endY) {
                if (decisionParameter <= 0) {
                    decisionParameter += decisionNegative;
                } else {
                    decisionParameter += decisionPositive;
                    x++;
                }
                y += ystep;
                pixels.push_back(Pixel(x, y));
            }
        }
        
        return pixels;
    }
};

#endif // LINE_RASTERIZER_H
