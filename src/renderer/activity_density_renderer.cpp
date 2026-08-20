#include "activity_density_renderer.h"

namespace simvis {

// X, Y, R, G, B, U, V
constexpr int VERTEX_SIZE = 7; 

static const char* densityVertexShader = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec3 aColor;
layout (location = 2) in vec2 aUV;

uniform mat4 uMVP;
out vec3 vColor;
out vec2 vUV;

void main() {
    // We are no longer relying on gl_PointSize! 
    gl_Position = uMVP * vec4(aPos, 0.0, 1.0);
    vColor = aColor;
    vUV = aUV;
}
)";

static const char* densityFragmentShader = R"(
#version 330 core
in vec3 vColor;
in vec2 vUV;
out vec4 FragColor;

void main() {
    float dist = length(vUV);
    if (dist > 1.0) discard;

    // Gaussian falloff for a soft glowing particle
    float alpha = exp(-4.0 * dist * dist) * 0.15; 
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

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);

    return true;
}

void ActivityDensityRenderer::cleanup() {
    if (vao_) { glDeleteVertexArrays(1, &vao_); vao_ = 0; }
    if (vbo_) { glDeleteBuffers(1, &vbo_); vbo_ = 0; }
    if (program_) { glDeleteProgram(program_); program_ = 0; }
    GLRenderer::cleanup();
}

void ActivityDensityRenderer::setDensityData(const std::vector<float>& vertexData) {
    if (vertexData.empty() || !initialized_) {
        vertexCount_ = 0;
        return;
    }

    vertexCount_ = vertexData.size() / VERTEX_SIZE;

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, vertexData.size() * sizeof(float), vertexData.data(), GL_STATIC_DRAW);

    // X, Y
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, VERTEX_SIZE * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // R, G, B
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, VERTEX_SIZE * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // U, V
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, VERTEX_SIZE * sizeof(float), (void*)(5 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
}

void ActivityDensityRenderer::render() {
    if (!initialized_ || !visible_ || vertexCount_ == 0) return;

    QMatrix4x4 mvp = projectionMatrix_ * viewMatrix_;

    glUseProgram(program_);
    GLint mvpLoc = glGetUniformLocation(program_, "uMVP");
    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, mvp.constData());

    // Disable depth testing so thousands of perfectly overlapping points don't hide each other
    GLboolean depthTestEnabled;
    glGetBooleanv(GL_DEPTH_TEST, &depthTestEnabled);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    // Additive blending for the hotspot effect
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE); 

    glBindVertexArray(vao_);
    // Draw TRIANGLES instead of points to bypass driver size limitations
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertexCount_));
    glBindVertexArray(0);
    
    // Restore the state for the rest of the app
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    if (depthTestEnabled) glEnable(GL_DEPTH_TEST);
    glUseProgram(0);
}

} // namespace simvis