#include "activity_density_renderer.h"

namespace simvis {

constexpr int VERTEX_SIZE = 2; // Just x, y

static const char* densityVertexShader = R"(
#version 330 core
layout (location = 0) in vec2 aPos;

uniform mat4 uMVP;

void main() {
    gl_Position = uMVP * vec4(aPos, 0.0, 1.0);
    gl_PointSize = 25.0; // Pixel size of the soft particle
}
)";

static const char* densityFragmentShader = R"(
#version 330 core
out vec4 FragColor;

void main() {
    // Calculate distance from the center of the point sprite
    vec2 coord = gl_PointCoord - vec2(0.5);
    float dist = length(coord);
    
    if (dist > 0.5) discard;

    // Option A: Lower falloff (-15.0) for softer edges, higher base multiplier (0.03) for brighter cores
    float alpha = exp(-15.0 * dist * dist) * 0.03;
    
    // Brighter thermal fire color palette
    FragColor = vec4(1.0 * alpha, 0.4 * alpha, 0.1 * alpha, 1.0);
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

void ActivityDensityRenderer::setIndices(NetworkIndex* network, VehicleIndex* vehicles) {
    networkIndex_ = network;
    vehicleIndex_ = vehicles;
    buffersNeedUpdate_ = true;
}

void ActivityDensityRenderer::buildBuffers() {
    if (!networkIndex_ || !vehicleIndex_) return;

    std::vector<float> vertices;
    
    // Extract every activity location from the trips database
    for (size_t personId = 0; personId < vehicleIndex_->personCount(); ++personId) {
        const auto* trips = vehicleIndex_->personTrips(static_cast<uint32_t>(personId));
        if (!trips) continue;

        for (const auto& trip : *trips) {
            auto addPoint = [&](uint32_t linkId) {
                if (linkId == 0xFFFFFFFFu) return;
                const LinkRecord* link = networkIndex_->getLink(linkId);
                if (!link) return;
                
                // Activities generally happen at the 'toNode' of a link routing
                const NodeRecord* node = networkIndex_->getNode(link->toNode);
                if (!node) return;
                
                vertices.push_back(node->x);
                vertices.push_back(node->y);
            };

            // Add the start and end activities for each trip
            if (trip.fromActTypeId != 0xFFFF) addPoint(trip.fromLinkId);
            if (trip.toActTypeId != 0xFFFF) addPoint(trip.toLinkId);
        }
    }

    if (vertices.empty()) {
        vertexCount_ = 0;
        buffersNeedUpdate_ = false;
        return;
    }

    vertexCount_ = vertices.size() / VERTEX_SIZE;

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, VERTEX_SIZE * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
    buffersNeedUpdate_ = false;
}

void ActivityDensityRenderer::render() {
    if (!initialized_ || !visible_) return;

    if (buffersNeedUpdate_) {
        buildBuffers();
    }

    if (vertexCount_ == 0) return;

    QMatrix4x4 mvp = projectionMatrix_ * viewMatrix_;

    glUseProgram(program_);
    GLint mvpLoc = glGetUniformLocation(program_, "uMVP");
    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, mvp.constData());

    // Enable point sizes to use gl_PointSize in the shader
    glEnable(GL_PROGRAM_POINT_SIZE);

    // ADDITIVE BLENDING: This creates the glowing hotspot effect
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE); 

    glBindVertexArray(vao_);
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(vertexCount_));
    glBindVertexArray(0);
    
    // Restore normal alpha blending for the rest of the application
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_BLEND);
    glUseProgram(0);
}

} // namespace simvis