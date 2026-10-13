#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <array>

enum class VoxelizationMode {
  Pixel,
  Triangle,
  Quad,
};

struct ColorRGBA8 {
  std::uint8_t r = 255;
  std::uint8_t g = 255;
  std::uint8_t b = 255;
  std::uint8_t a = 255;

  bool operator==(const ColorRGBA8& other) const
  {
    return r == other.r && g == other.g && b == other.b && a == other.a;
  }
};

struct PartVoxelMembership {
  std::size_t voxelIndex = 0;
  std::uint32_t partId = 0;
};

VoxelizationMode parseVoxelizationMode(const std::string& value);
const char* voxelizationModeName(VoxelizationMode mode);

struct VoxelGrid {
  int resolution = 128;
  std::array<double, 3> origin{};
  double voxelSize = 1.0;
  VoxelizationMode mode = VoxelizationMode::Pixel;
  std::vector<std::uint8_t> data;
  // RGBA8 in the same linear order as data. An empty vector means that the
  // source had no usable color information; occupancy remains authoritative.
  std::vector<ColorRGBA8> colors;
  // Optional source-part identity for each occupied voxel; zero means unassigned.
  std::vector<std::uint32_t> partIds;
  // Additional part labels for voxels where multiple source shells rasterize
  // into the same cell. Usually empty; keeps coincident details separable.
  std::vector<PartVoxelMembership> partOverlaps;

  bool get(int x, int y, int z) const;
  void set(int x, int y, int z);
  std::size_t occupied() const;
  std::size_t connectedComponents() const;
  bool hasColors() const { return !colors.empty() && colors.size() == data.size(); }
  bool hasParts() const { return !partIds.empty() && partIds.size() == data.size(); }
  std::size_t partCount() const;
};

// Thicken narrow pixel-mode features to at least the requested width in model units.
// Returns the number of newly occupied voxels.
std::size_t thickenThinFeatures(VoxelGrid& grid, double minimumWidth);

struct SourceMesh {
  std::vector<std::array<double, 3>> positions;
  std::vector<std::array<std::uint32_t, 3>> triangles;
};

class Voxelizer {
public:
  Voxelizer() = default;

  VoxelGrid voxelize(const std::vector<std::array<double, 3>>& positions,
                     const std::vector<std::array<std::uint32_t, 3>>& triangles,
                     int gridResolution = 128,
                     const std::vector<std::uint32_t>* triangleParts = nullptr);
};

VoxelGrid voxelizeMesh(const std::string& path, int resolution = 128,
                       VoxelizationMode mode = VoxelizationMode::Pixel,
                       std::string* backend = nullptr,
                       bool splitParts = false);
SourceMesh readSourceMesh(const std::string& path);
void writeSourceStl(const SourceMesh& mesh, const std::string& path);
void writeSourceObj(const SourceMesh& mesh, const std::string& path);
void writeSourceGlb(const SourceMesh& mesh, const std::string& path);
void writeSource3mf(const SourceMesh& mesh, const std::string& path);
#if defined(VOXKIT_ENABLE_METAL)
VoxelGrid voxelizeTrianglesMetal(
  const std::vector<std::array<double, 3>>& positions,
  const std::vector<std::array<std::uint32_t, 3>>& triangles,
  int resolution);
#endif
#if defined(VOXKIT_ENABLE_D3D11)
VoxelGrid voxelizeTrianglesD3D11(
  const std::vector<std::array<double, 3>>& positions,
  const std::vector<std::array<std::uint32_t, 3>>& triangles,
  int resolution);
#endif
void writeBinvox(const VoxelGrid& grid, const std::string& path);
VoxelGrid readBinvox(const std::string& path);
void writeVoxelColorSidecar(const VoxelGrid& grid, const std::string& binvoxPath);
bool readVoxelColorSidecar(VoxelGrid& grid, const std::string& binvoxPath);
void writeVoxelPartSidecar(const VoxelGrid& grid, const std::string& binvoxPath);
bool readVoxelPartSidecar(VoxelGrid& grid, const std::string& binvoxPath);
void writeStl(const VoxelGrid& grid, const std::string& path,
              VoxelizationMode mode = VoxelizationMode::Pixel);
void writeObj(const VoxelGrid& grid, const std::string& path,
              VoxelizationMode mode = VoxelizationMode::Pixel);
void writeGlb(const VoxelGrid& grid, const std::string& path,
              VoxelizationMode mode = VoxelizationMode::Pixel);
void write3mf(const VoxelGrid& grid, const std::string& path,
              VoxelizationMode mode = VoxelizationMode::Pixel,
              std::size_t maxMaterials = 8);
