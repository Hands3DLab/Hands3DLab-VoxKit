#ifdef _WIN32

#include "voxelizer.hpp"

#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace {

struct Float4 { float x, y, z, w; };
struct UInt4 { std::uint32_t x, y, z, w; };
struct Params {
  std::uint32_t resolution;
  std::uint32_t triangleCount;
  float voxelSize;
  std::uint32_t triangleOffset;
  Float4 origin;
};

constexpr const char* shaderSource = R"HLSL(
struct Params { uint resolution; uint triangleCount; float voxelSize; uint triangleOffset; };
cbuffer Constants : register(b0) { Params p; float4 origin; };
StructuredBuffer<float4> positions : register(t0);
StructuredBuffer<uint4> triangles : register(t1);
RWStructuredBuffer<uint> grid : register(u0);
uint index3(uint3 cell, uint resolution) {
  return cell.x + cell.y * resolution + cell.z * resolution * resolution;
}
[numthreads(64, 1, 1)]
void main(uint3 dispatchId : SV_DispatchThreadID) {
  uint tid = p.triangleOffset + dispatchId.x;
  if (tid >= p.triangleCount) return;
  uint4 triangle = triangles[tid];
  float3 a = positions[triangle.x].xyz;
  float3 b = positions[triangle.y].xyz;
  float3 c = positions[triangle.z].xyz;
  float3 minimum = min(a, min(b, c));
  float3 maximum = max(a, max(b, c));
  int3 lo = max(int3(0, 0, 0), int3(floor((minimum - origin.xyz) / p.voxelSize)) - 1);
  int3 hi = min(int3(p.resolution - 1, p.resolution - 1, p.resolution - 1),
                int3(floor((maximum - origin.xyz) / p.voxelSize)) + 1);
  float3 normal = cross(b - a, c - a);
  float normalLength = length(normal);
  if (normalLength < 1e-10) return;
  normal /= normalLength;
  float halfSize = p.voxelSize * 0.5;
  float radius = halfSize * (abs(normal.x) + abs(normal.y) + abs(normal.z));
  for (int z = lo.z; z <= hi.z; ++z) {
    for (int y = lo.y; y <= hi.y; ++y) {
      for (int x = lo.x; x <= hi.x; ++x) {
        float3 center = origin.xyz + (float3(x, y, z) + 0.5) * p.voxelSize;
        if (abs(dot(normal, center - a)) > radius) continue;
        uint previous;
        InterlockedExchange(grid[index3(uint3(x, y, z), p.resolution)], 1, previous);
      }
    }
  }
}
)HLSL";

void require(HRESULT result, const char* operation)
{
  if (FAILED(result)) {
    char message[96];
    std::snprintf(message, sizeof(message), "%s failed (HRESULT 0x%08X)", operation,
                  static_cast<unsigned int>(result));
    throw std::runtime_error(message);
  }
}

template<typename T>
ComPtr<ID3D11Buffer> createStructuredBuffer(ID3D11Device* device,
                                             const std::vector<T>& values,
                                             UINT bindFlags)
{
  if (values.empty() || values.size() > std::numeric_limits<UINT>::max() / sizeof(T)) {
    throw std::runtime_error("GPU input buffer is empty or too large");
  }
  D3D11_BUFFER_DESC description{};
  description.ByteWidth = static_cast<UINT>(values.size() * sizeof(T));
  description.Usage = D3D11_USAGE_IMMUTABLE;
  description.BindFlags = bindFlags;
  description.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
  description.StructureByteStride = sizeof(T);
  D3D11_SUBRESOURCE_DATA initial{};
  initial.pSysMem = values.data();
  ComPtr<ID3D11Buffer> buffer;
  require(device->CreateBuffer(&description, &initial, &buffer), "creating GPU input buffer");
  return buffer;
}

} // namespace

VoxelGrid voxelizeTrianglesD3D11(
  const std::vector<std::array<double, 3>>& positions,
  const std::vector<std::array<std::uint32_t, 3>>& triangles,
  int resolution)
{
  if (resolution <= 0 || resolution > 2048) throw std::runtime_error("resolution must be in 1..2048");
  if (positions.empty() || triangles.empty()) throw std::runtime_error("empty mesh");
  if (triangles.size() > std::numeric_limits<std::uint32_t>::max()) throw std::runtime_error("too many triangles for Direct3D 11");

  const std::size_t cells = static_cast<std::size_t>(resolution) * resolution * resolution;
  if (cells > std::numeric_limits<UINT>::max() / sizeof(std::uint32_t)) {
    throw std::runtime_error("resolution is too large for a Direct3D 11 voxel buffer");
  }

  UINT deviceFlags = 0;
#ifdef _DEBUG
  deviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
  const D3D_FEATURE_LEVEL requestedLevels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
  D3D_FEATURE_LEVEL selectedLevel{};
  ComPtr<ID3D11Device> device;
  ComPtr<ID3D11DeviceContext> context;
  HRESULT deviceResult = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, deviceFlags,
                                            requestedLevels, 2, D3D11_SDK_VERSION,
                                            &device, &selectedLevel, &context);
  if (deviceResult == E_INVALIDARG) {
    const D3D_FEATURE_LEVEL fallbackLevel[] = {D3D_FEATURE_LEVEL_11_0};
    deviceResult = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, deviceFlags,
                                     fallbackLevel, 1, D3D11_SDK_VERSION,
                                     &device, &selectedLevel, &context);
  }
  require(deviceResult, "creating Direct3D 11 hardware device");

  ComPtr<ID3DBlob> shaderBlob;
  ComPtr<ID3DBlob> errors;
  const HRESULT compileResult = D3DCompile(shaderSource, std::strlen(shaderSource), "voxelize.hlsl",
                                           nullptr, nullptr, "main", "cs_5_0",
                                           D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &shaderBlob, &errors);
  if (FAILED(compileResult)) {
    const std::string detail = errors
      ? std::string(static_cast<const char*>(errors->GetBufferPointer()), errors->GetBufferSize())
      : "unknown HLSL compiler error";
    throw std::runtime_error("failed to compile Direct3D voxelizer: " + detail);
  }
  ComPtr<ID3D11ComputeShader> shader;
  require(device->CreateComputeShader(shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(),
                                      nullptr, &shader), "creating Direct3D compute shader");

  std::array<double, 3> minimum = {std::numeric_limits<double>::max(), std::numeric_limits<double>::max(), std::numeric_limits<double>::max()};
  std::array<double, 3> maximum = {std::numeric_limits<double>::lowest(), std::numeric_limits<double>::lowest(), std::numeric_limits<double>::lowest()};
  std::vector<Float4> gpuPositions;
  gpuPositions.reserve(positions.size());
  for (const auto& position : positions) {
    for (int axis = 0; axis < 3; ++axis) {
      minimum[axis] = std::min(minimum[axis], position[axis]);
      maximum[axis] = std::max(maximum[axis], position[axis]);
    }
    gpuPositions.push_back({static_cast<float>(position[0]), static_cast<float>(position[1]), static_cast<float>(position[2]), 0.0f});
  }
  std::vector<UInt4> gpuTriangles;
  gpuTriangles.reserve(triangles.size());
  for (const auto& triangle : triangles) gpuTriangles.push_back({triangle[0], triangle[1], triangle[2], 0});

  const double span = std::max({maximum[0] - minimum[0], maximum[1] - minimum[1], maximum[2] - minimum[2], 1e-9});
  const double extent = span + 2.0 * (span * 0.005 + 1e-9);
  const double voxelSize = extent / resolution;
  const std::array<double, 3> origin = {
    (minimum[0] + maximum[0] - extent) * 0.5,
    (minimum[1] + maximum[1] - extent) * 0.5,
    (minimum[2] + maximum[2] - extent) * 0.5
  };

  const auto positionBuffer = createStructuredBuffer(device.Get(), gpuPositions, D3D11_BIND_SHADER_RESOURCE);
  const auto triangleBuffer = createStructuredBuffer(device.Get(), gpuTriangles, D3D11_BIND_SHADER_RESOURCE);
  D3D11_SHADER_RESOURCE_VIEW_DESC srvDescription{};
  srvDescription.Format = DXGI_FORMAT_UNKNOWN;
  srvDescription.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
  srvDescription.Buffer.NumElements = static_cast<UINT>(gpuPositions.size());
  ComPtr<ID3D11ShaderResourceView> positionView;
  require(device->CreateShaderResourceView(positionBuffer.Get(), &srvDescription, &positionView), "creating position view");
  srvDescription.Buffer.NumElements = static_cast<UINT>(gpuTriangles.size());
  ComPtr<ID3D11ShaderResourceView> triangleView;
  require(device->CreateShaderResourceView(triangleBuffer.Get(), &srvDescription, &triangleView), "creating triangle view");

  D3D11_BUFFER_DESC gridDescription{};
  gridDescription.ByteWidth = static_cast<UINT>(cells * sizeof(std::uint32_t));
  gridDescription.Usage = D3D11_USAGE_DEFAULT;
  gridDescription.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
  gridDescription.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
  gridDescription.StructureByteStride = sizeof(std::uint32_t);
  ComPtr<ID3D11Buffer> gridBuffer;
  require(device->CreateBuffer(&gridDescription, nullptr, &gridBuffer), "creating voxel grid buffer");
  D3D11_UNORDERED_ACCESS_VIEW_DESC uavDescription{};
  uavDescription.Format = DXGI_FORMAT_UNKNOWN;
  uavDescription.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
  uavDescription.Buffer.NumElements = static_cast<UINT>(cells);
  ComPtr<ID3D11UnorderedAccessView> gridView;
  require(device->CreateUnorderedAccessView(gridBuffer.Get(), &uavDescription, &gridView), "creating voxel grid view");
  const UINT clearValues[4] = {0, 0, 0, 0};
  context->ClearUnorderedAccessViewUint(gridView.Get(), clearValues);

  D3D11_BUFFER_DESC constantsDescription{};
  constantsDescription.ByteWidth = sizeof(Params);
  constantsDescription.Usage = D3D11_USAGE_DEFAULT;
  constantsDescription.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
  ComPtr<ID3D11Buffer> constants;
  require(device->CreateBuffer(&constantsDescription, nullptr, &constants), "creating shader constants");

  context->CSSetShader(shader.Get(), nullptr, 0);
  ID3D11ShaderResourceView* resources[] = {positionView.Get(), triangleView.Get()};
  context->CSSetShaderResources(0, 2, resources);
  ID3D11UnorderedAccessView* outputs[] = {gridView.Get()};
  context->CSSetUnorderedAccessViews(0, 1, outputs, nullptr);
  ID3D11Buffer* constantBuffers[] = {constants.Get()};
  context->CSSetConstantBuffers(0, 1, constantBuffers);

  constexpr std::uint32_t threadsPerGroup = 64;
  constexpr std::uint32_t maxTrianglesPerDispatch = 65535u * threadsPerGroup;
  for (std::uint32_t offset = 0; offset < triangles.size(); offset += maxTrianglesPerDispatch) {
    const std::uint32_t count = std::min<std::uint32_t>(maxTrianglesPerDispatch,
      static_cast<std::uint32_t>(triangles.size() - offset));
    const Params params{static_cast<std::uint32_t>(resolution), static_cast<std::uint32_t>(triangles.size()),
                        static_cast<float>(voxelSize), offset,
                        {static_cast<float>(origin[0]), static_cast<float>(origin[1]), static_cast<float>(origin[2]), 0.0f}};
    context->UpdateSubresource(constants.Get(), 0, nullptr, &params, 0, 0);
    context->Dispatch((count + threadsPerGroup - 1) / threadsPerGroup, 1, 1);
  }

  D3D11_BUFFER_DESC stagingDescription = gridDescription;
  stagingDescription.Usage = D3D11_USAGE_STAGING;
  stagingDescription.BindFlags = 0;
  stagingDescription.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  stagingDescription.MiscFlags = 0;
  stagingDescription.StructureByteStride = 0;
  ComPtr<ID3D11Buffer> staging;
  require(device->CreateBuffer(&stagingDescription, nullptr, &staging), "creating GPU readback buffer");
  context->CopyResource(staging.Get(), gridBuffer.Get());
  D3D11_MAPPED_SUBRESOURCE mapped{};
  require(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped), "reading GPU voxel grid");
  const auto* values = static_cast<const std::uint32_t*>(mapped.pData);
  VoxelGrid grid;
  grid.resolution = resolution;
  grid.origin = origin;
  grid.voxelSize = voxelSize;
  grid.data.resize(cells);
  for (std::size_t index = 0; index < cells; ++index) grid.data[index] = values[index] ? 1 : 0;
  context->Unmap(staging.Get(), 0);
  return grid;
}

#endif
