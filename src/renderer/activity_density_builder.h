#pragma once

#include "renderer/activity_density_renderer.h"

namespace simvis {

class NetworkIndex;
class VehicleIndex;

// Builds the activity-density geometry from the loaded plans.
//
// Pure CPU work with no OpenGL calls, so it is safe to run on a worker thread.
// MainWindow does exactly that once, on data load — the render thread only ever
// uploads the finished buffer.
ActivityDensityData buildActivityDensity(const VehicleIndex* vehicles,
                                         const NetworkIndex* network);

} // namespace simvis
