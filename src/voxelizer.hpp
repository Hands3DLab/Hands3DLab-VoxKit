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

  bool get(int x, int y, int z) const;
  void set(int x, int y, int z);
  std::size_t occupied() const;
  std::size_t connectedComponents() const;
  bool hasColors() const { return !colors.empty() && colors.size() == data.size(); }
};

class Voxelizer {
public:
  Voxelizer() = default;

  VoxelGrid voxelize(const std::vector<std::array<double, 3>>& positions,
                     const std::vector<std::array<std::uint32_t, 3>>& triangles,
                     int gridResolution = 128);
};

VoxelGrid voxelizeMesh(const std::string& path, int resolution = 128,
                       VoxelizationMode mode = VoxelizationMode::Pixel,
                       std::string* backend = nullptr);
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
void writeStl(const VoxelGrid& grid, const std::string& path,
              VoxelizationMode mode = VoxelizationMode::Pixel);
void writeObj(const VoxelGrid& grid, const std::string& path,
              VoxelizationMode mode = VoxelizationMode::Pixel);
void writeGlb(const VoxelGrid& grid, const std::string& path,
              VoxelizationMode mode = VoxelizationMode::Pixel);
void write3mf(const VoxelGrid& grid, const std::string& path,
              VoxelizationMode mode = VoxelizationMode::Pixel,
              std::size_t maxMaterials = 8);