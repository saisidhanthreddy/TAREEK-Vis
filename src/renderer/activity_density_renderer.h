#pragma once

#include "gl_renderer.h"
#include <vector>

namespace simvis {

class ActivityDensityRenderer : public GLRenderer {
public:
    ActivityDensityRenderer();
    ~ActivityDensityRenderer() override;

    bool initialize() override;
    void cleanup() override;

    // Direct data injection: receives pre-calculated [x, y, r, g, b] vertices
    void setDensityData(const std::vector<float>& vertexData);

    void render();

    void setVisible(bool visible) { visible_ = visible; }
    bool isVisible() const { return visible_; }

private:
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLuint program_ = 0;
    size_t vertexCount_ = 0;

    bool visible_ = false;
};

} // namespace simvis