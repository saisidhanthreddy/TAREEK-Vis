#include "renderer/activity_density_builder.h"

#include "core/logger.h"
#include "data/network_index.h"
#include "data/vehicle_index.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace simvis {
namespace {

// Activities are binned onto a regular grid rather than drawn per link. Per
// link, blobs in a city centre pile up hundreds deep and additive blending
// clips the whole core to flat white; on a grid the overlap is bounded by the
// neighbouring cells, so the result reads as an actual density field.
constexpr int kGridResolution = 320;  // cells along the longer axis

// Safety net on buffer size. Six vertices x 8 floats per cell.
constexpr size_t kMaxCells = 200000;

struct Rgb { float r, g, b; };

// Qualitative palette: bright on the dark map background and deliberately
// clear of the blues used by the transit route layer.
constexpr Rgb kPalette[] = {
    {1.00f, 0.45f, 0.15f},  // orange
    {0.32f, 0.86f, 0.42f},  // green
    {0.98f, 0.83f, 0.22f},  // yellow
    {0.80f, 0.44f, 0.98f},  // violet
    {1.00f, 0.34f, 0.55f},  // pink
    {0.40f, 0.92f, 0.85f},  // teal
    {0.95f, 0.62f, 0.42f},  // salmon
    {0.72f, 0.90f, 0.35f},  // lime
};
constexpr size_t kPaletteSize = sizeof(kPalette) / sizeof(kPalette[0]);

// Common MATSim activity types keep a fixed color so the legend means the same
// thing across scenarios. Anything else cycles through the leftovers.
int pinnedPaletteSlot(const QString& name) {
    if (name.contains("home", Qt::CaseInsensitive))      return 0;
    if (name.contains("work", Qt::CaseInsensitive))      return 1;
    if (name.contains("edu", Qt::CaseInsensitive) ||
        name.contains("school", Qt::CaseInsensitive) ||
        name.contains("univ", Qt::CaseInsensitive))      return 2;
    if (name.contains("shop", Qt::CaseInsensitive))      return 3;
    if (name.contains("leisure", Qt::CaseInsensitive) ||
        name.contains("sport", Qt::CaseInsensitive))     return 4;
    return -1;
}

struct Cell {
    float x, y;
    uint32_t count;
};

} // namespace

ActivityDensityData buildActivityDensity(const VehicleIndex* vehicles,
                                         const NetworkIndex* network) {
    ActivityDensityData out;
    if (!vehicles || !network) return out;

    const auto& bounds = network->bounds();
    const double spanX = bounds.maxX - bounds.minX;
    const double spanY = bounds.maxY - bounds.minY;
    if (spanX <= 0.0 || spanY <= 0.0) return out;

    const double cellSize = std::max(spanX, spanY) / kGridResolution;
    const int gridW = static_cast<int>(spanX / cellSize) + 1;

    // 1. Bin activities into (activity type, grid cell).
    //
    // A plan chains legs, so the same activity shows up as the arrival of one
    // leg and the departure of the next. Counting arrivals only, plus the
    // departure of the very first leg, visits each activity exactly once.
    auto cellOfLink = [&](uint32_t linkId, uint64_t& cellKey) {
        const LinkRecord* link = network->getLink(linkId);
        if (!link) return false;
        const NodeRecord* from = network->getNode(link->fromNode);
        const NodeRecord* to = network->getNode(link->toNode);
        if (!from || !to) return false;

        // Activities sit on a link, so bin by its midpoint
        const double mx = 0.5 * (from->x + to->x);
        const double my = 0.5 * (from->y + to->y);
        const int gx = static_cast<int>((mx - bounds.minX) / cellSize);
        const int gy = static_cast<int>((my - bounds.minY) / cellSize);
        cellKey = static_cast<uint64_t>(gy) * static_cast<uint64_t>(gridW)
                + static_cast<uint64_t>(gx);
        return true;
    };

    std::unordered_map<uint16_t, std::unordered_map<uint64_t, uint32_t>> counts;

    const size_t personCount = vehicles->personCount();
    uint64_t key = 0;
    for (size_t i = 0; i < personCount; ++i) {
        const auto* trips = vehicles->personTrips(static_cast<uint32_t>(i));
        if (!trips || trips->empty()) continue;

        const auto& first = trips->front();
        if (first.fromActTypeId != TRIP_NO_ACT && first.fromLinkId != TRIP_NO_LINK &&
            cellOfLink(first.fromLinkId, key)) {
            counts[first.fromActTypeId][key]++;
        }
        for (const auto& trip : *trips) {
            if (trip.toActTypeId != TRIP_NO_ACT && trip.toLinkId != TRIP_NO_LINK &&
                cellOfLink(trip.toLinkId, key)) {
                counts[trip.toActTypeId][key]++;
            }
        }
    }
    if (counts.empty()) return out;

    // 2. Flatten to per-type cell lists, busiest cell first.
    struct TypeBucket {
        QString name;
        std::vector<Cell> cells;
        uint64_t total = 0;
    };
    std::vector<TypeBucket> buckets;
    buckets.reserve(counts.size());

    uint32_t maxCount = 0;
    for (const auto& entry : counts) {
        TypeBucket bucket;
        bucket.name = vehicles->actTypeString(entry.first);
        if (bucket.name.isEmpty()) bucket.name = QStringLiteral("(unnamed)");
        bucket.cells.reserve(entry.second.size());
        for (const auto& perCell : entry.second) {
            const auto gx = static_cast<double>(perCell.first % static_cast<uint64_t>(gridW));
            const auto gy = static_cast<double>(perCell.first / static_cast<uint64_t>(gridW));
            bucket.cells.push_back({
                static_cast<float>(bounds.minX + (gx + 0.5) * cellSize),
                static_cast<float>(bounds.minY + (gy + 0.5) * cellSize),
                perCell.second});
            bucket.total += perCell.second;
            maxCount = std::max(maxCount, perCell.second);
        }
        std::sort(bucket.cells.begin(), bucket.cells.end(),
                  [](const Cell& a, const Cell& b) { return a.count > b.count; });
        buckets.push_back(std::move(bucket));
    }
    if (maxCount == 0) return out;

    std::sort(buckets.begin(), buckets.end(),
              [](const TypeBucket& a, const TypeBucket& b) { return a.total > b.total; });

    // 3. Bound the buffer by keeping the busiest cells, proportionally per type
    //    so a small activity type is not squeezed out entirely.
    size_t totalCells = 0;
    for (const auto& bucket : buckets) totalCells += bucket.cells.size();

    if (totalCells > kMaxCells) {
        const double keepRatio = static_cast<double>(kMaxCells) / static_cast<double>(totalCells);
        size_t kept = 0;
        for (auto& bucket : buckets) {
            const size_t quota = std::max<size_t>(
                1, static_cast<size_t>(bucket.cells.size() * keepRatio));
            if (bucket.cells.size() > quota) bucket.cells.resize(quota);
            kept += bucket.cells.size();
        }
        out.truncated = true;
        LOG_WARN(QString("Activity density: drawing %1 of %2 cells "
                         "(lowest-count cells dropped to bound the vertex buffer)")
                     .arg(kept).arg(totalCells));
    }

    // 4. Blobs slightly wider than a cell, so neighbours blend into a smooth
    //    field without stacking deeply enough to clip to white.
    const float radius = static_cast<float>(cellSize * 0.85);

    // Log scale: activity counts are long-tailed, so a linear ramp would leave
    // everything but the few busiest cells invisible. The floor keeps rare
    // activity types on screen; the ceiling leaves headroom for the handful of
    // types that overlap in a city centre.
    const float logMax = std::log1p(static_cast<float>(maxCount));
    auto intensityOf = [logMax](uint32_t count) {
        const float t = std::log1p(static_cast<float>(count)) / logMax;
        return 0.05f + 0.25f * std::clamp(t, 0.0f, 1.0f);
    };

    size_t cellsToDraw = 0;
    for (const auto& bucket : buckets) cellsToDraw += bucket.cells.size();
    out.vertices.reserve(cellsToDraw * 6 * 8);

    size_t nextPaletteSlot = 0;
    std::vector<bool> slotUsed(kPaletteSize, false);

    for (const auto& bucket : buckets) {
        int slot = pinnedPaletteSlot(bucket.name);
        if (slot < 0 || slotUsed[static_cast<size_t>(slot)]) {
            slot = -1;
            for (size_t n = 0; n < kPaletteSize; ++n) {
                const size_t candidate = (nextPaletteSlot + n) % kPaletteSize;
                if (!slotUsed[candidate]) { slot = static_cast<int>(candidate); break; }
            }
            if (slot < 0) slot = static_cast<int>(nextPaletteSlot % kPaletteSize);
        }
        slotUsed[static_cast<size_t>(slot)] = true;
        nextPaletteSlot = static_cast<size_t>(slot) + 1;
        const Rgb color = kPalette[static_cast<size_t>(slot)];

        ActivityDensityLayer layer;
        layer.name = bucket.name;
        layer.r = color.r;
        layer.g = color.g;
        layer.b = color.b;
        layer.totalCount = bucket.total;
        // "pt interaction" is MATSim's boarding/alighting placeholder, not a
        // real activity. It is available in the menu but off by default, since
        // it tracks the transit network rather than where people spend time.
        layer.visible = !bucket.name.contains("pt interaction", Qt::CaseInsensitive);
        layer.firstVertex = out.vertices.size() / 8;

        for (const auto& cell : bucket.cells) {
            const float cx = cell.x;
            const float cy = cell.y;
            const float intensity = intensityOf(cell.count);

            auto pushVert = [&](float dx, float dy, float u, float v) {
                out.vertices.push_back(cx + dx);
                out.vertices.push_back(cy + dy);
                out.vertices.push_back(color.r);
                out.vertices.push_back(color.g);
                out.vertices.push_back(color.b);
                out.vertices.push_back(u);
                out.vertices.push_back(v);
                out.vertices.push_back(intensity);
            };

            pushVert(-radius,  radius, -1.0f,  1.0f);
            pushVert(-radius, -radius, -1.0f, -1.0f);
            pushVert( radius, -radius,  1.0f, -1.0f);
            pushVert(-radius,  radius, -1.0f,  1.0f);
            pushVert( radius, -radius,  1.0f, -1.0f);
            pushVert( radius,  radius,  1.0f,  1.0f);

            layer.cellCount++;
        }

        layer.vertexCount = out.vertices.size() / 8 - layer.firstVertex;
        if (layer.vertexCount > 0) out.layers.push_back(std::move(layer));
    }

    LOG_INFO(QString("Activity density built: %1 types, %2 cells, %3 vertices")
                 .arg(out.layers.size())
                 .arg(cellsToDraw)
                 .arg(out.vertices.size() / 8));
    return out;
}

} // namespace simvis
