#include "voxelizer.hpp"

#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void usage()
{
  std::cerr << "Usage: voxkit <input.glb|input.obj|input.stl> [-r resolution]\n"
            << "              [--voxel-mode pixel|triangle|quad] [--minimum-feature-width-mm width] [--split-parts] [-o output.binvox]\n"
            << "       voxkit --export-<obj|glb|stl|3mf> <input.binvox> [-o output.<format>] [--minimum-feature-width-mm width] [--scale factor]\n"
            << "       voxkit --convert-<obj|glb|stl|3mf> <input.mesh> -o output.<format>\n";
}

std::string defaultOutput(const std::string& input)
{
  std::filesystem::path p(input);
  p.replace_extension(".binvox");
  return p.string();
}

void printCapabilities()
{
#if defined(VOXKIT_ENABLE_METAL)
  std::cout << "VOXKIT_CAPABILITIES triangle=true backend=metal-gpu" << std::endl;
#elif defined(VOXKIT_ENABLE_D3D11)
  std::cout << "VOXKIT_CAPABILITIES triangle=true backend=d3d11-gpu" << std::endl;
#else
  std::cout << "VOXKIT_CAPABILITIES triangle=false backend=cpu" << std::endl;
#endif
}

void progress(int percent, const std::string& stage)
{
  std::cout << "H3DL_PROGRESS " << percent << ' ' << stage << std::endl;
}

} // namespace

int main(int argc, char** argv)
{
  try {
    if (argc < 2) {
      usage();
      return 2;
    }

    std::string input;
    std::string output;
    int resolution = 128;
    bool splitParts = false;
    VoxelizationMode voxelMode = VoxelizationMode::Pixel;
    double minimumFeatureWidth = 0.0;
    double modelScale = 1.0;
    std::string exportFormat;
    std::string convertFormat;

    for (int i = 1; i < argc; ++i) {
      std::string arg = argv[i];
      if (arg == "-h" || arg == "--help") {
        usage();
        return 0;
      } else if (arg == "--capabilities") {
        printCapabilities();
        return 0;
      } else if ((arg == "-r" || arg == "--resolution") && i + 1 < argc) {
        resolution = std::stoi(argv[++i]);
      } else if (arg == "--split-parts") {
        splitParts = true;
      } else if (arg == "--minimum-feature-width-mm" && i + 1 < argc) {
        minimumFeatureWidth = std::stod(argv[++i]);
        if (!std::isfinite(minimumFeatureWidth) || minimumFeatureWidth <= 0.0) {
          throw std::runtime_error("minimum feature width must be greater than zero");
        }
      } else if (arg == "--scale" && i + 1 < argc) {
        modelScale = std::stod(argv[++i]);
        if (!std::isfinite(modelScale) || modelScale <= 0.0) {
          throw std::runtime_error("scale must be greater than zero");
        }
      } else if (arg == "--voxel-mode" && i + 1 < argc) {
        voxelMode = parseVoxelizationMode(argv[++i]);
      } else if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
        output = argv[++i];
      } else if (arg == "--export-stl" || arg == "--export-obj" || arg == "--export-glb" || arg == "--export-3mf") {
        exportFormat = arg.substr(9);
      } else if (arg == "--convert-stl" || arg == "--convert-obj" || arg == "--convert-glb" || arg == "--convert-3mf") {
        convertFormat = arg.substr(10);
      } else if (input.empty()) {
        input = arg;
      } else {
        throw std::runtime_error("unexpected argument: " + arg);
      }
    }

    if (input.empty()) {
      usage();
      return 2;
    }
    if (!convertFormat.empty()) {
      if (output.empty()) {
        std::filesystem::path defaultPath(input);
        defaultPath.replace_extension("." + convertFormat);
        output = defaultPath.string();
      }
      progress(5, "loading-mesh");
      const SourceMesh mesh = readSourceMesh(input);
      progress(85, "writing-" + convertFormat);
      if (convertFormat == "stl") writeSourceStl(mesh, output);
      else if (convertFormat == "obj") writeSourceObj(mesh, output);
      else if (convertFormat == "glb") writeSourceGlb(mesh, output);
      else writeSource3mf(mesh, output);
      progress(100, "done");
      std::cout << "VOXKIT_RESULT input=\"" << input << "\" output=\"" << output << "\" triangles=" << mesh.triangles.size() << std::endl;
      return 0;
    }
    if (!exportFormat.empty()) {
      if (output.empty()) {
        std::filesystem::path defaultPath(input);
        defaultPath.replace_extension("." + exportFormat);
        output = defaultPath.string();
      }
      progress(5, "loading-binvox");
      VoxelGrid grid = readBinvox(input);
      std::size_t expandedVoxels = 0;
      if (minimumFeatureWidth > 0.0) expandedVoxels = thickenThinFeatures(grid, minimumFeatureWidth);
      if (modelScale != 1.0) {
        grid.voxelSize *= modelScale;
        for (double& coordinate : grid.origin) coordinate *= modelScale;
      }
      progress(85, "writing-" + exportFormat);
      if (exportFormat == "stl") writeStl(grid, output, grid.mode);
      else if (exportFormat == "obj") writeObj(grid, output, grid.mode);
      else if (exportFormat == "glb") writeGlb(grid, output, grid.mode);
      else write3mf(grid, output, grid.mode);
      progress(100, "done");
      std::cout << "VOXKIT_RESULT input=\"" << input << "\" output=\"" << output
                << "\" resolution=" << grid.resolution << " occupied=" << grid.occupied()
                << " voxelSize=" << grid.voxelSize << " voxelMode=" << voxelizationModeName(grid.mode)
                << " colors=" << (grid.hasColors() ? "true" : "false")
                << " topology=" << (grid.mode == VoxelizationMode::Pixel ? "blocks" : grid.mode == VoxelizationMode::Triangle ? "triangles" : "quads")
                << " featureWidthMm=" << minimumFeatureWidth << " expandedVoxels=" << expandedVoxels << " scale=" << modelScale
                << std::endl;
      return 0;
    }
    if (output.empty()) output = defaultOutput(input);

    progress(5, "loading-mesh");
    std::string backend;
    VoxelGrid grid = voxelizeMesh(input, resolution, voxelMode, &backend, splitParts);
    std::size_t expandedVoxels = 0;
    if (minimumFeatureWidth > 0.0) expandedVoxels = thickenThinFeatures(grid, minimumFeatureWidth);
    if (modelScale != 1.0) {
      grid.voxelSize *= modelScale;
      for (double& coordinate : grid.origin) coordinate *= modelScale;
    }
    progress(85, "writing-binvox");
    writeBinvox(grid, output);
    progress(100, "done");

    const std::size_t components = splitParts
      ? (grid.hasParts() ? grid.partCount() : grid.connectedComponents())
      : 0;
    std::cout << "VOXKIT_RESULT input=\"" << input << "\" output=\"" << output << "\" resolution=" << resolution
          << " occupied=" << grid.occupied() << " voxelSize=" << grid.voxelSize
          << " voxelMode=" << voxelizationModeName(voxelMode) << " backend=" << backend
          << " colors=" << (grid.hasColors() ? "true" : "false")
          << " topology=" << (voxelMode == VoxelizationMode::Pixel ? "blocks" : voxelMode == VoxelizationMode::Triangle ? "triangles" : "quads")
          << " featureWidthMm=" << minimumFeatureWidth << " expandedVoxels=" << expandedVoxels << " scale=" << modelScale
          << " splitParts=" << (splitParts ? "true" : "false") << " components=" << components << std::endl;
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << "VOXKIT_ERROR " << ex.what() << std::endl;
    return 1;
  }
}
