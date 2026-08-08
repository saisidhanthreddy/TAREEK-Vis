#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <limits>

namespace simvis {

// Forward declarations
struct Node;
struct Link;
struct VehicleState;

// ============================================================================
// Configuration constants
// ============================================================================

// Binary file magic numbers for validation
constexpr uint32_t NETWORK_FILE_MAGIC = 0x4E455457;  // "NETW"
constexpr uint32_t FILE_VERSION = 1;

// ============================================================================
// Coordinate types
// ============================================================================

struct Point2D {
    double x = 0.0;
    double y = 0.0;

    Point2D() = default;
    Point2D(double x_, double y_) : x(x_), y(y_) {}
};

struct BoundingBox {
    double minX = std::numeric_limits<double>::max();
    double minY = std::numeric_limits<double>::max();
    double maxX = std::numeric_limits<double>::lowest();
    double maxY = std::numeric_limits<double>::lowest();

    void expand(double x, double y) {
        minX = std::min(minX, x);
        minY = std::min(minY, y);
        maxX = std::max(maxX, x);
        maxY = std::max(maxY, y);
    }

    void expand(const Point2D& p) {
        expand(p.x, p.y);
    }

    void expand(const BoundingBox& other) {
        minX = std::min(minX, other.minX);
        minY = std::min(minY, other.minY);
        maxX = std::max(maxX, other.maxX);
        maxY = std::max(maxY, other.maxY);
    }

    bool contains(double x, double y) const {
        return x >= minX && x <= maxX && y >= minY && y <= maxY;
    }

    bool intersects(const BoundingBox& other) const {
        return !(other.minX > maxX || other.maxX < minX ||
                 other.minY > maxY || other.maxY < minY);
    }

    double width() const { return maxX - minX; }
    double height() const { return maxY - minY; }
    Point2D center() const { return Point2D((minX + maxX) / 2, (minY + maxY) / 2); }
};

// ============================================================================
// Network data structures
// ============================================================================

// Compact node representation for binary storage
struct NodeRecord {
    uint32_t id;          // Internal numeric ID (mapped from string)
    float x;              // X coordinate
    float y;              // Y coordinate
    uint32_t padding;     // Alignment padding
};
static_assert(sizeof(NodeRecord) == 16, "NodeRecord must be 16 bytes");

// Compact link representation for binary storage
struct LinkRecord {
    uint32_t id;          // Internal numeric ID
    uint32_t fromNode;    // From node internal ID
    uint32_t toNode;      // To node internal ID
    float length;         // Link length in meters
    float freespeed;      // Free flow speed in m/s
    float capacity;       // Capacity
    uint8_t lanes;        // Number of lanes (max 255)
    uint8_t roadType;     // Road type enum
    uint16_t padding;     // Alignment padding
};
static_assert(sizeof(LinkRecord) == 28, "LinkRecord must be 28 bytes");

// Road types for coloring
enum class RoadType : uint8_t {
    Unknown = 0,
    Motorway,
    Primary,
    Secondary,
    Tertiary,
    Residential,
    Service,
    Other
};

RoadType parseRoadType(const std::string& type);

// Runtime node with full data
struct Node {
    uint32_t id;
    double x;
    double y;
    std::string originalId;  // Original string ID from XML
};

// Runtime link with full data
struct Link {
    uint32_t id;
    uint32_t fromNode;
    uint32_t toNode;
    double length;
    double freespeed;
    double capacity;
    uint8_t lanes;
    RoadType roadType;
    std::string originalId;

    // Cached geometry (from/to coordinates)
    Point2D fromPos;
    Point2D toPos;
};

// ============================================================================
// Event data structures
// ============================================================================

// Event types we care about for visualization
enum class EventType : uint8_t {
    Unknown = 0,
    VehicleEntersTraffic,
    VehicleLeavesTraffic,
    EnteredLink,
    LeftLink,
    PersonEntersVehicle,
    PersonLeavesVehicle,
    ActivityStart,
    ActivityEnd,
    Departure,           // MATSim "departure": person, link, legMode
    Arrival,             // MATSim "arrival": person, link, legMode
    TransitDriverStarts  // MATSim "TransitDriverStarts": driverId, vehicleId, transitLineId, transitRouteId
};

EventType parseEventType(const std::string& type);
EventType parseEventType(std::string_view type);
const char* eventTypeName(EventType type);

// ============================================================================
// Vehicle state for rendering
// ============================================================================

struct VehicleState {
    uint32_t vehicleId;
    uint32_t currentLink;
    float linkProgress;    // 0.0 = at from node, 1.0 = at to node
    float entryTime;
    float exitTime;
    bool active;

    // Interpolated position (computed each frame)
    float x;
    float y;
    float angle = 0.0f;            // Direction angle in radians (0 = east, pi/2 = north)

    // Speed-based color visualization
    float speedRatio = 1.0f;       // actualSpeed / freeflowSpeed (1.0 = full speed, 0.0 = stopped)
    float r = 0.2f;                // Per-vehicle color (default green)
    float g = 0.8f;
    float b = 0.2f;

    // Smooth link transition fields
    uint32_t previousLink = 0;         // Link vehicle just left (0 if none)
    float transitionStartTime = 0.0f;  // When the transition began
    float previousLinkAngle = 0.0f;    // Angle of the previous link (for smooth rotation)
    bool inTransition = false;         // Whether currently in a transition period
};

// ============================================================================
// Binary file headers
// ============================================================================

struct NetworkFileHeader {
    uint32_t magic;           // NETWORK_FILE_MAGIC
    uint32_t version;         // FILE_VERSION
    uint32_t nodeCount;
    uint32_t linkCount;
    uint64_t nodeOffset;      // Offset to node records
    uint64_t linkOffset;      // Offset to link records
    uint64_t stringTableOffset;
    uint64_t stringTableSize;
    BoundingBox bounds;
};

// ============================================================================
// String interning for ID mapping
// ============================================================================

class StringInterner {
public:
    uint32_t intern(const std::string& str);
    uint32_t intern(std::string_view sv);
    const std::string& getString(uint32_t id) const;
    uint32_t getId(const std::string& str) const;
    uint32_t getId(std::string_view sv) const;
    bool hasString(const std::string& str) const;
    bool hasString(std::string_view sv) const;

    size_t size() const { return strings_.size(); }
    const std::vector<std::string>& strings() const { return strings_; }

    void reserve(size_t capacity);
    void clear();

    // Serialization
    void writeTo(std::ostream& out) const;
    void readFrom(std::istream& in);

private:
    std::vector<std::string> strings_;
    std::unordered_map<std::string, uint32_t> stringToId_;
};

} // namespace simvis
