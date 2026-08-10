#pragma once

#include "gl_renderer.h"
#include "data/network_index.h"
#include "data/vehicle_index.h"

namespace simvis {

// Renders an activity-density heatmap using additive blending and Gaussian particles.
class ActivityDensityRenderer : public GLRenderer {
public:
    ActivityDensityRenderer();
    ~ActivityDensityRenderer() override;

    bool initialize() override;
    void cleanup() override;

    // Set data sources
    void setIndices(NetworkIndex* network, VehicleIndex* vehicles);

    // Build the GPU buffers from the raw event data
    void buildBuffers();

    // Render the density heatmap
    void render();

    // Visibility toggle
    void setVisible(bool visible) { visible_ = visible; }
    bool isVisible() const { return visible_; }

private:
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