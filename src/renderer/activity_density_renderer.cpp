#include "activity_density_renderer.h"

namespace simvis {

// x, y, r, g, b, u, v, intensity
constexpr int VERTEX_SIZE = 8;

static const char* densityVertexShader = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec3 aColor;
layout (location = 2) in vec2 aUV;
layout (location = 3) in float aIntensity;

uniform mat4 uMVP;
out vec3 vColor;
out vec2 vUV;
out float vIntensity;

void main() {
    // Quads, not GL_POINTS: gl_PointSize is capped by the driver and the cap
    // varies, so the blob size would not be reliable across machines.
    gl_Position = uMVP * vec4(aPos, 0.0, 1.0);
    vColor = aColor;
    vUV = aUV;
    vIntensity = aIntensity;
}
)";

static const char* densityFragmentShader = R"(
#version 330 core
in vec3 vColor;
in vec2 vUV;
in float vIntensity;
out vec4 FragColor;

void main() {
    float dist = length(vUV);
    if (dist > 1.0) discard;

    // Gaussian falloff so each cell reads as a soft blob rather than a square
    float alpha = exp(-3.2 * dist * dist) * vIntensity;
    FragColor = vec4(vColor, alpha);
}
)";

ActivityDensityRenderer::ActivityDensityRenderer() = default;

ActivityDensityRenderer::~ActivityDensityRenderer() {
    cleanup();
}

bool ActivityDensityRenderer::initialize() {
    if (!GLRenderer::initialize()) return false;

    program_ = compileShader(densityVertexShader, densityFragmentShader);
    if (!program_) return false;
    mvpLoc_ = glGetUniformLocation(program_, "uMVP");

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);

    return true;
}

void ActivityDensityRenderer::cleanup() {
    if (vao_) { glDeleteVertexArrays(1, &vao_); vao_ = 0; }
    if (vbo_) { glDeleteBuffers(1, &vbo_); vbo_ = 0; }
    if (program_) { glDeleteProgram(program_); program_ = 0; }
    layers_.clear();
    GLRenderer::cleanup();
}

void ActivityDensityRenderer::setDensityData(const ActivityDensityData& data) {
    if (!initialized_) return;

    layers_ = data.layers;

    if (data.vertices.empty()) {
        layers_.clear();
        return;
    }

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(data.vertices.size() * sizeof(float)),
                 data.vertices.data(), GL_STATIC_DRAW);

    const GLsizei stride = VERTEX_SIZE * sizeof(float);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(5 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride, (void*)(7 * sizeof(float)));
    glEnableVertexAttribArray(3);

    glBindVertexArray(0);
}

void ActivityDensityRenderer::setLayerVisible(size_t index, bool visible) {
    if (index < layers_.size()) layers_[index].visible = visible;
}

void ActivityDensityRenderer::render() {
    if (!initialized_ || !visible_ || layers_.empty()) return;

    QMatrix4x4 mvp = projectionMatrix_ * viewMatrix_;

    glUseProgram(program_);
    glUniformMatrix4fv(mvpLoc_, 1, GL_FALSE, mvp.constData());

    // Thousands of blobs overlap exactly; depth testing would let them hide
    // each other, and depth writes would block the layers drawn after.
    GLboolean depthTestEnabled = GL_FALSE;
    glGetBooleanv(GL_DEPTH_TEST, &depthTestEnabled);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);  // additive: overlap reads as a hotspot

    glBindVertexArray(vao_);
    for (const auto& layer : layers_) {
        if (!layer.visible || layer.vertexCount == 0) continue;
        glDrawArrays(GL_TRIANGLES, static_cast<GLint>(layer.firstVertex),
                     static_cast<GLsizei>(layer.vertexCount));
    }
    glBindVertexArray(0);

    // Restore the state the other renderers expect
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    if (depthTestEnabled) glEnable(GL_DEPTH_TEST);
    glUseProgram(0);
}

} // namespace simvis
