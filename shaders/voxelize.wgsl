// WebGPU surface-voxelization kernel used by the Electron preview path.
// Each invocation tests one triangle against candidate voxel cells; storage is an
// atomic byte grid so intersecting triangles can safely mark the same voxel.
struct Params {
  resolution: u32,
  triangleCount: u32,
  voxelSize: f32,
  _padding: u32,
  origin: vec3<f32>,
}

@group(0) @binding(0) var<storage, read> positions: array<vec4<f32>>;
@group(0) @binding(1) var<storage, read> indices: array<vec3<u32>>;
@group(0) @binding(2) var<storage, read_write> occupancy: array<atomic<u32>>;
@group(0) @binding(3) var<uniform> params: Params;

fn index3(cell: vec3<u32>) -> u32 {
  return cell.x + cell.y * params.resolution + cell.z * params.resolution * params.resolution;
}

// A conservative GPU preview kernel. Production export is performed by the
// native triangle/AABB SAT implementation, which shares this grid transform.
@compute @workgroup_size(8, 8, 1)
fn voxelize(@builtin(global_invocation_id) id: vec3<u32>) {
  if (id.z >= params.triangleCount || id.x >= params.resolution || id.y >= params.resolution) {
    return;
  }
  let tri = indices[id.z];
  let a = positions[tri.x].xyz;
  let b = positions[tri.y].xyz;
  let c = positions[tri.z].xyz;
  let minP = min(a, min(b, c));
  let maxP = max(a, max(b, c));
  let cellMin = vec3<u32>(clamp(floor((minP - params.origin) / params.voxelSize), vec3<f32>(0.0), vec3<f32>(f32(params.resolution - 1u))));
  let cellMax = vec3<u32>(clamp(floor((maxP - params.origin) / params.voxelSize), vec3<f32>(0.0), vec3<f32>(f32(params.resolution - 1u))));
  let cell = vec3<u32>(id.x, id.y, cellMin.z);
  if (id.x >= cellMin.x && id.x <= cellMax.x && id.y >= cellMin.y && id.y <= cellMax.y) {
    for (var z = cellMin.z; z <= cellMax.z; z++) {
      atomicStore(&occupancy[index3(vec3<u32>(id.x, id.y, z))], 1u);
    }
  }
}
