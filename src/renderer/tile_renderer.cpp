#include "tile_renderer.h"
#include <QNetworkRequest>
#include <QDebug>
#include <cmath>
#include <vector>

namespace simvis {

// Vertex shader: position + texcoord
static const char* tileVertexShader = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;
uniform mat4 uMVP;
out vec2 vTexCoord;
void main() {
    gl_Position = uMVP * vec4(aPos, 0.0, 1.0);
    vTexCoord = aTexCoord;
}
)";

static const char* tileFragmentShader = R"(
#version 330 core
in vec2 vTexCoord;
uniform sampler2D uTex;
out vec4 FragColor;
void main() {
    FragColor = texture(uTex, vTexCoord);
}
)";

TileRenderer::TileRenderer(QObject* parent)
    : QObject(parent)
{
}

TileRenderer::~TileRenderer() {
    cleanup();
}

bool TileRenderer::initialize() {
    if (!GLRenderer::initialize()) return false;

    shaderProgram_ = compileShader(tileVertexShader, tileFragmentShader);
    if (!shaderProgram_) return false;

    mvpLoc_ = glGetUniformLocation(shaderProgram_, "uMVP");
    texLoc_ = glGetUniformLocation(shaderProgram_, "uTex");

    // Create VAO/VBO for tile quads (will be filled per-tile during render)
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    // 4 vertices * (2 pos + 2 texcoord) = 16 floats per quad
    glBufferData(GL_ARRAY_BUFFER, 16 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    // position
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // texcoord
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    // Network manager for tile downloads
    netManager_ = new QNetworkAccessManager(this);
    connect(netManager_, &QNetworkAccessManager::finished,
            this, &TileRenderer::onTileDownloaded);

    return true;
}

void TileRenderer::cleanup() {
    if (!initialized_) return;

    // Delete all cached textures
    for (auto it = tileTextures_.begin(); it != tileTextures_.end(); ++it) {
        glDeleteTextures(1, &it.value());
    }
    tileTextures_.clear();
    tileLru_.clear();

    if (vbo_) { glDeleteBuffers(1, &vbo_); vbo_ = 0; }
    if (vao_) { glDeleteVertexArrays(1, &vao_); vao_ = 0; }
    if (shaderProgram_) { glDeleteProgram(shaderProgram_); shaderProgram_ = 0; }

    GLRenderer::cleanup();
}

void TileRenderer::setEnabled(bool enabled) {
    enabled_ = enabled;
}

void TileRenderer::setTileSource(TileSource source) {
    if (source_ == source) return;
    source_ = source;

    // Clear cache when switching sources
    if (initialized_) {
        for (auto it = tileTextures_.begin(); it != tileTextures_.end(); ++it) {
            glDeleteTextures(1, &it.value());
        }
        tileTextures_.clear();
        tileLru_.clear();
        pendingDownloads_.clear();
    }
}

int TileRenderer::calculateZoomLevel() const {
    // Get viewport bounds in network coordinates
    BoundingBox vb = getViewportBounds();
    if (vb.width() <= 0) return 12;

    // Convert corners to WGS84
    auto [lonMin, latMin] = crs::toWgs84(vb.minX, vb.minY, crs_);
    auto [lonMax, latMax] = crs::toWgs84(vb.maxX, vb.maxY, crs_);

    double lonSpan = std::abs(lonMax - lonMin);
    if (lonSpan <= 0) return 12;

    // At zoom z, the world is 2^z tiles wide, each covering 360/2^z degrees
    // We want roughly viewport_width/256 tiles across
    double tilesAcross = std::max(1.0, viewportWidth_ / 256.0);
    int z = static_cast<int>(std::log2(360.0 / lonSpan * tilesAcross));
    return std::clamp(z, 1, 19);
}

void TileRenderer::render() {
    if (!enabled_ || !initialized_ || !crs_.isValid()) return;

    // Upload any pending images (GL context is current during render)
    if (!pendingImages_.isEmpty()) {
        for (auto it = pendingImages_.begin(); it != pendingImages_.end(); ++it) {
            GLuint tex = uploadTexture(it.value());
            if (tex) {
                evictOldTiles();
                tileTextures_.insert(it.key(), tex);
                tileLru_.push_back(it.key());
            }
        }
        pendingImages_.clear();
    }

    // Get viewport bounds in network coordinates
    BoundingBox vb = getViewportBounds();
    if (vb.width() <= 0 || vb.height() <= 0) return;

    // Convert viewport corners to WGS84
    auto [lonMin, latMin] = crs::toWgs84(vb.minX, vb.minY, crs_);
    auto [lonMax, latMax] = crs::toWgs84(vb.maxX, vb.maxY, crs_);

    // Ensure correct ordering
    if (lonMin > lonMax) std::swap(lonMin, lonMax);
    if (latMin > latMax) std::swap(latMin, latMax);

    int z = calculateZoomLevel();

    // Get tile range covering the viewport
    auto [txMin, tyMax] = crs::lonLatToTile(lonMin, latMin, z); // lat min -> tile y max
    auto [txMax, tyMin] = crs::lonLatToTile(lonMax, latMax, z);

    // Clamp and add 1-tile margin
    int n = 1 << z;
    txMin = std::max(0, txMin - 1);
    tyMin = std::max(0, tyMin - 1);
    txMax = std::min(n - 1, txMax + 1);
    tyMax = std::min(n - 1, tyMax + 1);

    // Limit tiles per frame to avoid stalling
    int tileCount = (txMax - txMin + 1) * (tyMax - tyMin + 1);
    if (tileCount > 100) return; // too many tiles, zoom mismatch

    // Set up GL state
    QMatrix4x4 mvp = projectionMatrix_ * viewMatrix_;

    glUseProgram(shaderProgram_);
    glUniformMatrix4fv(mvpLoc_, 1, GL_FALSE, mvp.constData());
    glUniform1i(texLoc_, 0);
    glActiveTexture(GL_TEXTURE0);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glBindVertexArray(vao_);

    // Pre-compute CRS positions for tile grid edges to ensure adjacent tiles
    // share exact vertex positions (eliminates seams/black lines).
    // tileEdgeX[i] = CRS x for tile column (txMin + i), tileEdgeY[j] = CRS y for tile row (tyMin + j)
    int cols = txMax - txMin + 1;
    int rows = tyMax - tyMin + 1;
    std::vector<double> tileEdgeX(cols + 1);
    std::vector<double> tileEdgeY(rows + 1);

    int numTiles = 1 << z;
    for (int i = 0; i <= cols; ++i) {
        double lon = static_cast<double>(txMin + i) / numTiles * 360.0 - 180.0;
        // Use the center latitude of the viewport for x conversion (lon varies, lat is reference)
        double centerLat = (latMin + latMax) * 0.5;
        auto [ex, ey] = crs::fromWgs84(lon, centerLat, crs_);
        tileEdgeX[i] = ex;
    }
    for (int j = 0; j <= rows; ++j) {
        // Tile row tyMin+j: top edge of that tile row
        // In OSM, tile y increases downward (north to south), so tile row ty has:
        //   top edge at ty, bottom edge at ty+1
        double latRad = std::atan(std::sinh(crs::PI * (1.0 - 2.0 * static_cast<double>(tyMin + j) / numTiles)));
        double lat = latRad * crs::RAD2DEG;
        double centerLon = (lonMin + lonMax) * 0.5;
        auto [ex, ey] = crs::fromWgs84(centerLon, lat, crs_);
        tileEdgeY[j] = ey;
    }

    for (int ty = tyMin; ty <= tyMax; ++ty) {
        int jRow = ty - tyMin;
        for (int tx = txMin; tx <= txMax; ++tx) {
            int iCol = tx - txMin;
            TileKey key{ tx, ty, z };

            // Get or fetch texture
            auto it = tileTextures_.find(key);
            if (it == tileTextures_.end()) {
                fetchTile(key);
                continue; // skip rendering until downloaded
            }

            GLuint tex = it.value();

            // Tile edges from pre-computed grid (no per-tile CRS conversion = no seams)
            // tileEdgeY[jRow] = top (higher lat = higher northing)
            // tileEdgeY[jRow+1] = bottom (lower lat = lower northing)
            float x0 = (float)tileEdgeX[iCol];
            float x1 = (float)tileEdgeX[iCol + 1];
            float yTop = (float)tileEdgeY[jRow];       // top of tile (high lat)
            float yBot = (float)tileEdgeY[jRow + 1];   // bottom of tile (low lat)

            // Build quad: 4 vertices [x, y, u, v] as triangle strip
            // OSM tile images have origin at top-left, so v=0 is top, v=1 is bottom
            float quad[16] = {
                x0, yBot, 0.0f, 1.0f,  // bottom-left  (v=1 = bottom of image)
                x1, yBot, 1.0f, 1.0f,  // bottom-right
                x0, yTop, 0.0f, 0.0f,  // top-left     (v=0 = top of image)
                x1, yTop, 1.0f, 0.0f,  // top-right
            };

            glBindBuffer(GL_ARRAY_BUFFER, vbo_);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(quad), quad);

            glBindTexture(GL_TEXTURE_2D, tex);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        }
    }

    glBindVertexArray(0);
    glDisable(GL_BLEND);
    glUseProgram(0);
}

void TileRenderer::fetchTile(const TileKey& key) {
    if (pendingDownloads_.contains(key)) return;
    if (pendingDownloads_.size() >= 8) return;  // limit concurrent downloads

    pendingDownloads_.insert(key);

    QNetworkRequest request;
    request.setUrl(QUrl(tileUrl(key)));
    request.setRawHeader("User-Agent", "TAREEK-Vis/1.0");
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                         QNetworkRequest::PreferCache);

    QNetworkReply* reply = netManager_->get(request);
    reply->setProperty("tileX", key.x);
    reply->setProperty("tileY", key.y);
    reply->setProperty("tileZ", key.z);
}

void TileRenderer::onTileDownloaded(QNetworkReply* reply) {
    reply->deleteLater();

    TileKey key;
    key.x = reply->property("tileX").toInt();
    key.y = reply->property("tileY").toInt();
    key.z = reply->property("tileZ").toInt();

    pendingDownloads_.remove(key);

    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "Tile download failed:" << reply->url() << reply->errorString();
        return;
    }

    QImage image;
    if (!image.loadFromData(reply->readAll())) {
        qDebug() << "Failed to decode tile image:" << reply->url();
        return;
    }

    // Store image for GL upload during next render() when context is current
    // Do NOT mirror - the UV coords already account for OpenGL's bottom-up convention
    pendingImages_.insert(key, image.convertToFormat(QImage::Format_RGBA8888));
    emit tileLoaded();
}

GLuint TileRenderer::uploadTexture(const QImage& image) {
    // Called from render() where GL context is current
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width(), image.height(),
                 0, GL_RGBA, GL_UNSIGNED_BYTE, image.constBits());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    return tex;
}

void TileRenderer::evictOldTiles() {
    while (tileTextures_.size() >= MAX_CACHED_TILES && !tileLru_.empty()) {
        TileKey oldest = tileLru_.front();
        tileLru_.pop_front();
        auto it = tileTextures_.find(oldest);
        if (it != tileTextures_.end()) {
            glDeleteTextures(1, &it.value());
            tileTextures_.erase(it);
        }
    }
}

QString TileRenderer::tileUrl(const TileKey& key) const {
    switch (source_) {
    case TileSource::OpenStreetMap:
        // Carto Voyager tiles: English labels by default, clean style
        return QString("https://basemaps.cartocdn.com/rastertiles/voyager/%1/%2/%3.png")
            .arg(key.z).arg(key.x).arg(key.y);
    case TileSource::Satellite:
        return QString("https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/%1/%2/%3")
            .arg(key.z).arg(key.y).arg(key.x);
    case TileSource::Topo:
        return QString("https://tile.opentopomap.org/%1/%2/%3.png")
            .arg(key.z).arg(key.x).arg(key.y);
    }
    return {};
}

} // namespace simvis
