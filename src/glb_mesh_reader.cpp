#include "glb_mesh_reader.hpp"

#include <boost/json/src.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <unordered_set>

namespace {

namespace json = boost::json;

constexpr std::uint32_t GlbMagic = 0x46546c67;
constexpr std::uint32_t JsonChunk = 0x4e4f534a;
constexpr std::uint32_t BinChunk = 0x004e4942;

struct Matrix4 {
  double values[4][4]{};

  static Matrix4 identity()
  {
    Matrix4 result;
    for (int index = 0; index < 4; ++index) {
      result.values[index][index] = 1.0;
    }
    return result;
  }
};

struct AccessorView {
  const std::vector<std::uint8_t>* binary = nullptr;
  std::size_t offset = 0;
  std::size_t stride = 0;
  std::size_t count = 0;
  std::size_t componentSize = 0;
  std::size_t components = 0;
  std::uint32_t componentType = 0;
  bool normalized = false;
};

std::uint32_t readU32(const std::vector<std::uint8_t>& data, const std::size_t offset)
{
  if (offset + 4 > data.size()) {
    throw std::runtime_error("truncated GLB header");
  }
  return static_cast<std::uint32_t>(data[offset])
    | (static_cast<std::uint32_t>(data[offset + 1]) << 8)
    | (static_cast<std::uint32_t>(data[offset + 2]) << 16)
    | (static_cast<std::uint32_t>(data[offset + 3]) << 24);
}

const json::value& required(const json::object& object, const char* name)
{
  const auto found = object.find(name);
  if (found == object.end()) {
    throw std::runtime_error(std::string("GLB JSON is missing '") + name + "'");
  }
  return found->value();
}

const json::value* optional(const json::object& object, const char* name)
{
  const auto found = object.find(name);
  return found == object.end() ? nullptr : &found->value();
}

double number(const json::value& value)
{
  if (value.is_double()) {
    return value.as_double();
  }
  if (value.is_int64()) {
    return static_cast<double>(value.as_int64());
  }
  if (value.is_uint64()) {
    return static_cast<double>(value.as_uint64());
  }
  throw std::runtime_error("GLB JSON value is not numeric");
}

std::size_t unsignedInteger(const json::value& value, const char* name)
{
  const double numeric = number(value);
  if (!std::isfinite(numeric) || numeric < 0.0 || std::floor(numeric) != numeric
      || numeric > static_cast<double>(std::numeric_limits<std::size_t>::max())) {
    throw std::runtime_error(std::string("invalid GLB integer '") + name + "'");
  }
  return static_cast<std::size_t>(numeric);
}

std::size_t optionalInteger(const json::object& object,
                            const char* name,
                            const std::size_t defaultValue = 0)
{
  const json::value* value = optional(object, name);
  return value == nullptr ? defaultValue : unsignedInteger(*value, name);
}

Matrix4 multiply(const Matrix4& left, const Matrix4& right)
{
  Matrix4 result;
  for (int row = 0; row < 4; ++row) {
    for (int column = 0; column < 4; ++column) {
      for (int index = 0; index < 4; ++index) {
        result.values[row][column] += left.values[row][index] * right.values[index][column];
      }
    }
  }
  return result;
}

std::array<double, 3> transformPoint(const Matrix4& matrix,
                                     const std::array<double, 3>& point)
{
  const double x = matrix.values[0][0] * point[0] + matrix.values[0][1] * point[1]
    + matrix.values[0][2] * point[2] + matrix.values[0][3];
  const double y = matrix.values[1][0] * point[0] + matrix.values[1][1] * point[1]
    + matrix.values[1][2] * point[2] + matrix.values[1][3];
  const double z = matrix.values[2][0] * point[0] + matrix.values[2][1] * point[1]
    + matrix.values[2][2] * point[2] + matrix.values[2][3];
  const double w = matrix.values[3][0] * point[0] + matrix.values[3][1] * point[1]
    + matrix.values[3][2] * point[2] + matrix.values[3][3];
  if (std::abs(w) <= std::numeric_limits<double>::epsilon()) {
    throw std::runtime_error("GLB node transform produced a point at infinity");
  }
  return {x / w, y / w, z / w};
}

double determinant3(const Matrix4& matrix)
{
  const double a = matrix.values[0][0];
  const double b = matrix.values[0][1];
  const double c = matrix.values[0][2];
  const double d = matrix.values[1][0];
  const double e = matrix.values[1][1];
  const double f = matrix.values[1][2];
  const double g = matrix.values[2][0];
  const double h = matrix.values[2][1];
  const double i = matrix.values[2][2];
  return a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
}

std::array<double, 3> vector3(const json::value* value,
                              const std::array<double, 3>& fallback)
{
  if (value == nullptr) {
    return fallback;
  }
  const json::array& array = value->as_array();
  if (array.size() != 3) {
    throw std::runtime_error("GLB node vector must contain three numbers");
  }
  return {number(array[0]), number(array[1]), number(array[2])};
}

Matrix4 nodeMatrix(const json::object& node)
{
  if (const json::value* matrixValue = optional(node, "matrix")) {
    const json::array& array = matrixValue->as_array();
    if (array.size() != 16) {
      throw std::runtime_error("GLB node matrix must contain 16 numbers");
    }
    Matrix4 result;
    for (int column = 0; column < 4; ++column) {
      for (int row = 0; row < 4; ++row) {
        result.values[row][column] = number(array[column * 4 + row]);
      }
    }
    return result;
  }

  const std::array<double, 3> translation =
    vector3(optional(node, "translation"), {0.0, 0.0, 0.0});
  const std::array<double, 3> scale = vector3(optional(node, "scale"), {1.0, 1.0, 1.0});
  std::array<double, 4> rotation{0.0, 0.0, 0.0, 1.0};
  if (const json::value* rotationValue = optional(node, "rotation")) {
    const json::array& array = rotationValue->as_array();
    if (array.size() != 4) {
      throw std::runtime_error("GLB node rotation must contain four numbers");
    }
    for (std::size_t index = 0; index < 4; ++index) {
      rotation[index] = number(array[index]);
    }
  }
  const double quaternionLength = std::sqrt(rotation[0] * rotation[0]
                                             + rotation[1] * rotation[1]
                                             + rotation[2] * rotation[2]
                                             + rotation[3] * rotation[3]);
  if (quaternionLength <= std::numeric_limits<double>::epsilon()) {
    throw std::runtime_error("GLB node quaternion has zero length");
  }
  for (double& value : rotation) {
    value /= quaternionLength;
  }

  const double x = rotation[0];
  const double y = rotation[1];
  const double z = rotation[2];
  const double w = rotation[3];
  Matrix4 result = Matrix4::identity();
  result.values[0][0] = (1.0 - 2.0 * (y * y + z * z)) * scale[0];
  result.values[0][1] = (2.0 * (x * y - z * w)) * scale[1];
  result.values[0][2] = (2.0 * (x * z + y * w)) * scale[2];
  result.values[1][0] = (2.0 * (x * y + z * w)) * scale[0];
  result.values[1][1] = (1.0 - 2.0 * (x * x + z * z)) * scale[1];
  result.values[1][2] = (2.0 * (y * z - x * w)) * scale[2];
  result.values[2][0] = (2.0 * (x * z - y * w)) * scale[0];
  result.values[2][1] = (2.0 * (y * z + x * w)) * scale[1];
  result.values[2][2] = (1.0 - 2.0 * (x * x + y * y)) * scale[2];
  result.values[0][3] = translation[0];
  result.values[1][3] = translation[1];
  result.values[2][3] = translation[2];
  return result;
}

std::size_t componentSize(const std::uint32_t componentType)
{
  switch (componentType) {
    case 5120:
    case 5121:
      return 1;
    case 5122:
    case 5123:
      return 2;
    case 5125:
    case 5126:
      return 4;
    default:
      throw std::runtime_error("unsupported GLB accessor component type");
  }
}

std::size_t componentCount(const std::string& type)
{
  if (type == "SCALAR") return 1;
  if (type == "VEC2") return 2;
  if (type == "VEC3") return 3;
  if (type == "VEC4") return 4;
  if (type == "MAT2") return 4;
  if (type == "MAT3") return 9;
  if (type == "MAT4") return 16;
  throw std::runtime_error("unsupported GLB accessor layout: " + type);
}

AccessorView accessorView(const json::object& root,
                          const std::vector<std::uint8_t>& binary,
                          const std::size_t accessorIndex)
{
  const json::array& accessors = required(root, "accessors").as_array();
  if (accessorIndex >= accessors.size()) {
    throw std::runtime_error("GLB accessor index is out of range");
  }
  const json::object& accessor = accessors[accessorIndex].as_object();
  if (optional(accessor, "sparse") != nullptr) {
    throw std::runtime_error("sparse GLB accessors are not supported");
  }
  const std::size_t viewIndex = unsignedInteger(required(accessor, "bufferView"), "bufferView");
  const json::array& views = required(root, "bufferViews").as_array();
  if (viewIndex >= views.size()) {
    throw std::runtime_error("GLB bufferView index is out of range");
  }
  const json::object& view = views[viewIndex].as_object();
  if (const json::value* extensions = optional(view, "extensions")) {
    if (optional(extensions->as_object(), "EXT_meshopt_compression") != nullptr) {
      throw std::runtime_error("meshopt-compressed GLB buffers are not supported");
    }
  }
  if (optionalInteger(view, "buffer") != 0) {
    throw std::runtime_error("GLB external or secondary buffers are not supported");
  }

  const std::size_t viewOffset = optionalInteger(view, "byteOffset");
  const std::size_t viewLength = unsignedInteger(required(view, "byteLength"), "byteLength");
  const std::size_t accessorOffset = optionalInteger(accessor, "byteOffset");
  const std::uint32_t type = static_cast<std::uint32_t>(
    unsignedInteger(required(accessor, "componentType"), "componentType"));
  const std::size_t size = componentSize(type);
  const std::string layout = required(accessor, "type").as_string().c_str();
  const std::size_t components = componentCount(layout);
  const std::size_t elementSize = size * components;
  const json::value* strideValue = optional(view, "byteStride");
  const std::size_t stride = strideValue == nullptr
    ? elementSize
    : unsignedInteger(*strideValue, "byteStride");
  const std::size_t count = unsignedInteger(required(accessor, "count"), "count");
  if (viewOffset > binary.size() || viewLength > binary.size() - viewOffset) {
    throw std::runtime_error("GLB bufferView exceeds the binary chunk");
  }
  if (strideValue != nullptr && (stride < 4 || stride > 252 || stride % 4 != 0)) {
    throw std::runtime_error("GLB byteStride must be a multiple of 4 in the range 4..252");
  }
  if (stride < elementSize || stride % size != 0 || accessorOffset % size != 0
      || accessorOffset > viewLength) {
    throw std::runtime_error("invalid GLB accessor stride or offset");
  }
  std::size_t requiredBytes = 0;
  if (count != 0) {
    if (count - 1 > (std::numeric_limits<std::size_t>::max() - elementSize) / stride) {
      throw std::runtime_error("GLB accessor byte range overflows size_t");
    }
    requiredBytes = (count - 1) * stride + elementSize;
  }
  if (requiredBytes > viewLength - accessorOffset) {
    throw std::runtime_error("GLB accessor exceeds the binary chunk");
  }
  bool normalized = false;
  if (const json::value* normalizedValue = optional(accessor, "normalized")) {
    normalized = normalizedValue->as_bool();
  }
  return {&binary, viewOffset + accessorOffset, stride, count, size, components, type,
          normalized};
}

float readFloat(const AccessorView& view, const std::size_t item, const std::size_t component)
{
  if (view.componentType != 5126 || component >= view.components || item >= view.count) {
    throw std::runtime_error("GLB POSITION accessor must use float components");
  }
  float result = 0.0F;
  std::memcpy(&result,
              view.binary->data() + view.offset + item * view.stride + component * sizeof(float),
              sizeof(float));
  return result;
}

double readComponent(const AccessorView& view,
                     const std::size_t item,
                     const std::size_t component)
{
  if (component >= view.components || item >= view.count) {
    throw std::runtime_error("GLB accessor component is out of range");
  }
  const std::uint8_t* value = view.binary->data() + view.offset + item * view.stride
    + component * view.componentSize;
  switch (view.componentType) {
    case 5120: {
      std::int8_t result = 0;
      std::memcpy(&result, value, sizeof(result));
      return view.normalized ? std::max(-1.0, static_cast<double>(result) / 127.0) : result;
    }
    case 5121:
      return view.normalized ? static_cast<double>(*value) / 255.0 : *value;
    case 5122: {
      std::int16_t result = 0;
      std::memcpy(&result, value, sizeof(result));
      return view.normalized ? std::max(-1.0, static_cast<double>(result) / 32767.0) : result;
    }
    case 5123: {
      std::uint16_t result = 0;
      std::memcpy(&result, value, sizeof(result));
      return view.normalized ? static_cast<double>(result) / 65535.0 : result;
    }
    case 5125: {
      std::uint32_t result = 0;
      std::memcpy(&result, value, sizeof(result));
      return view.normalized ? static_cast<double>(result) / 4294967295.0 : result;
    }
    case 5126: {
      float result = 0.0F;
      std::memcpy(&result, value, sizeof(result));
      return result;
    }
    default:
      throw std::runtime_error("unsupported GLB accessor component type");
  }
}

std::uint32_t readIndex(const AccessorView& view, const std::size_t item)
{
  if (view.components != 1 || item >= view.count) {
    throw std::runtime_error("GLB index accessor must use scalar components");
  }
  const std::uint8_t* value = view.binary->data() + view.offset + item * view.stride;
  switch (view.componentType) {
    case 5121:
      return *value;
    case 5123: {
      std::uint16_t result = 0;
      std::memcpy(&result, value, sizeof(result));
      return result;
    }
    case 5125: {
      std::uint32_t result = 0;
      std::memcpy(&result, value, sizeof(result));
      return result;
    }
    default:
      throw std::runtime_error("GLB indices must be unsigned byte, short, or int");
  }
}

class SceneParser {
public:
  SceneParser(const json::object& root, const std::vector<std::uint8_t>& binary)
      : root_(root), binary_(binary)
  {
  }

  GlbScene parse()
  {
    const json::array& nodes = required(root_, "nodes").as_array();
    activeNodes_.reserve(nodes.size());
    const json::value* scenesValue = optional(root_, "scenes");
    if (scenesValue != nullptr && !scenesValue->as_array().empty()) {
      const json::array& scenes = scenesValue->as_array();
      const std::size_t sceneIndex = optionalInteger(root_, "scene");
      if (sceneIndex >= scenes.size()) {
        throw std::runtime_error("GLB default scene index is out of range");
      }
      const json::object& scene = scenes[sceneIndex].as_object();
      if (const json::value* roots = optional(scene, "nodes")) {
        for (const json::value& node : roots->as_array()) {
          visit(unsignedInteger(node, "scene node"), Matrix4::identity());
        }
      }
    }
    else {
      std::unordered_set<std::size_t> children;
      for (const json::value& value : nodes) {
        if (const json::value* nodeChildren = optional(value.as_object(), "children")) {
          for (const json::value& child : nodeChildren->as_array()) {
            children.insert(unsignedInteger(child, "child node"));
          }
        }
      }
      for (std::size_t node = 0; node < nodes.size(); ++node) {
        if (children.find(node) == children.end()) {
          visit(node, Matrix4::identity());
        }
      }
    }
    if (scene_.primitives.empty()) {
      throw std::runtime_error("GLB contains no triangle primitives");
    }
    return scene_;
  }

private:
  void visit(const std::size_t nodeIndex, const Matrix4& parent)
  {
    const json::array& nodes = required(root_, "nodes").as_array();
    if (nodeIndex >= nodes.size()) {
      throw std::runtime_error("GLB node index is out of range");
    }
    if (!activeNodes_.insert(nodeIndex).second) {
      throw std::runtime_error("GLB scene graph contains a cycle");
    }
    const json::object& node = nodes[nodeIndex].as_object();
    if (optional(node, "skin") != nullptr) {
      throw std::runtime_error("skinned GLB nodes are not supported");
    }
    const Matrix4 world = multiply(parent, nodeMatrix(node));
    if (const json::value* mesh = optional(node, "mesh")) {
      addMesh(unsignedInteger(*mesh, "mesh"), world);
    }
    if (const json::value* children = optional(node, "children")) {
      for (const json::value& child : children->as_array()) {
        visit(unsignedInteger(child, "child node"), world);
      }
    }
    activeNodes_.erase(nodeIndex);
  }

  void addMesh(const std::size_t meshIndex, const Matrix4& world)
  {
    const json::array& meshes = required(root_, "meshes").as_array();
    if (meshIndex >= meshes.size()) {
      throw std::runtime_error("GLB mesh index is out of range");
    }
    const json::object& mesh = meshes[meshIndex].as_object();
    if (const json::value* weights = optional(mesh, "weights")) {
      if (!weights->as_array().empty()) {
        throw std::runtime_error("morph-weighted GLB meshes are not supported");
      }
    }
    for (const json::value& primitiveValue : required(mesh, "primitives").as_array()) {
      const json::object& primitive = primitiveValue.as_object();
      if (optional(primitive, "targets") != nullptr) {
        throw std::runtime_error("GLB morph targets are not supported");
      }
      if (const json::value* extensions = optional(primitive, "extensions")) {
        if (optional(extensions->as_object(), "KHR_draco_mesh_compression") != nullptr) {
          throw std::runtime_error("Draco-compressed GLB meshes are not supported");
        }
      }
      addPrimitive(primitive, world);
    }
  }

  void addPrimitive(const json::object& primitive, const Matrix4& world)
  {
    const json::object& attributes = required(primitive, "attributes").as_object();
    const std::size_t positionIndex =
      unsignedInteger(required(attributes, "POSITION"), "POSITION");
    const AccessorView positions = accessorView(root_, binary_, positionIndex);
    if (positions.components != 3 || positions.componentType != 5126) {
      throw std::runtime_error("GLB POSITION must be a float VEC3 accessor");
    }

    GlbPrimitive result;
    result.positions.reserve(positions.count);
    for (std::size_t index = 0; index < positions.count; ++index) {
      result.positions.push_back(transformPoint(
        world,
        {readFloat(positions, index, 0),
         readFloat(positions, index, 1),
         readFloat(positions, index, 2)}));
    }

    if (const json::value* materialValue = optional(primitive, "material")) {
      const json::array& materials = required(root_, "materials").as_array();
      const std::size_t materialIndex = unsignedInteger(*materialValue, "material");
      if (materialIndex >= materials.size()) {
        throw std::runtime_error("GLB material index is out of range");
      }
      const json::object& material = materials[materialIndex].as_object();
      if (const json::value* pbrValue = optional(material, "pbrMetallicRoughness")) {
        if (const json::value* factorValue = optional(pbrValue->as_object(), "baseColorFactor")) {
          const json::array& factor = factorValue->as_array();
          if (factor.size() != 4) {
            throw std::runtime_error("GLB baseColorFactor must contain four numbers");
          }
          for (std::size_t component = 0; component < 4; ++component) {
            result.baseColor[component] = static_cast<float>(number(factor[component]));
          }
          result.hasColor = true;
        }
      }
    }

    if (const json::value* colorValue = optional(attributes, "COLOR_0")) {
      const AccessorView color = accessorView(
        root_, binary_, unsignedInteger(*colorValue, "COLOR_0"));
      if ((color.components != 3 && color.components != 4) || color.count != positions.count) {
        throw std::runtime_error("GLB COLOR_0 must be VEC3/VEC4 and match POSITION count");
      }
      if (color.componentType != 5126 && color.componentType != 5121
          && color.componentType != 5123) {
        throw std::runtime_error("GLB COLOR_0 must use float, unsigned byte, or unsigned short");
      }
      if (color.componentType != 5126 && !color.normalized) {
        throw std::runtime_error("integer GLB COLOR_0 accessors must be normalized");
      }
      result.colors.reserve(color.count);
      for (std::size_t index = 0; index < color.count; ++index) {
        std::array<float, 4> rgba{1.0F, 1.0F, 1.0F, 1.0F};
        for (std::size_t component = 0; component < color.components; ++component) {
          rgba[component] = static_cast<float>(readComponent(color, index, component));
        }
        for (std::size_t component = 0; component < 4; ++component) {
          rgba[component] *= result.baseColor[component];
        }
        result.colors.push_back(rgba);
      }
      result.hasColor = true;
    }
    else if (result.hasColor) {
      result.colors.assign(positions.count, result.baseColor);
    }

    std::vector<std::uint32_t> indices;
    if (const json::value* indexValue = optional(primitive, "indices")) {
      const AccessorView indexAccessor =
        accessorView(root_, binary_, unsignedInteger(*indexValue, "indices"));
      indices.reserve(indexAccessor.count);
      for (std::size_t index = 0; index < indexAccessor.count; ++index) {
        indices.push_back(readIndex(indexAccessor, index));
      }
    }
    else {
      indices.reserve(positions.count);
      for (std::size_t index = 0; index < positions.count; ++index) {
        indices.push_back(static_cast<std::uint32_t>(index));
      }
    }
    for (const std::uint32_t index : indices) {
      if (index >= result.positions.size()) {
        throw std::runtime_error("GLB triangle index is outside POSITION accessor");
      }
    }

    const std::size_t mode = optionalInteger(primitive, "mode", 4);
    if (mode == 4) {
      if (indices.size() % 3 != 0) {
        throw std::runtime_error("GLB triangle-list index count is not divisible by three");
      }
      for (std::size_t index = 0; index < indices.size(); index += 3) {
        result.triangles.push_back({indices[index], indices[index + 1], indices[index + 2]});
      }
    }
    else if (mode == 5) {
      for (std::size_t index = 0; index + 2 < indices.size(); ++index) {
        std::array<std::uint32_t, 3> triangle{
          indices[index], indices[index + 1], indices[index + 2]};
        if ((index & 1U) != 0U) {
          std::swap(triangle[0], triangle[1]);
        }
        result.triangles.push_back(triangle);
      }
    }
    else if (mode == 6) {
      for (std::size_t index = 1; index + 1 < indices.size(); ++index) {
        result.triangles.push_back({indices[0], indices[index], indices[index + 1]});
      }
    }
    else {
      throw std::runtime_error("GLB primitive mode is not triangles, strip, or fan");
    }

    result.triangles.erase(
      std::remove_if(result.triangles.begin(), result.triangles.end(),
                     [](const auto& triangle) {
                       return triangle[0] == triangle[1]
                         || triangle[1] == triangle[2]
                         || triangle[2] == triangle[0];
                     }),
      result.triangles.end());
    if (determinant3(world) < 0.0) {
      for (auto& triangle : result.triangles) {
        std::swap(triangle[1], triangle[2]);
      }
    }
    if (!result.triangles.empty()) {
      scene_.primitives.push_back(std::move(result));
    }
  }

private:
  const json::object& root_;
  const std::vector<std::uint8_t>& binary_;
  std::unordered_set<std::size_t> activeNodes_;
  GlbScene scene_;
};

} // namespace

GlbScene readGlbScene(const std::string& path)
{
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  if (!input) {
    throw std::runtime_error("cannot open GLB: " + path);
  }
  const std::streamsize fileSize = input.tellg();
  if (fileSize < 20) {
    throw std::runtime_error("GLB file is too small");
  }
  input.seekg(0);
  std::vector<std::uint8_t> data(static_cast<std::size_t>(fileSize));
  if (!input.read(reinterpret_cast<char*>(data.data()), fileSize)) {
    throw std::runtime_error("cannot read GLB: " + path);
  }
  if (readU32(data, 0) != GlbMagic || readU32(data, 4) != 2) {
    throw std::runtime_error("input is not a GLB 2.0 file");
  }
  const std::size_t declaredLength = readU32(data, 8);
  if (declaredLength != data.size()) {
    throw std::runtime_error("GLB declared length does not match file size");
  }

  std::string jsonText;
  std::vector<std::uint8_t> binary;
  std::size_t offset = 12;
  while (offset + 8 <= data.size()) {
    const std::size_t chunkLength = readU32(data, offset);
    const std::uint32_t chunkType = readU32(data, offset + 4);
    offset += 8;
    if (chunkLength > data.size() - offset) {
      throw std::runtime_error("GLB chunk exceeds file size");
    }
    if (chunkType == JsonChunk) {
      jsonText.assign(reinterpret_cast<const char*>(data.data() + offset), chunkLength);
    }
    else if (chunkType == BinChunk) {
      binary.assign(data.begin() + static_cast<std::ptrdiff_t>(offset),
                    data.begin() + static_cast<std::ptrdiff_t>(offset + chunkLength));
    }
    offset += chunkLength;
  }
  if (jsonText.empty() || binary.empty()) {
    throw std::runtime_error("GLB must contain JSON and BIN chunks");
  }

  const json::value document = json::parse(jsonText);
  const json::object& root = document.as_object();
  const json::object& asset = required(root, "asset").as_object();
  const std::string version = required(asset, "version").as_string().c_str();
  if (version.empty() || version[0] != '2') {
    throw std::runtime_error("only glTF 2.x assets are supported");
  }
  if (const json::value* requiredExtensions = optional(root, "extensionsRequired")) {
    static const std::unordered_set<std::string> geometryNeutralExtensions{
      "EXT_texture_avif",
      "EXT_texture_webp",
      "KHR_lights_punctual",
      "KHR_materials_anisotropy",
      "KHR_materials_clearcoat",
      "KHR_materials_dispersion",
      "KHR_materials_emissive_strength",
      "KHR_materials_ior",
      "KHR_materials_iridescence",
      "KHR_materials_pbrSpecularGlossiness",
      "KHR_materials_sheen",
      "KHR_materials_specular",
      "KHR_materials_transmission",
      "KHR_materials_unlit",
      "KHR_materials_variants",
      "KHR_materials_volume",
      "KHR_texture_basisu",
      "KHR_texture_transform",
      "KHR_xmp_json_ld",
    };
    for (const json::value& extensionValue : requiredExtensions->as_array()) {
      const std::string extension = extensionValue.as_string().c_str();
      if (geometryNeutralExtensions.find(extension) == geometryNeutralExtensions.end()) {
        throw std::runtime_error("unsupported required GLB extension: " + extension);
      }
    }
  }
  const json::array& buffers = required(root, "buffers").as_array();
  if (buffers.empty() || optional(buffers[0].as_object(), "uri") != nullptr) {
    throw std::runtime_error("GLB external buffers are not supported");
  }
  return SceneParser(root, binary).parse();
}
