cmake_minimum_required(VERSION 3.20)
project(Hands3DLabVoxKit VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Python3 COMPONENTS Interpreter REQUIRED)

find_path(BOOST_JSON_INCLUDE_DIR
  NAMES boost/json/src.hpp
)
if(NOT BOOST_JSON_INCLUDE_DIR)
  message(FATAL_ERROR "Boost.JSON headers are required to parse GLB files")
endif()

option(ENABLE_GPU "Enable native GPU backends when available" ON)
option(BUILD_TESTS "Enable CTest integration" ON)

add_library(voxkit_core STATIC
  src/glb_mesh_reader.cpp
  src/voxelizer.cpp
)

target_include_directories(voxkit_core PUBLIC
  ${BOOST_JSON_INCLUDE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(APPLE AND ENABLE_GPU)
  enable_language(OBJCXX)
  find_library(METAL_FRAMEWORK Metal)
  find_library(FOUNDATION_FRAMEWORK Foundation)
  if(METAL_FRAMEWORK AND FOUNDATION_FRAMEWORK)
    target_sources(voxkit_core PRIVATE src/metal_voxelizer.mm)
    target_link_libraries(voxkit_core PRIVATE ${METAL_FRAMEWORK} ${FOUNDATION_FRAMEWORK})
    target_compile_definitions(voxkit_core PRIVATE VOXKIT_ENABLE_METAL=1)
  endif()
endif()

add_executable(voxkit src/main.cpp)
target_link_libraries(voxkit PRIVATE voxkit_core)

install(TARGETS voxkit_core voxkit
  ARCHIVE DESTINATION lib
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
)
install(FILES src/voxelizer.hpp src/glb_mesh_reader.hpp DESTINATION include/voxkit)

if(BUILD_TESTS)
  enable_testing()
  add_test(
    NAME sample_voxkit
    COMMAND ${CMAKE_COMMAND} -E env VOXKIT_BIN=$<TARGET_FILE:voxkit>
      ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_voxkit.py
  )
endif()
