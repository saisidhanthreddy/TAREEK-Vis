#pragma once

#include "gl_renderer.h"
#include <QString>
#include <cstdint>
#include <vector>

namespace simvis {

// One activity type ("home", "work", ...) drawn as its own colored layer.
// Vertices for a layer are contiguous in the shared buffer, so a layer can be
// hidden or shown without touching the GPU buffer.
struct ActivityDensityLayer {
    QString name;
    float r = 1.0f, g = 1.0f, b = 1.0f;
    size_t firstVertex = 0;
    size_t vertexCount = 0;
    uint32_t cellCount = 0;   // distinct links carrying this activity type
    uint64_t totalCount = 0;  // activities of this type across the population
    bool visible = true;
};

// Aggregation result handed over from the worker thread. Building this is the
// expensive part, so it happens once on data load, never in paintGL().
struct ActivityDensityData {
    std::vector<float> vertices;              // packed, grouped by layer
    std::vector<ActivityDensityLayer> layers;
    bool truncated = false;  // some low-count cells dropped to bound the buffer
};

class ActivityDensityRenderer : public GLRenderer {
public:
    ActivityDensityRenderer();
    ~ActivityDensityRenderer() override;

    bool initialize() override;
    void cleanup() override;

    // Uploads pre-aggregated geometry. Must be called with a current GL
    // context; MapWidget defers this to paintGL().
    void setDensityData(const ActivityDensityData& data);

    void render();

    void setVisible(bool visible) { visible_ = visible; }
    bool isVisible() const { return visible_; }

    const std::vector<ActivityDensityLayer>& layers() const { return layers_; }
    void setLayerVisible(size_t index, bool visible);

private:
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLuint program_ = 0;
    GLint mvpLoc_ = -1;

    std::vector<ActivityDensityLayer> layers_;
    bool visible_ = false;
};

} // namespace simvis
