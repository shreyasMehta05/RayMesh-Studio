#ifndef SCANLINE_FILL_H
#define SCANLINE_FILL_H

#include <vector>
#include <list>
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include "LineRasterizer.h"

// ############################################################################################
// Edge structure for the scanline algorithm
struct Edge {
    int yMax;          // Maximum y-coordinate of the edge
    float xOfYMin;     // x-coordinate at the minimum y-coordinate
    float inverseSlope;// 1/slope of the edge
    
    Edge(int _yMax, float _xOfYMin, float _inverseSlope)
        : yMax(_yMax), xOfYMin(_xOfYMin), inverseSlope(_inverseSlope) {}
    
    // ############################################################################################
    // Comparison operator for sorting edges by x-coordinate
    bool operator<(const Edge& other) const {
        return xOfYMin < other.xOfYMin;
    }
};

// ############################################################################################
// ScanlineFill class implements polygon filling algorithms
class ScanlineFill {
public:
    // ############################################################################################
    // Fill a polygon using scanline algorithm
    static std::vector<Pixel> FillPolygon(const std::vector<std::pair<int, int>>& vertices) {
        std::vector<Pixel> filledPixels;
        
        if (vertices.size() < 3) {
            return filledPixels;
        }
        
        // Find the y-range of the polygon
        int yMin = vertices[0].second;
        int yMax = vertices[0].second;
        
        for (const auto& vertex : vertices) {
            yMin = std::min(yMin, vertex.second);
            yMax = std::max(yMax, vertex.second);
        }
        
        // Pre-allocate memory for filled pixels (rough estimation)
        int estimatedPixels = (yMax - yMin + 1) * vertices.size();
        filledPixels.reserve(estimatedPixels);
        
        // Create the edge table (ET)
        std::vector<std::list<Edge>> edgeTable(yMax - yMin + 1);
        
        // Build the edge table with improved handling of special cases
        for (size_t i = 0; i < vertices.size(); i++) {
            int x1 = vertices[i].first;
            int y1 = vertices[i].second;
            int x2 = vertices[(i + 1) % vertices.size()].first;
            int y2 = vertices[(i + 1) % vertices.size()].second;
            
            // Skip horizontal edges (they don't contribute to the edge table)
            if (y1 == y2) {
                // But we need to remember them for proper filling
                // Store this horizontal edge separately to handle special cases
                int minX = std::min(x1, x2);
                int maxX = std::max(x1, x2);
                for (int x = minX; x <= maxX; x++) {
                    filledPixels.push_back(Pixel(x, y1));
                }
                continue;
            }
            
            // Ensure we always process from min y to max y
            if (y1 > y2) {
                std::swap(x1, x2);
                std::swap(y1, y2);
            }
            
            // Calculate inverse slope (dx/dy)
            float inverseSlope = static_cast<float>(x2 - x1) / static_cast<float>(y2 - y1);
            
            // Add to edge table
            Edge edge(y2, static_cast<float>(x1), inverseSlope);
            edgeTable[y1 - yMin].push_back(edge);
        }
        
        // Active Edge List (AEL)
        std::list<Edge> activeEdges;
        
        // Scan from yMin to yMax
        for (int y = yMin; y <= yMax; y++) {
            // Add edges from ET to AEL
            for (auto& edge : edgeTable[y - yMin]) {
                activeEdges.push_back(edge);
            }
            
            // Remove edges from AEL if yMax == current scanline
            activeEdges.remove_if([y](const Edge& edge) { return edge.yMax <= y; });
            
            // Sort AEL by x-coordinate
            activeEdges.sort();
            
            // Fill pixels between edge pairs
            if (!activeEdges.empty()) {
                auto it = activeEdges.begin();
                while (it != activeEdges.end()) {
                    int x1 = std::round(it->xOfYMin);
                    ++it;
                    
                    if (it != activeEdges.end()) {
                        int x2 = std::round(it->xOfYMin);
                        ++it;
                        
                        // Fill pixels between x1 and x2
                        for (int x = x1; x <= x2; x++) {
                            filledPixels.push_back(Pixel(x, y));
                        }
                    }
                }
            }
            
            // Update x-coordinates of edges in AEL for the next scanline
            for (auto& edge : activeEdges) {
                edge.xOfYMin += edge.inverseSlope;
            }
        }
        
        return filledPixels;
    }
    
    // ############################################################################################
    // Handle polygon with holes (advanced function)
    static std::vector<Pixel> FillPolygonWithHoles(
        const std::vector<std::pair<int, int>>& outerPolygon,
        const std::vector<std::vector<std::pair<int, int>>>& holes) {
        
        // First fill the outer polygon
        std::vector<Pixel> filledPixels = FillPolygon(outerPolygon);
        
        // Create a fast lookup structure for filled pixels
        std::unordered_map<int, std::unordered_map<int, bool>> filledMap;
        for (const auto& pixel : filledPixels) {
            filledMap[pixel.y][pixel.x] = true;
        }
        
        // For each hole, get its filled pixels and remove them from the result
        for (const auto& hole : holes) {
            std::vector<Pixel> holePixels = FillPolygon(hole);
            for (const auto& pixel : holePixels) {
                filledMap[pixel.y][pixel.x] = false;
            }
        }
        
        // Rebuild the output pixels list
        std::vector<Pixel> result;
        for (const auto& yMap : filledMap) {
            for (const auto& xEntry : yMap.second) {
                if (xEntry.second) {
                    result.push_back(Pixel(xEntry.first, yMap.first));
                }
            }
        }
        
        return result;
    }
};

#endif // SCANLINE_FILL_H
