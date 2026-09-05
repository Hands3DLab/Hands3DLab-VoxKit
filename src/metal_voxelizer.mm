#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include "voxelizer.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace {
struct Float4 { float x, y, z, w; };
struct UInt4 { std::uint32_t x, y, z, w; };
struct Params {
  std::uint32_t resolution;
  std::uint32_t triangleCount;
  float voxelSize;
  std::uint32_t padding;
  Float4 origin;
};

const char* shaderSource = R"METAL(
#include <metal_stdlib>
using namespace metal;
struct Params { uint resolution; uint triangleCount; float voxelSize; uint padding; float4 origin; };
inline uint index3(uint3 p, uint r) { return p.x + p.y*r + p.z*r*r; }
kernel void voxelize_triangles(device const float4* positions [[buffer(0)]],
  device const uint4* tris [[buffer(1)]], device atomic_uint* grid [[buffer(2)]],
  constant Params& p [[buffer(3)]], uint tid [[thread_position_in_grid]]) {
  if (tid >= p.triangleCount) return;
  uint4 tri=tris[tid]; float3 a=positions[tri.x].xyz, b=positions[tri.y].xyz, c=positions[tri.z].xyz;
  float3 mn=min(a,min(b,c)), mx=max(a,max(b,c));
  int3 lo=max(int3(0), int3(floor((mn-p.origin.xyz)/p.voxelSize))-1);
  int3 hi=min(int3(int(p.resolution)-1), int3(floor((mx-p.origin.xyz)/p.voxelSize))+1);
  float3 n=cross(b-a,c-a); float nl=length(n); if(nl<1e-10) return; n/=nl;
  float boxHalf=p.voxelSize*0.5, radius=boxHalf*(abs(n.x)+abs(n.y)+abs(n.z));
  for(int z=lo.z;z<=hi.z;++z) for(int y=lo.y;y<=hi.y;++y) for(int x=lo.x;x<=hi.x;++x) {
    float3 center=p.origin.xyz+(float3(x,y,z)+0.5)*p.voxelSize;
    if(abs(dot(n,center-a))>radius) continue;
    // Conservative triangle AABB plus plane test. It intentionally includes edge cells,
    // producing a watertight surface suitable for the binary voxel grid.
    atomic_store_explicit(&grid[index3(uint3(x,y,z),p.resolution)],1u,memory_order_relaxed);
  }
}
)METAL";
}

VoxelGrid voxelizeTrianglesMetal(
  const std::vector<std::array<double, 3>>& positions,
  const std::vector<std::array<std::uint32_t, 3>>& triangles,
  int resolution)
{
  if (resolution <= 0 || resolution > 2048) throw std::runtime_error("resolution must be in 1..2048");
  @autoreleasepool {
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if (!device) throw std::runtime_error("Metal GPU is unavailable");
    NSError* error = nil;
    id<MTLLibrary> library = [device newLibraryWithSource:[NSString stringWithUTF8String:shaderSource] options:nil error:&error];
    if (!library) throw std::runtime_error(std::string("failed to compile Metal voxelizer: ") + [[error localizedDescription] UTF8String]);
    id<MTLFunction> function = [library newFunctionWithName:@"voxelize_triangles"];
    id<MTLComputePipelineState> pipeline = [device newComputePipelineStateWithFunction:function error:&error];
    if (!pipeline) throw std::runtime_error(std::string("failed to create Metal pipeline: ") + [[error localizedDescription] UTF8String]);

    std::array<double,3> mn={std::numeric_limits<double>::max(),std::numeric_limits<double>::max(),std::numeric_limits<double>::max()};
    std::array<double,3> mx={std::numeric_limits<double>::lowest(),std::numeric_limits<double>::lowest(),std::numeric_limits<double>::lowest()};
    std::vector<Float4> gpuPositions; gpuPositions.reserve(positions.size());
    for (const auto& v:positions) { for(int i=0;i<3;++i){mn[i]=std::min(mn[i],v[i]);mx[i]=std::max(mx[i],v[i]);} gpuPositions.push_back({float(v[0]),float(v[1]),float(v[2]),0}); }
    const double span=std::max({mx[0]-mn[0],mx[1]-mn[1],mx[2]-mn[2],1e-9});
    const double extent=span+2.0*(span*0.005+1e-9), voxelSize=extent/resolution;
    std::array<double,3> origin={(mn[0]+mx[0]-extent)*.5,(mn[1]+mx[1]-extent)*.5,(mn[2]+mx[2]-extent)*.5};
    std::vector<UInt4> gpuTriangles; gpuTriangles.reserve(triangles.size());
    for(const auto&t:triangles) gpuTriangles.push_back({t[0],t[1],t[2],0});
    const std::size_t cells=std::size_t(resolution)*resolution*resolution;
    id<MTLBuffer> pb=[device newBufferWithBytes:gpuPositions.data() length:gpuPositions.size()*sizeof(Float4) options:MTLResourceStorageModeShared];
    id<MTLBuffer> tb=[device newBufferWithBytes:gpuTriangles.data() length:gpuTriangles.size()*sizeof(UInt4) options:MTLResourceStorageModeShared];
    id<MTLBuffer> gb=[device newBufferWithLength:cells*sizeof(std::uint32_t) options:MTLResourceStorageModeShared];
    std::memset([gb contents],0,cells*sizeof(std::uint32_t));
    Params params{std::uint32_t(resolution),std::uint32_t(triangles.size()),float(voxelSize),0,{float(origin[0]),float(origin[1]),float(origin[2]),0}};
    id<MTLBuffer> paramsBuffer=[device newBufferWithBytes:&params length:sizeof(params) options:MTLResourceStorageModeShared];
    id<MTLCommandQueue> queue=[device newCommandQueue]; id<MTLCommandBuffer> command=[queue commandBuffer]; id<MTLComputeCommandEncoder> encoder=[command computeCommandEncoder];
    [encoder setComputePipelineState:pipeline]; [encoder setBuffer:pb offset:0 atIndex:0]; [encoder setBuffer:tb offset:0 atIndex:1]; [encoder setBuffer:gb offset:0 atIndex:2]; [encoder setBuffer:paramsBuffer offset:0 atIndex:3];
    const NSUInteger width=std::min<NSUInteger>(pipeline.maxTotalThreadsPerThreadgroup,256);
    [encoder dispatchThreads:MTLSizeMake(triangles.size(),1,1) threadsPerThreadgroup:MTLSizeMake(width,1,1)]; [encoder endEncoding]; [command commit]; [command waitUntilCompleted];
    if(command.status==MTLCommandBufferStatusError) throw std::runtime_error(std::string("Metal voxelization failed: ")+[[command.error localizedDescription] UTF8String]);
    VoxelGrid grid; grid.resolution=resolution; grid.origin=origin; grid.voxelSize=voxelSize; grid.data.resize(cells);
    const auto* values=static_cast<const std::uint32_t*>([gb contents]); for(std::size_t i=0;i<cells;++i) grid.data[i]=values[i]?1:0;
    return grid;
  }
}
