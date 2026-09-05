#include <metal_stdlib>
using namespace metal;

// Metal equivalent for future native macOS preview integration. The command-line
// exporter currently uses the exact portable SAT path for deterministic output.
struct Params { uint resolution; uint triangleCount; float voxelSize; float3 origin; };

kernel void clear_voxels(device atomic_uint* occupancy [[buffer(0)]],
                         constant Params& params [[buffer(1)]],
                         uint id [[thread_position_in_grid]]) {
  const uint count = params.resolution * params.resolution * params.resolution;
  if (id < count) atomic_store_explicit(&occupancy[id], 0u, memory_order_relaxed);
}
