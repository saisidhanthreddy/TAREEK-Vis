#pragma once

#include "gl_renderer.h"
#include "core/crs_transform.h"
#include <QObject>
#include <QHash>
#include <QSet>
#include <QImage>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <functional>
#include <deque>

namespace simvis {

enum class TileSource { OpenStreetMap, Satellite, Topo };

struct TileKey {
    int x, y, z;
    bool operator==(const TileKey& o) const { return x == o.x && y == o.y && z == o.z; }
};

inline size_t qHash(const TileKey& k, size_t seed = 0) {
    return ::qHash(k.x, seed) ^ ::qHash(k.y, seed + 1) ^ ::qHash(k.z, seed + 2);
}

// Renders slippy map tiles (OSM, satellite, etc.) as OpenGL textured quads
class TileRenderer : public QObject, public GLRenderer {
    Q_OBJECT

public:
    explicit TileRenderer(QObject* parent = nullptr);
    ~TileRenderer() override;

    bool initialize() override;
    void cleanup() override;
    void render();

    void setEnabled(bool enabled);
    bool isEnabled() const { return enabled_; }

    void setTileSource(TileSource source);
    TileSource tileSource() const { return source_; }

    void setCRS(const CRSInfo& crs) { crs_ = crs; }
    const CRSInfo& crsInfo() const { return crs_; }

signals:
    void tileLoaded();  // emitted when a tile finishes downloading (triggers repaint)

private slots:
    void onTileDownloaded(QNetworkReply* reply);

private:
    struct CachedTile {
        GLuint texture = 0;
        TileKey key;
    };

    void fetchTile(const TileKey& key);
    GLuint uploadTexture(const QImage& image);
    void evictOldTiles();
    QString tileUrl(const TileKey& key) const;
    int calculateZoomLevel() const;

    bool enabled_ = false;
    TileSource source_ = TileSource::OpenStreetMap;
    CRSInfo crs_;

    // GL resources
    GLuint shaderProgram_ = 0;
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLint mvpLoc_ = -1;
    GLint texLoc_ = -1;

    // Tile cache
    QHash<TileKey, GLuint> tileTextures_;
    std::deque<TileKey> tileLru_;  // front = oldest
    static constexpr int MAX_CACHED_TILES = 200;

    // Pending downloads and images waiting for GL upload
    QSet<TileKey> pendingDownloads_;
    QHash<TileKey, QImage> pendingImages_;  // downloaded but not yet uploaded to GL
    QNetworkAccessManager* netManager_ = nullptr;
};

} // namespace simvis
