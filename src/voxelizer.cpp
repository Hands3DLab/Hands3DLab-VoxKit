#include "voxelizer.hpp"
#include "glb_mesh_reader.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

using Vec3 = std::array<double, 3>;
using Tri = std::array<std::uint32_t, 3>;
using FloatColor = std::array<float, 4>;

Vec3 sub(const Vec3& a, const Vec3& b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
Vec3 add(const Vec3& a, const Vec3& b) { return {a[0] + b[0], a[1] + b[1], a[2] + b[2]}; }
Vec3 mul(const Vec3& a, double s) { return {a[0] * s, a[1] * s, a[2] * s}; }
double dot(const Vec3& a, const Vec3& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
Vec3 cross(const Vec3& a, const Vec3& b)
{
  return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}

double lengthSquared(const Vec3& a) { return dot(a, a); }

void projectTriangle(const Vec3& axis, const Vec3& a, const Vec3& b, const Vec3& c, double& minV, double& maxV)
{
  const double pa = dot(axis, a);
  const double pb = dot(axis, b);
  const double pc = dot(axis, c);
  minV = std::min({pa, pb, pc});
  maxV = std::max({pa, pb, pc});
}

bool axisOverlap(const Vec3& axis, const Vec3& half, const Vec3& a, const Vec3& b, const Vec3& c)
{
  if (lengthSquared(axis) < 1e-24) return true;
  double minV = 0.0, maxV = 0.0;
  projectTriangle(axis, a, b, c, minV, maxV);
  const double r = half[0] * std::abs(axis[0]) + half[1] * std::abs(axis[1]) + half[2] * std::abs(axis[2]);
  return !(minV > r || maxV < -r);
}

bool triangleBoxOverlap(const Vec3& boxCenter, const Vec3& half, Vec3 a, Vec3 b, Vec3 c)
{
  a = sub(a, boxCenter);
  b = sub(b, boxCenter);
  c = sub(c, boxCenter);

  for (int axis = 0; axis < 3; ++axis) {
    const double minV = std::min({a[axis], b[axis], c[axis]});
    const double maxV = std::max({a[axis], b[axis], c[axis]});
    if (minV > half[axis] || maxV < -half[axis]) return false;
  }

  const Vec3 e0 = sub(b, a);
  const Vec3 e1 = sub(c, b);
  const Vec3 e2 = sub(a, c);
  const Vec3 normal = cross(e0, e1);
  if (!axisOverlap(normal, half, a, b, c)) return false;

  constexpr Vec3 axes[3] = {{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}};
  for (const Vec3& edge : {e0, e1, e2}) {
    for (const Vec3& axisBase : axes) {
      if (!axisOverlap(cross(edge, axisBase), half, a, b, c)) return false;
    }
  }
  return true;
}

std::string lowerExtension(const std::string& path)
{
  std::string ext = std::filesystem::path(path).extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return ext;
}

void readObj(const std::string& path, std::vector<Vec3>& positions,
             std::vector<Tri>& triangles, std::vector<FloatColor>& colors,
             bool& hasColors)
{
  std::ifstream input(path);
  if (!input) throw std::runtime_error("failed to open OBJ: " + path);

  std::string line;
  while (std::getline(input, line)) {
    std::istringstream ss(line);
    std::string tag;
    ss >> tag;
    if (tag == "v") {
      Vec3 p{};
      ss >> p[0] >> p[1] >> p[2];
      positions.push_back(p);
      FloatColor color{1.0F, 1.0F, 1.0F, 1.0F};
      double r = 0.0, g = 0.0, b = 0.0, a = 1.0;
      if (ss >> r >> g >> b) {
        const bool hasAlpha = static_cast<bool>(ss >> a);
        const double scale = std::max({r, g, b}) > 1.0 ? 255.0 : 1.0;
        if (!hasAlpha) a = scale;
        color = {static_cast<float>(std::clamp(r / scale, 0.0, 1.0)),
                 static_cast<float>(std::clamp(g / scale, 0.0, 1.0)),
                 static_cast<float>(std::clamp(b / scale, 0.0, 1.0)),
                 static_cast<float>(std::clamp(a / scale, 0.0, 1.0))};
        hasColors = true;
      }
      colors.push_back(color);
    } else if (tag == "f") {
      std::vector<std::uint32_t> face;
      std::string token;
      while (ss >> token) {
        const std::size_t slash = token.find('/');
        const std::string head = token.substr(0, slash);
        int idx = std::stoi(head);
        if (idx < 0) idx = static_cast<int>(positions.size()) + idx + 1;
        if (idx <= 0 || static_cast<std::size_t>(idx) > positions.size()) throw std::runtime_error("OBJ face index out of range");
        face.push_back(static_cast<std::uint32_t>(idx - 1));
      }
      for (std::size_t i = 1; i + 1 < face.size(); ++i) triangles.push_back({face[0], face[i], face[i + 1]});
    }
  }
}

void readStl(const std::string& path, std::vector<Vec3>& positions, std::vector<Tri>& triangles)
{
  std::ifstream input(path, std::ios::binary);
  if (!input) throw std::runtime_error("failed to open STL: " + path);

  std::vector<char> bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
  if (bytes.size() >= 84) {
    std::uint32_t count = 0;
    std::memcpy(&count, bytes.data() + 80, sizeof(count));
    const std::size_t expected = 84u + static_cast<std::size_t>(count) * 50u;
    if (expected == bytes.size()) {
      const auto* record = reinterpret_cast<const unsigned char*>(bytes.data() + 84);
      for (std::uint32_t i = 0; i < count; ++i, record += 50) {
        Tri triangle{};
        for (int vertex = 0; vertex < 3; ++vertex) {
          Vec3 point{};
          for (int axis = 0; axis < 3; ++axis) {
            float coordinate = 0.0f;
            std::memcpy(&coordinate, record + 12 + vertex * 12 + axis * 4, sizeof(coordinate));
            point[axis] = static_cast<double>(coordinate);
          }
          triangle[vertex] = static_cast<std::uint32_t>(positions.size());
          positions.push_back(point);
        }
        triangles.push_back(triangle);
      }
      return;
    }
  }

  std::istringstream text(std::string(bytes.begin(), bytes.end()));
  std::string tag;
  std::vector<Vec3> facet;
  while (text >> tag) {
    if (tag != "vertex") continue;
    Vec3 point{};
    if (!(text >> point[0] >> point[1] >> point[2])) throw std::runtime_error("invalid ASCII STL vertex");
    facet.push_back(point);
    if (facet.size() == 3) {
      const std::uint32_t base = static_cast<std::uint32_t>(positions.size());
      positions.insert(positions.end(), facet.begin(), facet.end());
      triangles.push_back({base, base + 1, base + 2});
      facet.clear();
    }
  }
}

void readGlb(const std::string& path, std::vector<Vec3>& positions,
             std::vector<Tri>& triangles, std::vector<FloatColor>& colors,
             bool& hasColors)
{
  GlbScene scene = readGlbScene(path);
  for (const GlbPrimitive& primitive : scene.primitives) {
    const std::uint32_t base = static_cast<std::uint32_t>(positions.size());
    positions.insert(positions.end(), primitive.positions.begin(), primitive.positions.end());
    if (primitive.colors.empty()) {
      colors.insert(colors.end(), primitive.positions.size(), FloatColor{1.0F, 1.0F, 1.0F, 1.0F});
    }
    else {
      colors.insert(colors.end(), primitive.colors.begin(), primitive.colors.end());
    }
    hasColors = hasColors || primitive.hasColor;
    for (const auto& t : primitive.triangles) triangles.push_back({base + t[0], base + t[1], base + t[2]});
  }
}

void readMesh(const std::string& path, std::vector<Vec3>& positions,
              std::vector<Tri>& triangles, std::vector<FloatColor>& colors,
              bool& hasColors)
{
  const std::string ext = lowerExtension(path);
  if (ext == ".glb") readGlb(path, positions, triangles, colors, hasColors);
  else if (ext == ".obj") readObj(path, positions, triangles, colors, hasColors);
  else if (ext == ".stl") readStl(path, positions, triangles);
  else throw std::runtime_error("unsupported mesh format (currently .glb, .obj, and .stl): " + ext);
  if (positions.empty() || triangles.empty()) throw std::runtime_error("mesh contains no triangles");
}

ColorRGBA8 rgba8(const FloatColor& color)
{
  auto channel = [](float value) {
    return static_cast<std::uint8_t>(std::lround(std::clamp(value, 0.0F, 1.0F) * 255.0F));
  };
  return {channel(color[0]), channel(color[1]), channel(color[2]), channel(color[3])};
}

void projectVertexColors(VoxelGrid& grid, const std::vector<Vec3>& positions,
                         const std::vector<Tri>& triangles,
                         const std::vector<FloatColor>& colors)
{
  if (colors.size() != positions.size()) return;
  grid.colors.assign(grid.data.size(), ColorRGBA8{});
  std::vector<double> best(grid.data.size(), std::numeric_limits<double>::max());
  const Vec3 half{grid.voxelSize * 0.5, grid.voxelSize * 0.5, grid.voxelSize * 0.5};
  const std::size_t plane = static_cast<std::size_t>(grid.resolution) * grid.resolution;
  for (const Tri& tri : triangles) {
    const Vec3& a = positions[tri[0]];
    const Vec3& b = positions[tri[1]];
    const Vec3& c = positions[tri[2]];
    const Vec3 centroid = mul(add(add(a, b), c), 1.0 / 3.0);
    const Vec3 tmin{std::min({a[0], b[0], c[0]}), std::min({a[1], b[1], c[1]}), std::min({a[2], b[2], c[2]})};
    const Vec3 tmax{std::max({a[0], b[0], c[0]}), std::max({a[1], b[1], c[1]}), std::max({a[2], b[2], c[2]})};
    int lo[3], hi[3];
    for (int axis = 0; axis < 3; ++axis) {
      lo[axis] = std::max(0, static_cast<int>(std::floor((tmin[axis] - grid.origin[axis]) / grid.voxelSize)) - 1);
      hi[axis] = std::min(grid.resolution - 1, static_cast<int>(std::floor((tmax[axis] - grid.origin[axis]) / grid.voxelSize)) + 1);
    }
    FloatColor average{};
    for (int component = 0; component < 4; ++component) {
      average[component] = (colors[tri[0]][component] + colors[tri[1]][component]
                            + colors[tri[2]][component]) / 3.0F;
    }
    for (int z = lo[2]; z <= hi[2]; ++z) for (int y = lo[1]; y <= hi[1]; ++y) for (int x = lo[0]; x <= hi[0]; ++x) {
      const std::size_t index = static_cast<std::size_t>(x) + static_cast<std::size_t>(y) * grid.resolution + static_cast<std::size_t>(z) * plane;
      if (!grid.data[index]) continue;
      const Vec3 center{grid.origin[0] + (x + 0.5) * grid.voxelSize,
                        grid.origin[1] + (y + 0.5) * grid.voxelSize,
                        grid.origin[2] + (z + 0.5) * grid.voxelSize};
      if (!triangleBoxOverlap(center, half, a, b, c)) continue;
      const double distance = lengthSquared(sub(center, centroid));
      if (distance < best[index]) {
        best[index] = distance;
        grid.colors[index] = rgba8(average);
      }
    }
  }
}

} // namespace

VoxelizationMode parseVoxelizationMode(const std::string& value)
{
  if (value == "pixel") return VoxelizationMode::Pixel;
  if (value == "triangle") return VoxelizationMode::Triangle;
  if (value == "quad") return VoxelizationMode::Quad;
  throw std::runtime_error("voxel mode must be pixel, triangle, or quad");
}

const char* voxelizationModeName(VoxelizationMode mode)
{
  switch (mode) {
    case VoxelizationMode::Pixel: return "pixel";
    case VoxelizationMode::Triangle: return "triangle";
    case VoxelizationMode::Quad: return "quad";
  }
  return "unknown";
}

bool VoxelGrid::get(int x, int y, int z) const
{
  if (x < 0 || y < 0 || z < 0 || x >= resolution || y >= resolution || z >= resolution) return false;
  const std::size_t index = static_cast<std::size_t>(x) + static_cast<std::size_t>(y) * resolution
    + static_cast<std::size_t>(z) * resolution * resolution;
  return data[index] != 0;
}

void VoxelGrid::set(int x, int y, int z)
{
  if (x < 0 || y < 0 || z < 0 || x >= resolution || y >= resolution || z >= resolution) return;
  const std::size_t index = static_cast<std::size_t>(x) + static_cast<std::size_t>(y) * resolution
    + static_cast<std::size_t>(z) * resolution * resolution;
  data[index] = 1;
}

std::size_t VoxelGrid::occupied() const
{
  return static_cast<std::size_t>(std::count(data.begin(), data.end(), static_cast<std::uint8_t>(1)));
}

VoxelGrid Voxelizer::voxelize(const std::vector<Vec3>& positions, const std::vector<Tri>& triangles, int gridResolution)
{
  if (gridResolution <= 0 || gridResolution > 2048) throw std::runtime_error("resolution must be in 1..2048");
  if (positions.empty() || triangles.empty()) throw std::runtime_error("empty mesh");

  Vec3 minP = {std::numeric_limits<double>::max(), std::numeric_limits<double>::max(), std::numeric_limits<double>::max()};
  Vec3 maxP = {std::numeric_limits<double>::lowest(), std::numeric_limits<double>::lowest(), std::numeric_limits<double>::lowest()};
  for (const Vec3& p : positions) {
    for (int i = 0; i < 3; ++i) {
      minP[i] = std::min(minP[i], p[i]);
      maxP[i] = std::max(maxP[i], p[i]);
    }
  }

  const double span = std::max({maxP[0] - minP[0], maxP[1] - minP[1], maxP[2] - minP[2], 1e-9});
  const double padding = span * 0.005 + 1e-9;
  const double extent = span + 2.0 * padding;
  const double voxelSize = extent / static_cast<double>(gridResolution);
  Vec3 origin = {
    (minP[0] + maxP[0] - extent) * 0.5,
    (minP[1] + maxP[1] - extent) * 0.5,
    (minP[2] + maxP[2] - extent) * 0.5,
  };

  VoxelGrid grid;
  grid.resolution = gridResolution;
  grid.origin = origin;
  grid.voxelSize = voxelSize;
  grid.data.assign(static_cast<std::size_t>(gridResolution) * gridResolution * gridResolution, 0);

  const Vec3 half = {voxelSize * 0.5, voxelSize * 0.5, voxelSize * 0.5};
  for (const Tri& tri : triangles) {
    if (tri[0] >= positions.size() || tri[1] >= positions.size() || tri[2] >= positions.size()) continue;
    const Vec3 a = positions[tri[0]];
    const Vec3 b = positions[tri[1]];
    const Vec3 c = positions[tri[2]];
    if (lengthSquared(cross(sub(b, a), sub(c, a))) < 1e-24) continue;

    Vec3 tmin = {std::min({a[0], b[0], c[0]}), std::min({a[1], b[1], c[1]}), std::min({a[2], b[2], c[2]})};
    Vec3 tmax = {std::max({a[0], b[0], c[0]}), std::max({a[1], b[1], c[1]}), std::max({a[2], b[2], c[2]})};

    int lo[3];
    int hi[3];
    for (int axis = 0; axis < 3; ++axis) {
      lo[axis] = std::max(0, static_cast<int>(std::floor((tmin[axis] - origin[axis]) / voxelSize)) - 1);
      hi[axis] = std::min(gridResolution - 1, static_cast<int>(std::floor((tmax[axis] - origin[axis]) / voxelSize)) + 1);
    }

    for (int z = lo[2]; z <= hi[2]; ++z) {
      for (int y = lo[1]; y <= hi[1]; ++y) {
        for (int x = lo[0]; x <= hi[0]; ++x) {
          const Vec3 center = {origin[0] + (x + 0.5) * voxelSize, origin[1] + (y + 0.5) * voxelSize, origin[2] + (z + 0.5) * voxelSize};
          if (triangleBoxOverlap(center, half, a, b, c)) grid.set(x, y, z);
        }
      }
    }
  }
  return grid;
}

std::size_t VoxelGrid::connectedComponents() const
{
  if (resolution <= 0 || data.empty()) return 0;
  std::vector<std::uint8_t> visited(data.size(), 0);
  std::queue<std::size_t> pending;
  std::size_t count = 0;
  const std::size_t plane = static_cast<std::size_t>(resolution) * resolution;

  for (std::size_t start = 0; start < data.size(); ++start) {
    if (!data[start] || visited[start]) continue;
    ++count;
    visited[start] = 1;
    pending.push(start);
    while (!pending.empty()) {
      const std::size_t index = pending.front();
      pending.pop();
      const int x = static_cast<int>(index % resolution);
      const int y = static_cast<int>((index / resolution) % resolution);
      const int z = static_cast<int>(index / plane);
      constexpr int directions[6][3] = {{-1,0,0},{1,0,0},{0,-1,0},{0,1,0},{0,0,-1},{0,0,1}};
      for (const auto& direction : directions) {
        const int nx = x + direction[0], ny = y + direction[1], nz = z + direction[2];
        if (nx < 0 || ny < 0 || nz < 0 || nx >= resolution || ny >= resolution || nz >= resolution) continue;
        const std::size_t next = static_cast<std::size_t>(nx) + static_cast<std::size_t>(ny) * resolution + static_cast<std::size_t>(nz) * plane;
        if (data[next] && !visited[next]) {
          visited[next] = 1;
          pending.push(next);
        }
      }
    }
  }
  return count;
}

VoxelGrid voxelizeMesh(const std::string& path, int resolution, VoxelizationMode mode, std::string* backend)
{
  std::vector<Vec3> positions;
  std::vector<Tri> triangles;
  std::vector<FloatColor> colors;
  bool hasColors = false;
  readMesh(path, positions, triangles, colors, hasColors);
  if (mode == VoxelizationMode::Triangle) {
#if defined(VOXKIT_ENABLE_METAL)
    if (backend) *backend = "metal-gpu";
    VoxelGrid grid = voxelizeTrianglesMetal(positions, triangles, resolution);
    grid.mode = mode;
    if (hasColors) projectVertexColors(grid, positions, triangles, colors);
    return grid;
#else
    throw std::runtime_error("triangle mode requires a Metal GPU build (ENABLE_GPU=ON)");
#endif
  }
  if (backend) *backend = mode == VoxelizationMode::Quad ? "cpu-quad" : "cpu-pixel";

  if (mode == VoxelizationMode::Quad) {
    // Quad mode reconstructs coplanar triangle pairs sharing an edge. Processing
    // each patch as a unit preserves the original four-sided face boundary while
    // retaining unpaired triangles for STL and triangulated glTF inputs.
    std::vector<std::uint8_t> paired(triangles.size(), 0);
    std::vector<Tri> quadTriangles;
    quadTriangles.reserve(triangles.size());
    for (std::size_t i = 0; i < triangles.size(); ++i) {
      if (paired[i]) continue;
      std::size_t partner = triangles.size();
      const Tri& a = triangles[i];
      const Vec3 na = cross(sub(positions[a[1]], positions[a[0]]), sub(positions[a[2]], positions[a[0]]));
      for (std::size_t j = i + 1; j < triangles.size(); ++j) {
        if (paired[j]) continue;
        int shared = 0;
        for (auto av : a) for (auto bv : triangles[j]) if (av == bv) ++shared;
        if (shared != 2) continue;
        const Tri& b = triangles[j];
        const Vec3 nb = cross(sub(positions[b[1]], positions[b[0]]), sub(positions[b[2]], positions[b[0]]));
        const double denom = std::sqrt(lengthSquared(na) * lengthSquared(nb));
        if (denom > 1e-18 && std::abs(dot(na, nb)) / denom > 0.9999) { partner = j; break; }
      }
      quadTriangles.push_back(a);
      paired[i] = 1;
      if (partner != triangles.size()) {
        quadTriangles.push_back(triangles[partner]);
        paired[partner] = 1;
      }
    }
    VoxelGrid grid = Voxelizer().voxelize(positions, quadTriangles, resolution);
    grid.mode = mode;
    if (hasColors) projectVertexColors(grid, positions, quadTriangles, colors);
    return grid;
  }
  VoxelGrid grid = Voxelizer().voxelize(positions, triangles, resolution);
  grid.mode = mode;
  if (hasColors) projectVertexColors(grid, positions, triangles, colors);
  return grid;
}

void writeBinvox(const VoxelGrid& grid, const std::string& path)
{
  std::ofstream out(path, std::ios::binary);
  if (!out) throw std::runtime_error("failed to open output: " + path);
  out << "#binvox 1\n";
  out << "dim " << grid.resolution << ' ' << grid.resolution << ' ' << grid.resolution << "\n";
  out << "translate " << grid.origin[0] << ' ' << grid.origin[1] << ' ' << grid.origin[2] << "\n";
  out << "scale " << (grid.voxelSize * grid.resolution) << "\n";
  out << "voxel_mode " << voxelizationModeName(grid.mode) << "\n";
  out << "data\n";

  if (grid.data.empty()) return;
  std::uint8_t current = grid.data[0] ? 1 : 0;
  std::uint8_t count = 0;
  auto flush = [&]() {
    out.put(static_cast<char>(current));
    out.put(static_cast<char>(count));
  };
  for (std::uint8_t value : grid.data) {
    value = value ? 1 : 0;
    if (value == current && count < 255) {
      ++count;
    } else {
      flush();
      current = value;
      count = 1;
    }
  }
  flush();
  out.close();
  if (!out) throw std::runtime_error("failed while writing binvox output: " + path);
  writeVoxelColorSidecar(grid, path);
}

void writeVoxelColorSidecar(const VoxelGrid& grid, const std::string& binvoxPath)
{
  const std::filesystem::path target = binvoxPath + ".colors";
  if (!grid.hasColors()) {
    std::error_code ignored;
    std::filesystem::remove(target, ignored);
    return;
  }
  const std::filesystem::path temporary = target.string() + ".tmp";
  std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
  if (!out) throw std::runtime_error("failed to open voxel color sidecar: " + target.string());
  constexpr char magic[8] = {'V','X','C','O','L','R','1','\0'};
  out.write(magic, sizeof(magic));
  const std::uint32_t resolution = static_cast<std::uint32_t>(grid.resolution);
  const std::uint64_t voxelCount = static_cast<std::uint64_t>(grid.data.size());
  const std::uint64_t occupiedCount = static_cast<std::uint64_t>(grid.occupied());
  out.write(reinterpret_cast<const char*>(&resolution), sizeof(resolution));
  out.write(reinterpret_cast<const char*>(&voxelCount), sizeof(voxelCount));
  out.write(reinterpret_cast<const char*>(&occupiedCount), sizeof(occupiedCount));
  for (std::size_t index = 0; index < grid.data.size(); ++index) {
    if (!grid.data[index]) continue;
    out.write(reinterpret_cast<const char*>(&grid.colors[index]), sizeof(ColorRGBA8));
  }
  out.close();
  if (!out) {
    std::error_code ignored;
    std::filesystem::remove(temporary, ignored);
    throw std::runtime_error("failed while writing voxel color sidecar: " + target.string());
  }
  std::error_code error;
  std::filesystem::rename(temporary, target, error);
  if (error) {
    std::filesystem::remove(target, error);
    error.clear();
    std::filesystem::rename(temporary, target, error);
  }
  if (error) throw std::runtime_error("failed to install voxel color sidecar: " + error.message());
}

bool readVoxelColorSidecar(VoxelGrid& grid, const std::string& binvoxPath)
{
  std::ifstream input(binvoxPath + ".colors", std::ios::binary);
  if (!input) return false;
  char magic[8]{};
  std::uint32_t resolution = 0;
  std::uint64_t voxelCount = 0, occupiedCount = 0;
  input.read(magic, sizeof(magic));
  input.read(reinterpret_cast<char*>(&resolution), sizeof(resolution));
  input.read(reinterpret_cast<char*>(&voxelCount), sizeof(voxelCount));
  input.read(reinterpret_cast<char*>(&occupiedCount), sizeof(occupiedCount));
  constexpr char expected[8] = {'V','X','C','O','L','R','1','\0'};
  if (!input || std::memcmp(magic, expected, sizeof(magic)) != 0
      || resolution != static_cast<std::uint32_t>(grid.resolution)
      || voxelCount != grid.data.size() || occupiedCount != grid.occupied()) {
    return false;
  }
  std::vector<ColorRGBA8> colors(grid.data.size());
  for (std::size_t index = 0; index < grid.data.size(); ++index) {
    if (!grid.data[index]) continue;
    input.read(reinterpret_cast<char*>(&colors[index]), sizeof(ColorRGBA8));
    if (!input) return false;
  }
  if (input.peek() != std::char_traits<char>::eof()) return false;
  grid.colors = std::move(colors);
  return true;
}

VoxelGrid readBinvox(const std::string& path)
{
  std::ifstream input(path, std::ios::binary);
  if (!input) throw std::runtime_error("failed to open binvox: " + path);

  std::string line;
  if (!std::getline(input, line) || line != "#binvox 1") throw std::runtime_error("invalid binvox header");

  VoxelGrid grid;
  bool dataSection = false;
  while (std::getline(input, line)) {
    if (line == "data") {
      dataSection = true;
      break;
    }
    std::istringstream header(line);
    std::string key;
    header >> key;
    if (key == "dim") {
      int x = 0, y = 0, z = 0;
      header >> x >> y >> z;
      if (x <= 0 || x != y || x != z || x > 2048) throw std::runtime_error("binvox dimensions must be equal and in 1..2048");
      grid.resolution = x;
      grid.data.assign(static_cast<std::size_t>(x) * y * z, 0);
    } else if (key == "translate") {
      header >> grid.origin[0] >> grid.origin[1] >> grid.origin[2];
    } else if (key == "scale") {
      double scale = 0.0;
      header >> scale;
      if (!(scale > 0.0)) throw std::runtime_error("invalid binvox scale");
      grid.voxelSize = scale / static_cast<double>(grid.resolution);
    } else if (key == "voxel_mode") {
      std::string value;
      header >> value;
      grid.mode = parseVoxelizationMode(value);
    }
  }
  if (!dataSection || grid.data.empty()) throw std::runtime_error("invalid binvox data section");

  std::size_t offset = 0;
  while (offset < grid.data.size()) {
    char value = 0;
    char countByte = 0;
    if (!input.get(value) || !input.get(countByte)) {
      throw std::runtime_error("truncated binvox data");
    }
    const unsigned char count = static_cast<unsigned char>(countByte);
    if (count == 0) throw std::runtime_error("invalid zero-length binvox run");
    if (offset + count > grid.data.size()) throw std::runtime_error("binvox data exceeds declared dimensions");
    std::fill(grid.data.begin() + static_cast<std::ptrdiff_t>(offset),
              grid.data.begin() + static_cast<std::ptrdiff_t>(offset + count),
              value ? 1 : 0);
    offset += count;
  }
  readVoxelColorSidecar(grid, path);
  return grid;
}

namespace {

struct StlTriangle {
  Vec3 normal{};
  Vec3 a{};
  Vec3 b{};
  Vec3 c{};
};

struct SurfaceMesh {
  std::vector<Vec3> vertices;
  std::vector<std::vector<std::uint32_t>> faces;
};

Vec3 normalize(const Vec3& value)
{
  const double length = std::sqrt(lengthSquared(value));
  if (length < 1e-12) return {0.0, 0.0, 0.0};
  return mul(value, 1.0 / length);
}

void addFace(std::vector<StlTriangle>& triangles, const Vec3& origin, double size,
             int x, int y, int z, int axis, int direction)
{
  const Vec3 p = {
    origin[0] + x * size,
    origin[1] + y * size,
    origin[2] + z * size,
  };
  const Vec3 u = axis == 0 ? Vec3{0.0, size, 0.0} : axis == 1 ? Vec3{0.0, 0.0, size} : Vec3{size, 0.0, 0.0};
  const Vec3 v = axis == 0 ? Vec3{0.0, 0.0, size} : axis == 1 ? Vec3{size, 0.0, 0.0} : Vec3{0.0, size, 0.0};
  const Vec3 normal = axis == 0 ? Vec3{static_cast<double>(direction), 0.0, 0.0}
                                : axis == 1 ? Vec3{0.0, static_cast<double>(direction), 0.0}
                                             : Vec3{0.0, 0.0, static_cast<double>(direction)};
  const Vec3 pu = add(p, u);
  const Vec3 pv = add(p, v);
  const Vec3 puv = add(pu, v);
  if (direction > 0) {
    triangles.push_back({normal, p, pu, puv});
    triangles.push_back({normal, p, puv, pv});
  } else {
    triangles.push_back({normal, p, pv, puv});
    triangles.push_back({normal, p, puv, pu});
  }
}

std::vector<StlTriangle> surfaceTriangles(const VoxelGrid& grid)
{
  if (grid.resolution <= 0 || grid.data.size() != static_cast<std::size_t>(grid.resolution) * grid.resolution * grid.resolution)
    throw std::runtime_error("invalid voxel grid");
  if (!(grid.voxelSize > 0.0)) throw std::runtime_error("invalid voxel size");
  std::vector<StlTriangle> triangles;
  triangles.reserve(grid.occupied() * 12);
  for (int z = 0; z < grid.resolution; ++z) for (int y = 0; y < grid.resolution; ++y) for (int x = 0; x < grid.resolution; ++x) {
    if (!grid.get(x, y, z)) continue;
    if (!grid.get(x - 1, y, z)) addFace(triangles, grid.origin, grid.voxelSize, x, y, z, 0, -1);
    if (!grid.get(x + 1, y, z)) addFace(triangles, grid.origin, grid.voxelSize, x + 1, y, z, 0, 1);
    if (!grid.get(x, y - 1, z)) addFace(triangles, grid.origin, grid.voxelSize, x, y, z, 1, -1);
    if (!grid.get(x, y + 1, z)) addFace(triangles, grid.origin, grid.voxelSize, x, y + 1, z, 1, 1);
    if (!grid.get(x, y, z - 1)) addFace(triangles, grid.origin, grid.voxelSize, x, y, z, 2, -1);
    if (!grid.get(x, y, z + 1)) addFace(triangles, grid.origin, grid.voxelSize, x, y, z + 1, 2, 1);
  }
  return triangles;
}

SurfaceMesh blockSurfaceMesh(const VoxelGrid& grid)
{
  SurfaceMesh mesh;
  const auto triangles = surfaceTriangles(grid);
  mesh.vertices.reserve(triangles.size() * 3);
  mesh.faces.reserve(triangles.size());
  for (const auto& triangle : triangles) {
    const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());
    mesh.vertices.insert(mesh.vertices.end(), {triangle.a, triangle.b, triangle.c});
    mesh.faces.push_back({base, base + 1, base + 2});
  }
  return mesh;
}

// Surface Nets over binary voxel samples. Each mixed dual cell receives one
// vertex at the average of its sign-changing edge intersections. Grid edges
// then connect four neighboring dual vertices into a continuous quad surface.
SurfaceMesh surfaceNetMesh(const VoxelGrid& grid, bool triangulate)
{
  if (grid.resolution <= 0 || grid.data.size() != static_cast<std::size_t>(grid.resolution) * grid.resolution * grid.resolution)
    throw std::runtime_error("invalid voxel grid");
  const int n = grid.resolution;
  const int dualN = n + 1;
  const std::size_t dualCount = static_cast<std::size_t>(dualN) * dualN * dualN;
  constexpr std::uint32_t invalid = std::numeric_limits<std::uint32_t>::max();
  std::vector<std::uint32_t> dualVertex(dualCount, invalid);
  SurfaceMesh mesh;
  auto occupied = [&](int x, int y, int z) { return grid.get(x, y, z); };
  auto dualIndex = [dualN](int x, int y, int z) {
    return static_cast<std::size_t>(x) + static_cast<std::size_t>(y) * dualN
      + static_cast<std::size_t>(z) * dualN * dualN;
  };
  constexpr int corners[8][3] = {
    {0,0,0},{1,0,0},{0,1,0},{1,1,0},{0,0,1},{1,0,1},{0,1,1},{1,1,1}
  };
  constexpr int edges[12][2] = {
    {0,1},{2,3},{4,5},{6,7},{0,2},{1,3},{4,6},{5,7},{0,4},{1,5},{2,6},{3,7}
  };

  for (int z = -1; z < n; ++z) for (int y = -1; y < n; ++y) for (int x = -1; x < n; ++x) {
    bool values[8]{};
    int inside = 0;
    for (int c = 0; c < 8; ++c) {
      values[c] = occupied(x + corners[c][0], y + corners[c][1], z + corners[c][2]);
      inside += values[c] ? 1 : 0;
    }
    if (inside == 0 || inside == 8) continue;
    Vec3 average{};
    int crossings = 0;
    for (const auto& edge : edges) {
      if (values[edge[0]] == values[edge[1]]) continue;
      for (int axis = 0; axis < 3; ++axis) {
        const double sample = static_cast<double>(x)
          + 0.5 * (corners[edge[0]][axis] + corners[edge[1]][axis]);
        const double coordinate = axis == 0 ? sample : axis == 1
          ? static_cast<double>(y) + 0.5 * (corners[edge[0]][axis] + corners[edge[1]][axis])
          : static_cast<double>(z) + 0.5 * (corners[edge[0]][axis] + corners[edge[1]][axis]);
        average[axis] += grid.origin[axis] + (coordinate + 0.5) * grid.voxelSize;
      }
      ++crossings;
    }
    if (!crossings) continue;
    average = mul(average, 1.0 / crossings);
    const std::uint32_t id = static_cast<std::uint32_t>(mesh.vertices.size());
    mesh.vertices.push_back(average);
    dualVertex[dualIndex(x + 1, y + 1, z + 1)] = id;
  }

  auto addQuad = [&](std::array<std::uint32_t, 4> q, bool flip) {
    for (auto id : q) if (id == invalid) return;
    if (flip) std::swap(q[1], q[3]);
    if (triangulate) {
      mesh.faces.push_back({q[0], q[1], q[2]});
      mesh.faces.push_back({q[0], q[2], q[3]});
    } else {
      mesh.faces.push_back({q[0], q[1], q[2], q[3]});
    }
  };
  auto dv = [&](int x, int y, int z) -> std::uint32_t {
    if (x < -1 || y < -1 || z < -1 || x >= n || y >= n || z >= n) return invalid;
    return dualVertex[dualIndex(x + 1, y + 1, z + 1)];
  };

  // Every occupancy-changing sample edge owns exactly one surface quad.
  for (int z = 0; z < n; ++z) for (int y = 0; y < n; ++y) for (int x = -1; x < n; ++x) {
    const bool a = occupied(x, y, z), b = occupied(x + 1, y, z);
    if (a != b) addQuad({dv(x,y-1,z-1), dv(x,y,z-1), dv(x,y,z), dv(x,y-1,z)}, a);
  }
  for (int z = 0; z < n; ++z) for (int y = -1; y < n; ++y) for (int x = 0; x < n; ++x) {
    const bool a = occupied(x, y, z), b = occupied(x, y + 1, z);
    if (a != b) addQuad({dv(x-1,y,z-1), dv(x-1,y,z), dv(x,y,z), dv(x,y,z-1)}, !a);
  }
  for (int z = -1; z < n; ++z) for (int y = 0; y < n; ++y) for (int x = 0; x < n; ++x) {
    const bool a = occupied(x, y, z), b = occupied(x, y, z + 1);
    if (a != b) addQuad({dv(x-1,y-1,z), dv(x,y-1,z), dv(x,y,z), dv(x-1,y,z)}, a);
  }
  return mesh;
}

SurfaceMesh makeSurfaceMesh(const VoxelGrid& grid, VoxelizationMode mode)
{
  if (mode == VoxelizationMode::Pixel) return blockSurfaceMesh(grid);
  return surfaceNetMesh(grid, mode == VoxelizationMode::Triangle);
}

std::vector<StlTriangle> triangulateMesh(const SurfaceMesh& mesh)
{
  std::vector<StlTriangle> triangles;
  for (const auto& face : mesh.faces) {
    if (face.size() < 3) continue;
    for (std::size_t i = 1; i + 1 < face.size(); ++i) {
      const Vec3& a = mesh.vertices[face[0]];
      const Vec3& b = mesh.vertices[face[i]];
      const Vec3& c = mesh.vertices[face[i + 1]];
      triangles.push_back({normalize(cross(sub(b, a), sub(c, a))), a, b, c});
    }
  }
  return triangles;
}

} // namespace

void writeStl(const VoxelGrid& grid, const std::string& path, VoxelizationMode mode)
{
  if (grid.resolution <= 0 || grid.data.size() != static_cast<std::size_t>(grid.resolution) * grid.resolution * grid.resolution) {
    throw std::runtime_error("invalid voxel grid");
  }
  if (!(grid.voxelSize > 0.0)) throw std::runtime_error("invalid voxel size");

  std::ofstream out(path, std::ios::binary);
  if (!out) throw std::runtime_error("failed to open STL output: " + path);
  std::array<char, 80> header{};
  const std::string label = "Hands3DLab VoxKit - Snapmaker U1 STL";
  std::copy(label.begin(), label.end(), header.begin());
  out.write(header.data(), static_cast<std::streamsize>(header.size()));

  const std::vector<StlTriangle> triangles = triangulateMesh(makeSurfaceMesh(grid, mode));

  if (triangles.size() > std::numeric_limits<std::uint32_t>::max()) throw std::runtime_error("STL contains too many triangles");
  const std::uint32_t count = static_cast<std::uint32_t>(triangles.size());
  out.write(reinterpret_cast<const char*>(&count), sizeof(count));
  for (const StlTriangle& triangle : triangles) {
    const Vec3 normal = normalize(cross(sub(triangle.b, triangle.a), sub(triangle.c, triangle.a)));
    const auto writeFloat = [&out](double value) {
      const float converted = static_cast<float>(value);
      out.write(reinterpret_cast<const char*>(&converted), sizeof(converted));
    };
    for (double value : normal) writeFloat(value);
    for (const Vec3& point : {triangle.a, triangle.b, triangle.c}) for (double value : point) writeFloat(value);
    const std::uint16_t attribute = 0;
    out.write(reinterpret_cast<const char*>(&attribute), sizeof(attribute));
  }
  if (!out) throw std::runtime_error("failed while writing STL output: " + path);
}

void writeObj(const VoxelGrid& grid, const std::string& path, VoxelizationMode mode)
{
  std::ofstream out(path);
  if (!out) throw std::runtime_error("failed to open OBJ output: " + path);
  const SurfaceMesh mesh = makeSurfaceMesh(grid, mode);
  out << "# Hands3DLab VoxKit " << voxelizationModeName(mode) << " voxel surface\n";
  for (const auto& p : mesh.vertices) out << "v " << p[0] << ' ' << p[1] << ' ' << p[2] << "\n";
  for (const auto& face : mesh.faces) {
    out << "f";
    for (auto index : face) out << ' ' << index + 1;
    out << "\n";
  }
  if (!out) throw std::runtime_error("failed while writing OBJ output");
}

void writeGlb(const VoxelGrid& grid, const std::string& path, VoxelizationMode mode)
{
  const auto triangles = triangulateMesh(makeSurfaceMesh(grid, mode));
  std::vector<float> positions;
  positions.reserve(triangles.size() * 9);
  for (const auto& t : triangles) for (const auto& p : {t.a, t.b, t.c}) for (double v : p) positions.push_back(static_cast<float>(v));
  std::vector<std::uint8_t> bin(positions.size() * sizeof(float));
  std::memcpy(bin.data(), positions.data(), bin.size());
  while (bin.size() % 4) bin.push_back(0);
  const std::size_t jsonLength = 250 + std::to_string(positions.size() / 3).size() * 2;
  std::string json = "{\"asset\":{\"version\":\"2.0\"},\"scene\":0,\"scenes\":[{\"nodes\":[0]}],\"nodes\":[{\"mesh\":0}],\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0},\"mode\":4}]}],\"buffers\":[{\"byteLength\":" + std::to_string(bin.size()) + "}],\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":" + std::to_string(bin.size()) + "}],\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":" + std::to_string(positions.size() / 3) + ",\"type\":\"VEC3\"}]}";
  while (json.size() % 4) json.push_back(' ');
  std::ofstream out(path, std::ios::binary);
  if (!out) throw std::runtime_error("failed to open GLB output: " + path);
  const auto u32 = [&out](std::uint32_t v) { out.write(reinterpret_cast<const char*>(&v), 4); };
  u32(0x46546C67); u32(2); u32(static_cast<std::uint32_t>(12 + 8 + json.size() + 8 + bin.size()));
  u32(static_cast<std::uint32_t>(json.size())); u32(0x4E4F534A); out.write(json.data(), json.size());
  u32(static_cast<std::uint32_t>(bin.size())); u32(0x004E4942); out.write(reinterpret_cast<const char*>(bin.data()), bin.size());
  if (!out) throw std::runtime_error("failed while writing GLB output");
}

namespace {

std::uint32_t crc32(const std::string& bytes)
{
  std::uint32_t crc = 0xffffffffU;
  for (const unsigned char byte : bytes) {
    crc ^= byte;
    for (int bit = 0; bit < 8; ++bit) {
      crc = (crc >> 1U) ^ (0xedb88320U & (0U - (crc & 1U)));
    }
  }
  return ~crc;
}

void writeU16(std::ostream& out, std::uint16_t value)
{
  const char bytes[2] = {static_cast<char>(value), static_cast<char>(value >> 8U)};
  out.write(bytes, 2);
}

void writeU32(std::ostream& out, std::uint32_t value)
{
  const char bytes[4] = {static_cast<char>(value), static_cast<char>(value >> 8U),
                         static_cast<char>(value >> 16U), static_cast<char>(value >> 24U)};
  out.write(bytes, 4);
}

void writeStoredZip(const std::string& path,
                    const std::vector<std::pair<std::string, std::string>>& entries)
{
  struct Record { std::string name; std::uint32_t crc, size, offset; };
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) throw std::runtime_error("failed to open 3MF output: " + path);
  std::vector<Record> records;
  for (const auto& entry : entries) {
    if (entry.first.size() > 65535 || entry.second.size() > 0xffffffffULL) {
      throw std::runtime_error("3MF ZIP entry exceeds classic ZIP limits");
    }
    const auto offset = static_cast<std::uint64_t>(out.tellp());
    if (offset > 0xffffffffULL) throw std::runtime_error("3MF archive exceeds classic ZIP limits");
    const std::uint32_t size = static_cast<std::uint32_t>(entry.second.size());
    const std::uint32_t checksum = crc32(entry.second);
    writeU32(out, 0x04034b50); writeU16(out, 20); writeU16(out, 0); writeU16(out, 0);
    writeU16(out, 0); writeU16(out, 0); writeU32(out, checksum); writeU32(out, size);
    writeU32(out, size); writeU16(out, static_cast<std::uint16_t>(entry.first.size()));
    writeU16(out, 0); out.write(entry.first.data(), entry.first.size());
    out.write(entry.second.data(), entry.second.size());
    records.push_back({entry.first, checksum, size, static_cast<std::uint32_t>(offset)});
  }
  const auto centralOffset64 = static_cast<std::uint64_t>(out.tellp());
  if (centralOffset64 > 0xffffffffULL || records.size() > 65535) {
    throw std::runtime_error("3MF archive exceeds classic ZIP limits");
  }
  for (const Record& record : records) {
    writeU32(out, 0x02014b50); writeU16(out, 20); writeU16(out, 20); writeU16(out, 0);
    writeU16(out, 0); writeU16(out, 0); writeU16(out, 0); writeU32(out, record.crc);
    writeU32(out, record.size); writeU32(out, record.size);
    writeU16(out, static_cast<std::uint16_t>(record.name.size())); writeU16(out, 0);
    writeU16(out, 0); writeU16(out, 0); writeU16(out, 0); writeU32(out, 0);
    writeU32(out, record.offset); out.write(record.name.data(), record.name.size());
  }
  const std::uint32_t centralOffset = static_cast<std::uint32_t>(centralOffset64);
  const auto centralEnd = static_cast<std::uint64_t>(out.tellp());
  if (centralEnd - centralOffset64 > 0xffffffffULL) {
    throw std::runtime_error("3MF central directory exceeds classic ZIP limits");
  }
  writeU32(out, 0x06054b50); writeU16(out, 0); writeU16(out, 0);
  writeU16(out, static_cast<std::uint16_t>(records.size()));
  writeU16(out, static_cast<std::uint16_t>(records.size()));
  writeU32(out, static_cast<std::uint32_t>(centralEnd - centralOffset64));
  writeU32(out, centralOffset); writeU16(out, 0);
  if (!out) throw std::runtime_error("failed while writing 3MF output: " + path);
}

ColorRGBA8 colorAt(const VoxelGrid& grid, const Vec3& point)
{
  if (!grid.hasColors()) return {210, 210, 210, 255};
  int coordinate[3];
  for (int axis = 0; axis < 3; ++axis) {
    coordinate[axis] = std::clamp(static_cast<int>(std::floor(
      (point[axis] - grid.origin[axis]) / grid.voxelSize)), 0, grid.resolution - 1);
  }
  const std::size_t plane = static_cast<std::size_t>(grid.resolution) * grid.resolution;
  const std::size_t index = static_cast<std::size_t>(coordinate[0])
    + static_cast<std::size_t>(coordinate[1]) * grid.resolution
    + static_cast<std::size_t>(coordinate[2]) * plane;
  if (grid.data[index]) return grid.colors[index];
  for (int radius = 1; radius <= 2; ++radius) {
    for (int z = -radius; z <= radius; ++z) for (int y = -radius; y <= radius; ++y) for (int x = -radius; x <= radius; ++x) {
      const int nx = coordinate[0] + x, ny = coordinate[1] + y, nz = coordinate[2] + z;
      if (!grid.get(nx, ny, nz)) continue;
      const std::size_t nearby = static_cast<std::size_t>(nx) + static_cast<std::size_t>(ny) * grid.resolution + static_cast<std::size_t>(nz) * plane;
      return grid.colors[nearby];
    }
  }
  return {210, 210, 210, 255};
}

std::uint32_t packedColor(const ColorRGBA8& color)
{
  return static_cast<std::uint32_t>(color.r) << 24U
    | static_cast<std::uint32_t>(color.g) << 16U
    | static_cast<std::uint32_t>(color.b) << 8U | color.a;
}

} // namespace

void write3mf(const VoxelGrid& grid, const std::string& path,
              VoxelizationMode mode, std::size_t maxMaterials)
{
  if (maxMaterials == 0 || maxMaterials > 256) {
    throw std::runtime_error("3MF material count must be in 1..256");
  }
  const SurfaceMesh mesh = makeSurfaceMesh(grid, mode);
  const std::vector<StlTriangle> triangles = triangulateMesh(mesh);
  if (triangles.empty()) throw std::runtime_error("cannot export an empty 3MF mesh");

  std::vector<ColorRGBA8> triangleColors;
  std::unordered_map<std::uint32_t, std::size_t> frequencies;
  triangleColors.reserve(triangles.size());
  for (const auto& triangle : triangles) {
    const ColorRGBA8 color = colorAt(grid, mul(add(add(triangle.a, triangle.b), triangle.c), 1.0 / 3.0));
    triangleColors.push_back(color);
    ++frequencies[packedColor(color)];
  }
  std::vector<std::pair<std::uint32_t, std::size_t>> ranked(frequencies.begin(), frequencies.end());
  std::sort(ranked.begin(), ranked.end(), [](const auto& left, const auto& right) {
    return left.second != right.second ? left.second > right.second : left.first < right.first;
  });
  ranked.resize(std::min(maxMaterials, ranked.size()));
  std::vector<ColorRGBA8> palette;
  for (const auto& item : ranked) {
    palette.push_back({static_cast<std::uint8_t>(item.first >> 24U),
                       static_cast<std::uint8_t>(item.first >> 16U),
                       static_cast<std::uint8_t>(item.first >> 8U),
                       static_cast<std::uint8_t>(item.first)});
  }
  auto paletteIndex = [&palette](const ColorRGBA8& color) {
    std::size_t bestIndex = 0;
    std::uint64_t bestDistance = std::numeric_limits<std::uint64_t>::max();
    for (std::size_t index = 0; index < palette.size(); ++index) {
      const long dr = static_cast<long>(color.r) - palette[index].r;
      const long dg = static_cast<long>(color.g) - palette[index].g;
      const long db = static_cast<long>(color.b) - palette[index].b;
      const std::uint64_t distance = dr * dr + dg * dg + db * db;
      if (distance < bestDistance) { bestDistance = distance; bestIndex = index; }
    }
    return bestIndex;
  };

  std::ostringstream model;
  model.imbue(std::locale::classic());
  model << std::setprecision(9)
        << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        << "<model unit=\"millimeter\" xml:lang=\"en-US\" xmlns=\"http://schemas.microsoft.com/3dmanufacturing/core/2015/02\">\n"
        << " <metadata name=\"Title\">Hands3DLab VoxKit</metadata>\n <resources>\n"
        << "  <basematerials id=\"2\">\n";
  for (std::size_t index = 0; index < palette.size(); ++index) {
    const auto& color = palette[index];
    model << "   <base name=\"VoxKit Color " << index + 1 << "\" displaycolor=\"#"
          << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(color.r)
          << std::setw(2) << static_cast<int>(color.g) << std::setw(2) << static_cast<int>(color.b)
          << std::setw(2) << static_cast<int>(color.a) << std::dec << std::nouppercase << "\"/>\n";
  }
  model << "  </basematerials>\n  <object id=\"1\" type=\"model\"><mesh><vertices>\n";
  for (const auto& triangle : triangles) for (const auto& point : {triangle.a, triangle.b, triangle.c}) {
    model << "   <vertex x=\"" << point[0] << "\" y=\"" << point[1] << "\" z=\"" << point[2] << "\"/>\n";
  }
  model << "  </vertices><triangles>\n";
  for (std::size_t index = 0; index < triangles.size(); ++index) {
    const std::size_t vertex = index * 3;
    model << "   <triangle v1=\"" << vertex << "\" v2=\"" << vertex + 1
          << "\" v3=\"" << vertex + 2 << "\" pid=\"2\" p1=\""
          << paletteIndex(triangleColors[index]) << "\"/>\n";
  }
  model << "  </triangles></mesh></object>\n </resources>\n <build><item objectid=\"1\"/></build>\n</model>\n";

  const std::string contentTypes =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
    "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
    "<Default Extension=\"model\" ContentType=\"application/vnd.ms-package.3dmanufacturing-3dmodel+xml\"/>"
    "</Types>";
  const std::string relationships =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
    "<Relationship Target=\"/3D/3dmodel.model\" Id=\"rel0\" "
    "Type=\"http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel\"/>"
    "</Relationships>";
  writeStoredZip(path, {{"[Content_Types].xml", contentTypes},
                        {"_rels/.rels", relationships},
                        {"3D/3dmodel.model", model.str()}});
}
