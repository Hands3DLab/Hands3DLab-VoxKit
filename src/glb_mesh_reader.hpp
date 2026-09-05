#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

struct GlbPrimitive {
  std::vector<std::array<double, 3>> positions;
  std::vector<std::array<float, 4>> colors;
  std::vector<std::array<std::uint32_t, 3>> triangles;
  std::array<float, 4> baseColor{1.0F, 1.0F, 1.0F, 1.0F};
  bool hasColor = false;
};

struct GlbScene {
  std::vector<GlbPrimitive> primitives;
};

GlbScene readGlbScene(const std::string& path);
