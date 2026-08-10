#pragma once

#include "gl_renderer.h"
#include "data/network_index.h"
#include "data/vehicle_index.h"
#include <vector>
#include <unordered_map>

namespace simvis {

// Renders a heatmap overlay showing total daily vehicle volume per link
class LinkVolumeRenderer : public GLRenderer {
public:
    LinkVolumeRenderer();
    ~LinkVolumeRenderer() override;

    bool initialize() override;
    void cleanup() override;

    // Set data sources
    void setIndices(NetworkIndex* network, VehicleIndex* vehicles);

    // Calculate volumes and build OpenGL buffers
    void buildBuffers();

    // Render the volume overlay
    void render();

    // Visibility toggle
    void setVisible(bool visible) { visible_ = visible; }
    bool isVisible() const { return visible_; }

private:
    // Helper to generate a blue-to-red color gradient based on volume intensity
    void getHeatmapColor(float intensity, float& r, float& g, float& b) const;

    NetworkIndex* networkIndex_ = nullptr;
    VehicleIndex* vehicleIndex_ = nullptr;

    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLuint program_ = 0;
    size_t vertexCount_ = 0;

    bool visible_ = false;
    bool buffersNeedUpdate_ = true;
};

} // namespace simvis