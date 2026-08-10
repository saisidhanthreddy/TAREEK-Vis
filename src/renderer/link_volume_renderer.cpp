#include "link_volume_renderer.h"
#include <algorithm>

namespace simvis {

constexpr int VERTEX_SIZE = 5; // x, y, r, g, b

static const char* volumeVertexShader = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec3 aColor;

uniform mat4 uMVP;
out vec3 vColor;

void main() {
    gl_Position = uMVP * vec4(aPos, 0.0, 1.0);
    vColor = aColor;
}
)";

static const char* volumeFragmentShader = R"(
#version 330 core
in vec3 vColor;
out vec4 FragColor;

void main() {
    FragColor = vec4(vColor, 0.85); // Slight transparency
}
)";

LinkVolumeRenderer::LinkVolumeRenderer() = default;

LinkVolumeRenderer::~LinkVolumeRenderer() {
    cleanup();
}

bool LinkVolumeRenderer::initialize() {
    if (!GLRenderer::initialize()) return false;

    program_ = compileShader(volumeVertexShader, volumeFragmentShader);
    if (!program_) return false;

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);

    return true;
}

void LinkVolumeRenderer::cleanup() {
    if (vao_) { glDeleteVertexArrays(1, &vao_); vao_ = 0; }
    if (vbo_) { glDeleteBuffers(1, &vbo_); vbo_ = 0; }
    if (program_) { glDeleteProgram(program_); program_ = 0; }
    GLRenderer::cleanup();
}

void LinkVolumeRenderer::setIndices(NetworkIndex* network, VehicleIndex* vehicles) {
    networkIndex_ = network;
    vehicleIndex_ = vehicles;
    buffersNeedUpdate_ = true;
}

void LinkVolumeRenderer::getHeatmapColor(float intensity, float& r, float& g, float& b) const {
    // Clamp intensity between 0.0 and 1.0
    intensity = std::clamp(intensity, 0.0f, 1.0f);
    
    // Simple Blue -> Cyan -> Green -> Yellow -> Red gradient
    if (intensity < 0.25f) {
        float t = intensity / 0.25f;
        r = 0.0f; g = t; b = 1.0f;
    } else if (intensity < 0.5f) {
        float t = (intensity - 0.25f) / 0.25f;
        r = 0.0f; g = 1.0f; b = 1.0f - t;
    } else if (intensity < 0.75f) {
        float t = (intensity - 0.5f) / 0.25f;
        r = t; g = 1.0f; b = 0.0f;
    } else {
        float t = (intensity - 0.75f) / 0.25f;
        r = 1.0f; g = 1.0f - t; b = 0.0f;
    }
}

void LinkVolumeRenderer::buildBuffers() {
    if (!networkIndex_ || !vehicleIndex_) return;

    // 1. Accumulate volumes per link
    std::unordered_map<uint32_t, uint32_t> linkVolumes;
    uint32_t maxVolume = 0;

    size_t vehicleCount = vehicleIndex_->vehicleCount();
    for (size_t i = 0; i < vehicleCount; ++i) {
        const VehicleTrajectory* traj = vehicleIndex_->trajectory(static_cast<VehicleId>(i));
        if (!traj) continue;
        
        for (const auto& seg : traj->segments) {
            linkVolumes[seg.linkId]++;
            if (linkVolumes[seg.linkId] > maxVolume) {
                maxVolume = linkVolumes[seg.linkId];
            }
        }
    }

    if (maxVolume == 0 || linkVolumes.empty()) {
        vertexCount_ = 0;
        buffersNeedUpdate_ = false;
        return;
    }

    // 2. Build OpenGL Geometry
    std::vector<float> vertices;
    vertices.reserve(linkVolumes.size() * 2 * VERTEX_SIZE);

    for (const auto& [linkId, volume] : linkVolumes) {
        const LinkRecord* link = networkIndex_->getLink(linkId);
        if (!link) continue;

        const NodeRecord* fromNode = networkIndex_->getNode(link->fromNode);
        const NodeRecord* toNode = networkIndex_->getNode(link->toNode);
        if (!fromNode || !toNode) continue;

        // Calculate color intensity
        float intensity = static_cast<float>(volume) / static_cast<float>(maxVolume);
        
        // Logarithmic scale often looks better for traffic volumes
        intensity = std::log10(1.0f + 9.0f * intensity); 
        
        float r, g, b;
        getHeatmapColor(intensity, r, g, b);

        // From vertex
        vertices.push_back(fromNode->x); vertices.push_back(fromNode->y);
        vertices.push_back(r); vertices.push_back(g); vertices.push_back(b);

        // To vertex
        vertices.push_back(toNode->x); vertices.push_back(toNode->y);
        vertices.push_back(r); vertices.push_back(g); vertices.push_back(b);
    }

    vertexCount_ = vertices.size() / VERTEX_SIZE;

    // 3. Upload to GPU
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, VERTEX_SIZE * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, VERTEX_SIZE * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
    buffersNeedUpdate_ = false;
}

void LinkVolumeRenderer::render() {
    // 1. Only check if initialized and visible first
    if (!initialized_ || !visible_) return;

    // 2. Build the buffers if they are flagged for an update
    if (buffersNeedUpdate_) {
        buildBuffers();
    }

    // 3. NOW check if we actually have any vertices to draw
    if (vertexCount_ == 0) return;

    QMatrix4x4 mvp = projectionMatrix_ * viewMatrix_;

    glUseProgram(program_);
    GLint mvpLoc = glGetUniformLocation(program_, "uMVP");
    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, mvp.constData());

    glLineWidth(3.0f); // Thicker lines for visibility
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glBindVertexArray(vao_);
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(vertexCount_));
    glBindVertexArray(0);
    
    glDisable(GL_BLEND);
    glUseProgram(0);
}

} // namespace simvis