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
#include <unordered_set>
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

struct QuantizedPoint {
  std::int64_t x, y, z;
  bool operator==(const QuantizedPoint& other) const
  {
    return x == other.x && y == other.y && z == other.z;
  }
};

struct QuantizedPointHash {
  std::size_t operator()(const QuantizedPoint& point) const
  {
    std::size_t value = std::hash<std::int64_t>{}(point.x);
    value ^= std::hash<std::int64_t>{}(point.y) + 0x9e3779b9U + (value << 6U) + (value >> 2U);
    value ^= std::hash<std::int64_t>{}(point.z) + 0x9e3779b9U + (value << 6U) + (value >> 2U);
    return value;
  }
};

std::vector<std::uint32_t> findTriangleParts(const std::vector<Vec3>& positions,
                                              const std::vector<Tri>& triangles)
{
  if (triangles.empty()) return {};
  Vec3 minimum{std::numeric_limits<double>::max(), std::numeric_limits<double>::max(), std::numeric_limits<double>::max()};
  Vec3 maximum{std::numeric_limits<double>::lowest(), std::numeric_limits<double>::lowest(), std::numeric_limits<double>::lowest()};
  for (const Vec3& point : positions) for (int axis = 0; axis < 3; ++axis) {
    minimum[axis] = std::min(minimum[axis], point[axis]);
    maximum[axis] = std::max(maximum[axis], point[axis]);
  }
  const double span = std::max({maximum[0] - minimum[0], maximum[1] - minimum[1], maximum[2] - minimum[2], 1e-9});
  const double tolerance = std::max(span * 1e-7, 1e-9);
  std::unordered_map<QuantizedPoint, std::uint32_t, QuantizedPointHash> weldedVertices;
  std::vector<std::array<std::uint32_t, 3>> triangleVertices;
  triangleVertices.reserve(triangles.size());
  for (const Tri& triangle : triangles) {
    std::array<std::uint32_t, 3> welded{};
    for (int corner = 0; corner < 3; ++corner) {
      if (triangle[corner] >= positions.size()) throw std::runtime_error("triangle vertex index out of range");
      const Vec3& point = positions[triangle[corner]];
      const QuantizedPoint key{
        static_cast<std::int64_t>(std::llround((point[0] - minimum[0]) / tolerance)),
        static_cast<std::int64_t>(std::llround((point[1] - minimum[1]) / tolerance)),
        static_cast<std::int64_t>(std::llround((point[2] - minimum[2]) / tolerance))
      };
      const auto [entry, inserted] = weldedVertices.emplace(key, static_cast<std::uint32_t>(weldedVertices.size()));
      (void)inserted;
      welded[corner] = entry->second;
    }
    triangleVertices.push_back(welded);
  }

  // STL does not carry object/group records.  A number of exporters preserve
  // separate source bodies by writing coincident faces twice, which produces
  // four triangles on a shared edge.  Joining every edge (the old behaviour)
  // therefore collapses those bodies into one component.  First build the
  // manifold shells using only ordinary two-face edges, then attach tiny
  // fragments to the nearest substantial shell.
  std::vector<std::size_t> parent(triangles.size());
  std::vector<std::uint8_t> rank(triangles.size(), 0);
  for (std::size_t index = 0; index < parent.size(); ++index) parent[index] = index;
  const auto rootOf = [&parent](std::size_t item) {
    std::size_t root = item;
    while (parent[root] != root) root = parent[root];
    while (parent[item] != item) {
      const std::size_t next = parent[item];
      parent[item] = root;
      item = next;
    }
    return root;
  };
  const auto join = [&parent, &rank, &rootOf](std::size_t left, std::size_t right) {
    left = rootOf(left);
    right = rootOf(right);
    if (left == right) return;
    if (rank[left] < rank[right]) std::swap(left, right);
    parent[right] = left;
    if (rank[left] == rank[right]) ++rank[left];
  };
  std::unordered_map<std::uint64_t, std::vector<std::size_t>> trianglesByEdge;
  trianglesByEdge.reserve(triangles.size() * 2);
  for (std::size_t triangleIndex = 0; triangleIndex < triangleVertices.size(); ++triangleIndex) {
    const auto& vertices = triangleVertices[triangleIndex];
    for (int edge = 0; edge < 3; ++edge) {
      std::uint32_t a = vertices[edge];
      std::uint32_t b = vertices[(edge + 1) % 3];
      if (a > b) std::swap(a, b);
      const std::uint64_t key = static_cast<std::uint64_t>(a) << 32U | b;
      trianglesByEdge[key].push_back(triangleIndex);
    }
  }

  // Join only manifold edges.  Edges with four incident facets are the common
  // STL representation of two touching source bodies and must remain seams.
  for (const auto& [edge, incident] : trianglesByEdge) {
    (void)edge;
    if (incident.size() == 2) join(incident[0], incident[1]);
  }

  struct Component {
    std::vector<std::size_t> triangles;
    Vec3 centroid{};
    std::size_t firstTriangle = std::numeric_limits<std::size_t>::max();
  };
  std::unordered_map<std::size_t, std::size_t> componentIndex;
  std::vector<Component> components;
  componentIndex.reserve(triangles.size());
  std::vector<std::size_t> triangleComponents(triangles.size());
  for (std::size_t index = 0; index < triangles.size(); ++index) {
    const std::size_t root = rootOf(index);
    auto entry = componentIndex.find(root);
    if (entry == componentIndex.end()) {
      const std::size_t component = components.size();
      componentIndex.emplace(root, component);
      components.emplace_back();
      entry = componentIndex.find(root);
    }
    const std::size_t component = entry->second;
    triangleComponents[index] = component;
    Component& target = components[component];
    target.triangles.push_back(index);
    target.firstTriangle = std::min(target.firstTriangle, index);
    const Tri& triangle = triangles[index];
    for (std::uint32_t vertex : triangle) target.centroid = add(target.centroid, positions[vertex]);
  }
  for (Component& component : components) {
    const double divisor = static_cast<double>(component.triangles.size()) * 3.0;
    component.centroid = mul(component.centroid, 1.0 / divisor);
  }

  // In a large STL, isolated sliver components are usually the loose half of
  // a duplicated seam.  Keep every substantial manifold shell as its own
  // source part, and fold only these slivers back into a neighbouring shell.
  const std::size_t minimumPartTriangles = triangles.size() <= 64
    ? 1
    : std::max<std::size_t>(8, triangles.size() / 5000);
  std::vector<std::size_t> substantial;
  for (std::size_t index = 0; index < components.size(); ++index) {
    if (components[index].triangles.size() >= minimumPartTriangles) substantial.push_back(index);
  }
  if (substantial.empty()) {
    substantial.push_back(static_cast<std::size_t>(std::distance(
      components.begin(), std::max_element(components.begin(), components.end(),
        [](const Component& left, const Component& right) {
          return left.triangles.size() < right.triangles.size();
        }))));
  }

  std::vector<std::size_t> owner(components.size(), std::numeric_limits<std::size_t>::max());
  for (std::size_t component : substantial) owner[component] = component;
  for (std::size_t small = 0; small < components.size(); ++small) {
    if (owner[small] != std::numeric_limits<std::size_t>::max()) continue;
    std::unordered_map<std::size_t, std::size_t> seamTouches;
    for (const auto& [edge, incident] : trianglesByEdge) {
      (void)edge;
      if (incident.size() <= 2) continue;
      bool touchesSmall = false;
      std::unordered_set<std::size_t> neighbours;
      for (std::size_t triangle : incident) {
        const std::size_t component = triangleComponents[triangle];
        if (component == small) touchesSmall = true;
        else if (owner[component] == component) neighbours.insert(component);
      }
      if (touchesSmall) for (std::size_t neighbour : neighbours) ++seamTouches[neighbour];
    }
    std::size_t selected = substantial.front();
    std::size_t selectedTouches = 0;
    double selectedDistance = std::numeric_limits<double>::max();
    for (std::size_t candidate : substantial) {
      const auto touch = seamTouches.find(candidate);
      const std::size_t touchCount = touch == seamTouches.end() ? 0 : touch->second;
      const Vec3 delta = sub(components[small].centroid, components[candidate].centroid);
      const double distance = lengthSquared(delta);
      if (touchCount > selectedTouches
          || (touchCount == selectedTouches && distance < selectedDistance)) {
        selected = candidate;
        selectedTouches = touchCount;
        selectedDistance = distance;
      }
    }
    owner[small] = selected;
  }

  std::vector<std::size_t> ordered = substantial;
  std::sort(ordered.begin(), ordered.end(), [&components](std::size_t left, std::size_t right) {
    return components[left].firstTriangle < components[right].firstTriangle;
  });
  std::unordered_map<std::size_t, std::uint32_t> partIds;
  for (std::size_t index = 0; index < ordered.size(); ++index) {
    partIds[ordered[index]] = static_cast<std::uint32_t>(index + 1);
  }
  std::vector<std::uint32_t> parts(triangles.size());
  for (std::size_t index = 0; index < triangles.size(); ++index) {
    parts[index] = partIds[owner[triangleComponents[index]]];
  }
  return parts;
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

std::size_t VoxelGrid::partCount() const
{
  if (!hasParts()) return 0;
  std::uint32_t maximum = *std::max_element(partIds.begin(), partIds.end());
  for (const PartVoxelMembership& membership : partOverlaps) maximum = std::max(maximum, membership.partId);
  return static_cast<std::size_t>(maximum);
}

std::size_t thickenThinFeatures(VoxelGrid& grid, double minimumWidth)
{
  if (!std::isfinite(minimumWidth) || minimumWidth <= 0.0) {
    throw std::runtime_error("minimum feature width must be greater than zero");
  }
  if (grid.mode != VoxelizationMode::Pixel) {
    throw std::runtime_error("minimum feature compensation is only available in pixel mode");
  }
  if (grid.resolution <= 0 || !(grid.voxelSize > 0.0)
      || grid.data.size() != static_cast<std::size_t>(grid.resolution) * grid.resolution * grid.resolution) {
    throw std::runtime_error("invalid voxel grid for feature compensation");
  }

  const double requestedVoxels = std::ceil(minimumWidth / grid.voxelSize - 1e-9);
  if (!std::isfinite(requestedVoxels) || requestedVoxels > 4095.0) {
    throw std::runtime_error("minimum feature width is too large for the voxel grid");
  }
  const int minimumVoxels = std::max(1, static_cast<int>(requestedVoxels));
  if (minimumVoxels <= 1 || grid.occupied() == 0) return 0;

  const int maximumGrowth = minimumVoxels / 2;
  const int expandedResolution = grid.resolution + maximumGrowth * 2;
  if (expandedResolution > 2048) {
    throw std::runtime_error("feature compensation would exceed the 2048 voxel grid limit");
  }

  const int n = grid.resolution;
  const std::size_t plane = static_cast<std::size_t>(n) * n;
  const std::size_t cellCount = grid.data.size();
  constexpr std::uint16_t infinity = std::numeric_limits<std::uint16_t>::max();
  std::vector<std::uint16_t> distance(cellCount, 0);
  for (std::size_t index = 0; index < cellCount; ++index) {
    if (grid.data[index]) distance[index] = infinity;
  }

  // Two city-block sweeps give a compact distance field suitable for a
  // conservative pixel-width estimate without a large per-voxel queue.
  for (int z = 0; z < n; ++z) {
    for (int y = 0; y < n; ++y) {
      for (int x = 0; x < n; ++x) {
        const std::size_t index = static_cast<std::size_t>(x) + static_cast<std::size_t>(y) * n + static_cast<std::size_t>(z) * plane;
        if (!grid.data[index]) continue;
        std::uint16_t value = distance[index];
        if (x == 0 || y == 0 || z == 0) value = 1;
        const auto relax = [&value, &distance](std::size_t neighbor) {
          if (distance[neighbor] < std::numeric_limits<std::uint16_t>::max() - 1) {
            value = std::min(value, static_cast<std::uint16_t>(distance[neighbor] + 1));
          }
        };
        if (x > 0) relax(index - 1);
        if (y > 0) relax(index - n);
        if (z > 0) relax(index - plane);
        distance[index] = value;
      }
    }
  }
  for (int z = n - 1; z >= 0; --z) {
    for (int y = n - 1; y >= 0; --y) {
      for (int x = n - 1; x >= 0; --x) {
        const std::size_t index = static_cast<std::size_t>(x) + static_cast<std::size_t>(y) * n + static_cast<std::size_t>(z) * plane;
        if (!grid.data[index]) continue;
        std::uint16_t value = distance[index];
        if (x == n - 1 || y == n - 1 || z == n - 1) value = std::min<std::uint16_t>(value, 1);
        const auto relax = [&value, &distance](std::size_t neighbor) {
          if (distance[neighbor] < std::numeric_limits<std::uint16_t>::max() - 1) {
            value = std::min(value, static_cast<std::uint16_t>(distance[neighbor] + 1));
          }
        };
        if (x + 1 < n) relax(index + 1);
        if (y + 1 < n) relax(index + n);
        if (z + 1 < n) relax(index + plane);
        distance[index] = value;
      }
    }
  }

  const bool hasColors = grid.hasColors();
  VoxelGrid expanded;
  expanded.resolution = expandedResolution;
  expanded.origin = {grid.origin[0] - maximumGrowth * grid.voxelSize,
                     grid.origin[1] - maximumGrowth * grid.voxelSize,
                     grid.origin[2] - maximumGrowth * grid.voxelSize};
  expanded.voxelSize = grid.voxelSize;
  expanded.mode = grid.mode;
  const std::size_t expandedCount = static_cast<std::size_t>(expandedResolution) * expandedResolution * expandedResolution;
  expanded.data.assign(expandedCount, 0);
  if (hasColors) expanded.colors.assign(expandedCount, ColorRGBA8{});
  const bool hasParts = grid.hasParts();
  if (hasParts) expanded.partIds.assign(expandedCount, 0);
  const std::size_t expandedPlane = static_cast<std::size_t>(expandedResolution) * expandedResolution;
  const auto expandedIndex = [expandedResolution, expandedPlane](int x, int y, int z) {
    return static_cast<std::size_t>(x) + static_cast<std::size_t>(y) * expandedResolution
      + static_cast<std::size_t>(z) * expandedPlane;
  };
  for (int z = 0; z < n; ++z) {
    for (int y = 0; y < n; ++y) {
      const std::size_t sourceRow = static_cast<std::size_t>(y) * n + static_cast<std::size_t>(z) * plane;
      const std::size_t targetRow = expandedIndex(maximumGrowth, y + maximumGrowth, z + maximumGrowth);
      std::copy_n(grid.data.begin() + static_cast<std::ptrdiff_t>(sourceRow), n,
                  expanded.data.begin() + static_cast<std::ptrdiff_t>(targetRow));
      if (hasColors) {
        std::copy_n(grid.colors.begin() + static_cast<std::ptrdiff_t>(sourceRow), n,
                    expanded.colors.begin() + static_cast<std::ptrdiff_t>(targetRow));
      }
      if (hasParts) {
        std::copy_n(grid.partIds.begin() + static_cast<std::ptrdiff_t>(sourceRow), n,
                    expanded.partIds.begin() + static_cast<std::ptrdiff_t>(targetRow));
      }
    }
  }
  if (hasParts) {
    expanded.partOverlaps.reserve(grid.partOverlaps.size());
    for (const PartVoxelMembership& membership : grid.partOverlaps) {
      const int x = static_cast<int>(membership.voxelIndex % n);
      const int y = static_cast<int>((membership.voxelIndex / n) % n);
      const int z = static_cast<int>(membership.voxelIndex / plane);
      const std::size_t expandedVoxel = expandedIndex(x + maximumGrowth, y + maximumGrowth, z + maximumGrowth);
      expanded.partOverlaps.push_back({expandedVoxel, membership.partId});
    }
  }

  std::size_t added = 0;
  std::vector<std::uint16_t> remaining(expandedCount, 0);
  std::priority_queue<std::pair<int, std::size_t>> pending;
  constexpr int directions[6][3] = {{-1,0,0},{1,0,0},{0,-1,0},{0,1,0},{0,0,-1},{0,0,1}};
  for (int z = 0; z < n; ++z) {
    for (int y = 0; y < n; ++y) {
      for (int x = 0; x < n; ++x) {
        const std::size_t sourceIndex = static_cast<std::size_t>(x) + static_cast<std::size_t>(y) * n + static_cast<std::size_t>(z) * plane;
        if (!grid.data[sourceIndex]) continue;
        bool ridge = true;
        for (const auto& direction : directions) {
          const int nx = x + direction[0], ny = y + direction[1], nz = z + direction[2];
          if (nx < 0 || ny < 0 || nz < 0 || nx >= n || ny >= n || nz >= n) continue;
          const std::size_t neighbor = static_cast<std::size_t>(nx) + static_cast<std::size_t>(ny) * n + static_cast<std::size_t>(nz) * plane;
          if (grid.data[neighbor] && distance[neighbor] > distance[sourceIndex]) {
            ridge = false;
            break;
          }
        }
        if (!ridge) continue;
        const int currentWidth = std::max(1, static_cast<int>(distance[sourceIndex]) * 2 - 1);
        if (currentWidth >= minimumVoxels) continue;
        const int growth = std::min(maximumGrowth, (minimumVoxels - currentWidth + 1) / 2);
        const int centerX = x + maximumGrowth, centerY = y + maximumGrowth, centerZ = z + maximumGrowth;
        if (growth == 0) continue;
        const std::size_t center = expandedIndex(centerX, centerY, centerZ);
        const int steps = growth + 1;
        if (steps > remaining[center]) {
          remaining[center] = static_cast<std::uint16_t>(steps);
          pending.emplace(steps, center);
        }
      }
    }
  }

  while (!pending.empty()) {
    const auto [steps, index] = pending.top();
    pending.pop();
    if (steps <= 1 || remaining[index] != steps) continue;
    const int x = static_cast<int>(index % expandedResolution);
    const int y = static_cast<int>((index / expandedResolution) % expandedResolution);
    const int z = static_cast<int>(index / expandedPlane);
    const int nextSteps = steps - 1;
    for (int dz = -1; dz <= 1; ++dz) {
      for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
          if (dx == 0 && dy == 0 && dz == 0) continue;
          const int nx = x + dx, ny = y + dy, nz = z + dz;
          if (nx < 0 || ny < 0 || nz < 0 || nx >= expandedResolution || ny >= expandedResolution || nz >= expandedResolution) continue;
          const std::size_t target = expandedIndex(nx, ny, nz);
          if (remaining[target] >= nextSteps) continue;
          remaining[target] = static_cast<std::uint16_t>(nextSteps);
          if (!expanded.data[target]) {
            expanded.data[target] = 1;
            if (hasColors) expanded.colors[target] = expanded.colors[index];
            if (hasParts) expanded.partIds[target] = expanded.partIds[index];
            ++added;
          }
          pending.emplace(nextSteps, target);
        }
      }
    }
  }

  if (added) grid = std::move(expanded);
  return added;
}

VoxelGrid Voxelizer::voxelize(const std::vector<Vec3>& positions, const std::vector<Tri>& triangles,
                              int gridResolution, const std::vector<std::uint32_t>* triangleParts)
{
  if (gridResolution <= 0 || gridResolution > 2048) throw std::runtime_error("resolution must be in 1..2048");
  if (positions.empty() || triangles.empty()) throw std::runtime_error("empty mesh");
  if (triangleParts && triangleParts->size() != triangles.size()) throw std::runtime_error("triangle part map size mismatch");

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
  if (triangleParts) grid.partIds.assign(grid.data.size(), 0);
  std::unordered_map<std::size_t, std::vector<std::uint32_t>> overlappingParts;

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
          if (triangleBoxOverlap(center, half, a, b, c)) {
            grid.set(x, y, z);
            if (triangleParts) {
              const std::size_t index = static_cast<std::size_t>(x) + static_cast<std::size_t>(y) * gridResolution
                + static_cast<std::size_t>(z) * gridResolution * gridResolution;
              const std::uint32_t partId = (*triangleParts)[&tri - triangles.data()];
              if (grid.partIds[index] == 0) grid.partIds[index] = partId;
              else if (grid.partIds[index] != partId) {
                std::vector<std::uint32_t>& overlaps = overlappingParts[index];
                if (std::find(overlaps.begin(), overlaps.end(), partId) == overlaps.end()) overlaps.push_back(partId);
              }
            }
          }
        }
      }
    }
  }
  if (triangleParts) {
    for (auto& [voxelIndex, partIds] : overlappingParts) {
      std::sort(partIds.begin(), partIds.end());
      for (const std::uint32_t partId : partIds) grid.partOverlaps.push_back({voxelIndex, partId});
    }
    std::sort(grid.partOverlaps.begin(), grid.partOverlaps.end(), [](const auto& left, const auto& right) {
      return left.voxelIndex != right.voxelIndex ? left.voxelIndex < right.voxelIndex : left.partId < right.partId;
    });
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

VoxelGrid voxelizeMesh(const std::string& path, int resolution, VoxelizationMode mode,
                       std::string* backend, bool splitParts)
{
  std::vector<Vec3> positions;
  std::vector<Tri> triangles;
  std::vector<FloatColor> colors;
  bool hasColors = false;
  readMesh(path, positions, triangles, colors, hasColors);
  const std::vector<std::uint32_t> triangleParts = splitParts ? findTriangleParts(positions, triangles) : std::vector<std::uint32_t>{};
  const auto* partMap = splitParts ? &triangleParts : nullptr;
  if (mode == VoxelizationMode::Triangle) {
    if (splitParts) {
      if (backend) *backend = "cpu-split-parts";
      VoxelGrid grid = Voxelizer().voxelize(positions, triangles, resolution, partMap);
      grid.mode = mode;
      if (hasColors) projectVertexColors(grid, positions, triangles, colors);
      return grid;
    }
#if defined(VOXKIT_ENABLE_METAL)
    if (backend) *backend = "metal-gpu";
    VoxelGrid grid = voxelizeTrianglesMetal(positions, triangles, resolution);
    grid.mode = mode;
    if (hasColors) projectVertexColors(grid, positions, triangles, colors);
    return grid;
  #elif defined(VOXKIT_ENABLE_D3D11)
    if (backend) *backend = "d3d11-gpu";
    VoxelGrid grid = voxelizeTrianglesD3D11(positions, triangles, resolution);
    grid.mode = mode;
    if (hasColors) projectVertexColors(grid, positions, triangles, colors);
    return grid;
#else
    throw std::runtime_error("triangle mode requires a supported GPU build (Metal on macOS or Direct3D 11 on Windows; ENABLE_GPU=ON)");
#endif
  }
  if (backend) *backend = mode == VoxelizationMode::Quad ? "cpu-quad" : "cpu-pixel";

  if (mode == VoxelizationMode::Quad) {
    // Quad mode reconstructs coplanar triangle pairs sharing an edge. Processing
    // each patch as a unit preserves the original four-sided face boundary while
    // retaining unpaired triangles for STL and triangulated glTF inputs.
    std::vector<std::uint8_t> paired(triangles.size(), 0);
    std::vector<Tri> quadTriangles;
    std::vector<std::uint32_t> quadParts;
    quadTriangles.reserve(triangles.size());
    if (splitParts) quadParts.reserve(triangles.size());
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
      if (splitParts) quadParts.push_back(triangleParts[i]);
      paired[i] = 1;
      if (partner != triangles.size()) {
        quadTriangles.push_back(triangles[partner]);
        if (splitParts) quadParts.push_back(triangleParts[partner]);
        paired[partner] = 1;
      }
    }
    VoxelGrid grid = Voxelizer().voxelize(positions, quadTriangles, resolution,
                                         splitParts ? &quadParts : nullptr);
    grid.mode = mode;
    if (hasColors) projectVertexColors(grid, positions, quadTriangles, colors);
    return grid;
  }
  VoxelGrid grid = Voxelizer().voxelize(positions, triangles, resolution, partMap);
  grid.mode = mode;
  if (hasColors) projectVertexColors(grid, positions, triangles, colors);
  return grid;
}

SourceMesh readSourceMesh(const std::string& path)
{
  SourceMesh mesh;
  std::vector<FloatColor> colors;
  bool hasColors = false;
  readMesh(path, mesh.positions, mesh.triangles, colors, hasColors);
  return mesh;
}

void writeSourceObj(const SourceMesh& mesh, const std::string& path)
{
  std::ofstream out(path);
  if (!out) throw std::runtime_error("failed to open OBJ output: " + path);
  out << "# Hands3DLab VoxKit source mesh conversion\n";
  for (const auto& p : mesh.positions) out << "v " << p[0] << ' ' << p[1] << ' ' << p[2] << "\n";
  for (const auto& t : mesh.triangles) out << "f " << t[0] + 1 << ' ' << t[1] + 1 << ' ' << t[2] + 1 << "\n";
  if (!out) throw std::runtime_error("failed while writing OBJ output");
}

void writeSourceStl(const SourceMesh& mesh, const std::string& path)
{
  std::ofstream out(path, std::ios::binary);
  if (!out) throw std::runtime_error("failed to open STL output: " + path);
  std::array<char, 80> header{};
  const std::string label = "Hands3DLab VoxKit source mesh";
  std::copy(label.begin(), label.end(), header.begin());
  out.write(header.data(), 80);
  const auto count = static_cast<std::uint32_t>(mesh.triangles.size());
  out.write(reinterpret_cast<const char*>(&count), 4);
  for (const auto& t : mesh.triangles) {
    const Vec3& a = mesh.positions[t[0]], b = mesh.positions[t[1]], c = mesh.positions[t[2]];
    const Vec3 rawNormal = cross(sub(b, a), sub(c, a));
    const double normalLength = std::sqrt(lengthSquared(rawNormal));
    const Vec3 normal = normalLength > 1e-18
      ? Vec3{rawNormal[0] / normalLength, rawNormal[1] / normalLength, rawNormal[2] / normalLength}
      : Vec3{0.0, 0.0, 0.0};
    const auto writeFloat = [&out](double value) { const float v = static_cast<float>(value); out.write(reinterpret_cast<const char*>(&v), 4); };
    for (double v : normal) writeFloat(v);
    for (const Vec3& p : {a, b, c}) for (double v : p) writeFloat(v);
    const std::uint16_t attribute = 0;
    out.write(reinterpret_cast<const char*>(&attribute), 2);
  }
  if (!out) throw std::runtime_error("failed while writing STL output");
}

void writeSourceGlb(const SourceMesh& mesh, const std::string& path)
{
  std::vector<float> positions;
  positions.reserve(mesh.triangles.size() * 9);
  for (const auto& t : mesh.triangles) for (auto index : t) for (double v : mesh.positions[index]) positions.push_back(static_cast<float>(v));
  std::vector<std::uint8_t> bin(positions.size() * sizeof(float));
  std::memcpy(bin.data(), positions.data(), bin.size());
  while (bin.size() % 4) bin.push_back(0);
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
  writeVoxelPartSidecar(grid, path);
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

void writeVoxelPartSidecar(const VoxelGrid& grid, const std::string& binvoxPath)
{
  const std::filesystem::path target = binvoxPath + ".parts";
  if (!grid.hasParts()) {
    std::error_code ignored;
    std::filesystem::remove(target, ignored);
    return;
  }
  const std::filesystem::path temporary = target.string() + ".tmp";
  std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
  if (!out) throw std::runtime_error("failed to open voxel part sidecar: " + target.string());
  constexpr char magic[8] = {'V','X','P','A','R','T','2','\0'};
  const std::uint32_t resolution = static_cast<std::uint32_t>(grid.resolution);
  const std::uint64_t voxelCount = static_cast<std::uint64_t>(grid.data.size());
  const std::uint64_t occupiedCount = static_cast<std::uint64_t>(grid.occupied());
  const std::size_t partCount = grid.partCount();
  if (partCount > std::numeric_limits<std::uint32_t>::max()) throw std::runtime_error("too many source parts to save");
  const std::uint32_t serializedPartCount = static_cast<std::uint32_t>(partCount);
  const std::uint64_t overlapCount = static_cast<std::uint64_t>(grid.partOverlaps.size());
  out.write(magic, sizeof(magic));
  out.write(reinterpret_cast<const char*>(&resolution), sizeof(resolution));
  out.write(reinterpret_cast<const char*>(&voxelCount), sizeof(voxelCount));
  out.write(reinterpret_cast<const char*>(&occupiedCount), sizeof(occupiedCount));
  out.write(reinterpret_cast<const char*>(&serializedPartCount), sizeof(serializedPartCount));
  out.write(reinterpret_cast<const char*>(&overlapCount), sizeof(overlapCount));
  for (std::size_t index = 0; index < grid.data.size(); ++index) {
    if (!grid.data[index]) continue;
    const std::uint32_t partId = grid.partIds[index];
    if (partId == 0 || partId > serializedPartCount) throw std::runtime_error("invalid source part assignment in voxel grid");
    out.write(reinterpret_cast<const char*>(&partId), sizeof(partId));
  }
  for (const PartVoxelMembership& membership : grid.partOverlaps) {
    if (membership.voxelIndex >= grid.data.size() || !grid.data[membership.voxelIndex]
        || membership.partId == 0 || membership.partId > serializedPartCount
        || membership.partId == grid.partIds[membership.voxelIndex]) {
      throw std::runtime_error("invalid overlapping source part assignment in voxel grid");
    }
    const std::uint64_t index = static_cast<std::uint64_t>(membership.voxelIndex);
    out.write(reinterpret_cast<const char*>(&index), sizeof(index));
    out.write(reinterpret_cast<const char*>(&membership.partId), sizeof(membership.partId));
  }
  out.close();
  if (!out) {
    std::error_code ignored;
    std::filesystem::remove(temporary, ignored);
    throw std::runtime_error("failed while writing voxel part sidecar: " + target.string());
  }
  std::error_code error;
  std::filesystem::rename(temporary, target, error);
  if (error) {
    std::filesystem::remove(target, error);
    error.clear();
    std::filesystem::rename(temporary, target, error);
  }
  if (error) throw std::runtime_error("failed to install voxel part sidecar: " + error.message());
}

bool readVoxelPartSidecar(VoxelGrid& grid, const std::string& binvoxPath)
{
  std::ifstream input(binvoxPath + ".parts", std::ios::binary);
  if (!input) return false;
  char magic[8]{};
  std::uint32_t resolution = 0, partCount = 0;
  std::uint64_t voxelCount = 0, occupiedCount = 0;
  input.read(magic, sizeof(magic));
  input.read(reinterpret_cast<char*>(&resolution), sizeof(resolution));
  input.read(reinterpret_cast<char*>(&voxelCount), sizeof(voxelCount));
  input.read(reinterpret_cast<char*>(&occupiedCount), sizeof(occupiedCount));
  input.read(reinterpret_cast<char*>(&partCount), sizeof(partCount));
  const bool versionOne = std::memcmp(magic, "VXPART1\0", sizeof(magic)) == 0;
  const bool versionTwo = std::memcmp(magic, "VXPART2\0", sizeof(magic)) == 0;
  std::uint64_t overlapCount = 0;
  if (versionTwo) input.read(reinterpret_cast<char*>(&overlapCount), sizeof(overlapCount));
  if (!input || (!versionOne && !versionTwo)
      || resolution != static_cast<std::uint32_t>(grid.resolution)
      || voxelCount != grid.data.size() || occupiedCount != grid.occupied() || partCount == 0
      || (versionOne && overlapCount != 0)) {
    return false;
  }
  const std::uint64_t headerSize = versionTwo ? 40 : 32;
  constexpr std::uint64_t baseRecordSize = sizeof(std::uint32_t);
  constexpr std::uint64_t overlapRecordSize = sizeof(std::uint64_t) + sizeof(std::uint32_t);
  const std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
  if (occupiedCount > (maximum - headerSize) / baseRecordSize) return false;
  const std::uint64_t baseSize = headerSize + occupiedCount * baseRecordSize;
  if (overlapCount > (maximum - baseSize) / overlapRecordSize) return false;
  std::error_code sizeError;
  const std::uintmax_t sidecarSize = std::filesystem::file_size(binvoxPath + ".parts", sizeError);
  if (sizeError || sidecarSize != baseSize + overlapCount * overlapRecordSize) return false;
  std::vector<std::uint32_t> partIds(grid.data.size(), 0);
  for (std::size_t index = 0; index < grid.data.size(); ++index) {
    if (!grid.data[index]) continue;
    input.read(reinterpret_cast<char*>(&partIds[index]), sizeof(std::uint32_t));
    if (!input || partIds[index] == 0 || partIds[index] > partCount) return false;
  }
  std::vector<PartVoxelMembership> partOverlaps;
  partOverlaps.reserve(static_cast<std::size_t>(overlapCount));
  for (std::uint64_t overlap = 0; overlap < overlapCount; ++overlap) {
    std::uint64_t index = 0;
    std::uint32_t partId = 0;
    input.read(reinterpret_cast<char*>(&index), sizeof(index));
    input.read(reinterpret_cast<char*>(&partId), sizeof(partId));
    if (!input || index >= grid.data.size() || !grid.data[static_cast<std::size_t>(index)]
        || partId == 0 || partId > partCount || partId == partIds[static_cast<std::size_t>(index)]) return false;
    partOverlaps.push_back({static_cast<std::size_t>(index), partId});
  }
  if (input.peek() != std::char_traits<char>::eof()) return false;
  grid.partIds = std::move(partIds);
  grid.partOverlaps = std::move(partOverlaps);
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
  readVoxelPartSidecar(grid, path);
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

struct SurfacePart {
  std::uint32_t id = 0;
  std::vector<StlTriangle> triangles;
};

std::vector<std::uint32_t> voxelPartIds(const VoxelGrid& grid)
{
  if (!grid.hasParts()) return {0};
  std::vector<std::uint32_t> ids;
  for (std::size_t index = 0; index < grid.partIds.size(); ++index) {
    if (grid.data[index] && grid.partIds[index]) ids.push_back(grid.partIds[index]);
  }
  for (const PartVoxelMembership& membership : grid.partOverlaps) ids.push_back(membership.partId);
  std::sort(ids.begin(), ids.end());
  ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
  return ids;
}

VoxelGrid extractVoxelPart(const VoxelGrid& grid, std::uint32_t id)
{
  if (!grid.hasParts()) return grid;
  VoxelGrid component;
  component.resolution = grid.resolution;
  component.origin = grid.origin;
  component.voxelSize = grid.voxelSize;
  component.mode = grid.mode;
  component.data.assign(grid.data.size(), 0);
  if (grid.hasColors()) component.colors.assign(grid.data.size(), ColorRGBA8{});
  std::unordered_map<std::size_t, std::vector<std::uint32_t>> overlapIds;
  for (const PartVoxelMembership& membership : grid.partOverlaps) overlapIds[membership.voxelIndex].push_back(membership.partId);
  for (std::size_t index = 0; index < grid.data.size(); ++index) {
    if (!grid.data[index]) continue;
    bool belongsToPart = grid.partIds[index] == id;
    if (!belongsToPart) {
      const auto overlap = overlapIds.find(index);
      belongsToPart = overlap != overlapIds.end()
        && std::find(overlap->second.begin(), overlap->second.end(), id) != overlap->second.end();
    }
    if (!belongsToPart) continue;
    component.data[index] = 1;
    if (grid.hasColors()) component.colors[index] = grid.colors[index];
  }
  return component;
}

std::vector<SurfacePart> surfaceParts(const VoxelGrid& grid, VoxelizationMode mode)
{
  const std::vector<std::uint32_t> ids = voxelPartIds(grid);
  std::vector<SurfacePart> result;
  result.reserve(ids.size());
  for (const std::uint32_t id : ids) {
    const VoxelGrid component = extractVoxelPart(grid, id);
    std::vector<StlTriangle> triangles = triangulateMesh(makeSurfaceMesh(component, mode));
    if (!triangles.empty()) result.push_back({id, std::move(triangles)});
  }
  return result;
}

} // namespace

void writeStl(const VoxelGrid& grid, const std::string& path, VoxelizationMode mode)
{
  if (grid.resolution <= 0 || grid.data.size() != static_cast<std::size_t>(grid.resolution) * grid.resolution * grid.resolution) {
    throw std::runtime_error("invalid voxel grid");
  }
  if (!(grid.voxelSize > 0.0)) throw std::runtime_error("invalid voxel size");

  const std::vector<SurfacePart> parts = surfaceParts(grid, mode);
  std::vector<StlTriangle> triangles;
  for (const SurfacePart& part : parts) triangles.insert(triangles.end(), part.triangles.begin(), part.triangles.end());
  if (triangles.empty()) throw std::runtime_error("cannot export an empty STL mesh");

  // STL has no binary object table.  When source-part labels are available,
  // use the standard ASCII multi-solid form so slicers that understand solid
  // names can keep each source part as an individual object while the output
  // remains a single .stl file.  Unsplit exports retain the compact binary
  // representation used previously.
  if (grid.hasParts()) {
    std::ofstream out(path);
    if (!out) throw std::runtime_error("failed to open STL output: " + path);
    out << std::setprecision(9);
    for (const SurfacePart& part : parts) {
      out << "solid VoxelPart_" << part.id << "\n";
      for (const StlTriangle& triangle : part.triangles) {
        const Vec3 normal = normalize(cross(sub(triangle.b, triangle.a), sub(triangle.c, triangle.a)));
        out << "  facet normal " << normal[0] << ' ' << normal[1] << ' ' << normal[2] << "\n"
            << "    outer loop\n";
        for (const Vec3& point : {triangle.a, triangle.b, triangle.c}) {
          out << "      vertex " << point[0] << ' ' << point[1] << ' ' << point[2] << "\n";
        }
        out << "    endloop\n  endfacet\n";
      }
      out << "endsolid VoxelPart_" << part.id << "\n";
    }
    if (!out) throw std::runtime_error("failed while writing STL output: " + path);
    return;
  }

  std::ofstream out(path, std::ios::binary);
  if (!out) throw std::runtime_error("failed to open STL output: " + path);
  std::array<char, 80> header{};
  const std::string label = "Hands3DLab VoxKit - Snapmaker U1 STL";
  std::copy(label.begin(), label.end(), header.begin());
  out.write(header.data(), static_cast<std::streamsize>(header.size()));

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
  out << "# Hands3DLab VoxKit " << voxelizationModeName(mode) << " voxel surface\n";
  std::size_t vertexOffset = 0;
  for (const std::uint32_t id : voxelPartIds(grid)) {
    const VoxelGrid component = extractVoxelPart(grid, id);
    const SurfaceMesh mesh = makeSurfaceMesh(component, mode);
    if (mesh.faces.empty()) continue;
    out << "o VoxelPart_" << (id ? id : 1) << "\n";
    for (const auto& p : mesh.vertices) out << "v " << p[0] << ' ' << p[1] << ' ' << p[2] << "\n";
    for (const auto& face : mesh.faces) {
      out << "f";
      for (auto index : face) out << ' ' << (vertexOffset + index + 1);
      out << "\n";
    }
    vertexOffset += mesh.vertices.size();
  }
  if (!out) throw std::runtime_error("failed while writing OBJ output");
}

void writeGlb(const VoxelGrid& grid, const std::string& path, VoxelizationMode mode)
{
  const std::vector<SurfacePart> parts = surfaceParts(grid, mode);
  if (parts.empty()) throw std::runtime_error("cannot export an empty GLB mesh");
  std::vector<float> positions;
  std::vector<std::size_t> partOffsets, partLengths, partVertexCounts;
  partOffsets.reserve(parts.size());
  partLengths.reserve(parts.size());
  partVertexCounts.reserve(parts.size());
  for (const SurfacePart& part : parts) {
    partOffsets.push_back(positions.size() * sizeof(float));
    const std::size_t offset = positions.size();
    for (const auto& triangle : part.triangles) {
      for (const auto& point : {triangle.a, triangle.b, triangle.c}) {
        for (double value : point) positions.push_back(static_cast<float>(value));
      }
    }
    partLengths.push_back((positions.size() - offset) * sizeof(float));
    partVertexCounts.push_back((positions.size() - offset) / 3);
  }
  std::vector<std::uint8_t> bin(positions.size() * sizeof(float));
  std::memcpy(bin.data(), positions.data(), bin.size());
  while (bin.size() % 4) bin.push_back(0);
  std::ostringstream jsonStream;
  jsonStream << "{\"asset\":{\"version\":\"2.0\"},\"scene\":0,\"scenes\":[{\"nodes\":[";
  for (std::size_t index = 0; index < parts.size(); ++index) {
    if (index) jsonStream << ',';
    jsonStream << index;
  }
  jsonStream << "]}],\"nodes\":[";
  for (std::size_t index = 0; index < parts.size(); ++index) {
    if (index) jsonStream << ',';
    jsonStream << "{\"mesh\":" << index << ",\"name\":\"VoxelPart_"
               << (parts[index].id ? parts[index].id : 1) << "\"}";
  }
  jsonStream << "],\"meshes\":[";
  for (std::size_t index = 0; index < parts.size(); ++index) {
    if (index) jsonStream << ',';
    jsonStream << "{\"primitives\":[{\"attributes\":{\"POSITION\":" << index << "},\"mode\":4}]}";
  }
  jsonStream << "],\"buffers\":[{\"byteLength\":" << bin.size() << "}],\"bufferViews\":[";
  for (std::size_t index = 0; index < parts.size(); ++index) {
    if (index) jsonStream << ',';
    jsonStream << "{\"buffer\":0,\"byteOffset\":" << partOffsets[index]
               << ",\"byteLength\":" << partLengths[index] << "}";
  }
  jsonStream << "],\"accessors\":[";
  for (std::size_t index = 0; index < parts.size(); ++index) {
    if (index) jsonStream << ',';
    jsonStream << "{\"bufferView\":" << index << ",\"componentType\":5126,\"count\":"
               << partVertexCounts[index] << ",\"type\":\"VEC3\"}";
  }
  jsonStream << "]}";
  std::string json = jsonStream.str();
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
  struct ColoredPart {
    std::uint32_t id;
    VoxelGrid voxels;
    std::vector<StlTriangle> triangles;
    std::vector<ColorRGBA8> colors;
  };
  std::vector<ColoredPart> parts;
  std::unordered_map<std::uint32_t, std::size_t> frequencies;
  for (const SurfacePart& surface : surfaceParts(grid, mode)) {
    ColoredPart part{surface.id, extractVoxelPart(grid, surface.id), surface.triangles, {}};
    part.colors.reserve(part.triangles.size());
    for (const auto& triangle : part.triangles) {
      const ColorRGBA8 color = colorAt(part.voxels, mul(add(add(triangle.a, triangle.b), triangle.c), 1.0 / 3.0));
      part.colors.push_back(color);
      ++frequencies[packedColor(color)];
    }
    parts.push_back(std::move(part));
  }
  if (parts.empty()) throw std::runtime_error("cannot export an empty 3MF mesh");
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
  model << "  </basematerials>\n";
  std::vector<std::uint32_t> objectIds;
  objectIds.reserve(parts.size());
  for (std::size_t partIndex = 0; partIndex < parts.size(); ++partIndex) {
    const std::uint32_t objectId = static_cast<std::uint32_t>(10 + partIndex);
    objectIds.push_back(objectId);
    const ColoredPart& part = parts[partIndex];
    model << "  <object id=\"" << objectId << "\" name=\"VoxelPart_"
          << part.id << "\" type=\"model\"><mesh><vertices>\n";
    for (const auto& triangle : part.triangles) for (const auto& point : {triangle.a, triangle.b, triangle.c}) {
      model << "   <vertex x=\"" << point[0] << "\" y=\"" << point[1] << "\" z=\"" << point[2] << "\"/>\n";
    }
    model << "  </vertices><triangles>\n";
    for (std::size_t index = 0; index < part.triangles.size(); ++index) {
      const std::size_t vertex = index * 3;
      model << "   <triangle v1=\"" << vertex << "\" v2=\"" << vertex + 1
            << "\" v3=\"" << vertex + 2 << "\" pid=\"2\" p1=\""
            << paletteIndex(part.colors[index]) << "\"/>\n";
    }
    model << "  </triangles></mesh></object>\n";
  }
  model << " </resources>\n <build>";
  for (const std::uint32_t objectId : objectIds) model << "<item objectid=\"" << objectId << "\"/>";
  model << "</build>\n</model>\n";

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

void writeSource3mf(const SourceMesh& mesh, const std::string& path)
{
  if (mesh.triangles.empty()) throw std::runtime_error("cannot export an empty 3MF mesh");
  std::ostringstream model;
  model.imbue(std::locale::classic());
  model << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        << "<model unit=\"millimeter\" xml:lang=\"en-US\" xmlns=\"http://schemas.microsoft.com/3dmanufacturing/core/2015/02\">\n"
        << "<resources><object id=\"1\" type=\"model\"><mesh><vertices>\n";
  for (const auto& point : mesh.positions) model << "<vertex x=\"" << point[0] << "\" y=\"" << point[1] << "\" z=\"" << point[2] << "\"/>\n";
  model << "</vertices><triangles>\n";
  for (const auto& triangle : mesh.triangles) model << "<triangle v1=\"" << triangle[0] << "\" v2=\"" << triangle[1] << "\" v3=\"" << triangle[2] << "\"/>\n";
  model << "</triangles></mesh></object></resources><build><item objectid=\"1\"/></build></model>\n";
  const std::string contentTypes = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\"><Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/><Default Extension=\"model\" ContentType=\"application/vnd.ms-package.3dmanufacturing-3dmodel+xml\"/></Types>";
  const std::string relationships = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\"><Relationship Target=\"/3D/3dmodel.model\" Id=\"rel0\" Type=\"http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel\"/></Relationships>";
  writeStoredZip(path, {{"[Content_Types].xml", contentTypes}, {"_rels/.rels", relationships}, {"3D/3dmodel.model", model.str()}});
}
