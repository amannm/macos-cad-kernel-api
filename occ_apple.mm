#import "occ_apple.h"
#include "occ_c.h"

#include <cstring>
#include <limits>
#include <new>

struct occ_apple_metal_mesh_t {
  id<MTLBuffer> vertices;
  id<MTLBuffer> positions;
  id<MTLBuffer> normals;
  id<MTLBuffer> uvs;
  id<MTLBuffer> indices;
  occ_mesh_packed_layout_t layout;
};

namespace {

bool apple_mul_size(size_t a, size_t b, size_t& out)
{
  if (a != 0 && b > std::numeric_limits<size_t>::max() / a) {
    return false;
  }
  out = a * b;
  return true;
}

id<MTLBuffer> apple_buffer(id<MTLDevice> device, size_t size)
{
  return size == 0 ? nil : [device newBufferWithLength:(NSUInteger)size options:MTLResourceStorageModeShared];
}

void apple_mesh_destroy(occ_apple_metal_mesh_t* mesh)
{
  if (mesh == nullptr) {
    return;
  }
  [mesh->vertices release];
  [mesh->positions release];
  [mesh->normals release];
  [mesh->uvs release];
  [mesh->indices release];
  delete mesh;
}

occ_status_t apple_mesh_validate(occ_mesh_view_t view)
{
  if ((view.vertex_count != 0 && view.positions == nullptr) || (view.triangle_count != 0 && view.triangles == nullptr)) {
    return OCC_NULL_ARGUMENT;
  }
  return OCC_OK;
}

}

OCC_APPLE_API occ_status_t OCC_APPLE_CALL occ_apple_metal_mesh_create(id<MTLDevice> device, occ_mesh_view_t view, occ_apple_metal_mesh_t** out_mesh)
{
  if (device == nil || out_mesh == nullptr) {
    return OCC_NULL_ARGUMENT;
  }
  *out_mesh = nullptr;
  occ_status_t status = apple_mesh_validate(view);
  if (status != OCC_OK) {
    return status;
  }
  size_t position_bytes = 0;
  size_t normal_bytes = 0;
  size_t uv_bytes = 0;
  size_t index_count = 0;
  size_t index_bytes = 0;
  if (!apple_mul_size(view.vertex_count, sizeof(occ_vec3_t), position_bytes) ||
      !apple_mul_size(view.vertex_count, sizeof(occ_vec3_t), normal_bytes) ||
      !apple_mul_size(view.vertex_count, sizeof(occ_vec2_t), uv_bytes) ||
      !apple_mul_size(view.triangle_count, size_t(3), index_count) ||
      !apple_mul_size(view.triangle_count, sizeof(occ_triangle_t), index_bytes)) {
    return OCC_INVALID_ARGUMENT;
  }
  auto* mesh = new (std::nothrow) occ_apple_metal_mesh_t{};
  if (mesh == nullptr) {
    return OCC_OUT_OF_MEMORY;
  }
  mesh->positions = apple_buffer(device, position_bytes);
  mesh->normals = view.normals != nullptr ? apple_buffer(device, normal_bytes) : nil;
  mesh->uvs = view.uvs != nullptr ? apple_buffer(device, uv_bytes) : nil;
  mesh->indices = apple_buffer(device, index_bytes);
  if ((position_bytes != 0 && mesh->positions == nil) ||
      (view.normals != nullptr && normal_bytes != 0 && mesh->normals == nil) ||
      (view.uvs != nullptr && uv_bytes != 0 && mesh->uvs == nil) ||
      (index_bytes != 0 && mesh->indices == nil)) {
    apple_mesh_destroy(mesh);
    return OCC_OUT_OF_MEMORY;
  }
  if (position_bytes != 0) {
    std::memcpy([mesh->positions contents], view.positions, position_bytes);
  }
  if (normal_bytes != 0 && mesh->normals != nil) {
    std::memcpy([mesh->normals contents], view.normals, normal_bytes);
  }
  if (uv_bytes != 0 && mesh->uvs != nil) {
    std::memcpy([mesh->uvs contents], view.uvs, uv_bytes);
  }
  if (index_bytes != 0) {
    std::memcpy([mesh->indices contents], view.triangles, index_bytes);
  }
  mesh->layout.vertex_stride = sizeof(occ_vec3_t);
  mesh->layout.vertex_count = view.vertex_count;
  mesh->layout.vertex_bytes = position_bytes;
  mesh->layout.index_format = OCC_MESH_INDEX_UINT32;
  mesh->layout.index_stride = sizeof(uint32_t);
  mesh->layout.index_count = index_count;
  mesh->layout.index_bytes = index_bytes;
  *out_mesh = mesh;
  return OCC_OK;
}

OCC_APPLE_API occ_status_t OCC_APPLE_CALL occ_apple_metal_mesh_create_packed(id<MTLDevice> device, occ_mesh_view_t view, const occ_mesh_packed_options_t* options_or_null, occ_apple_metal_mesh_t** out_mesh)
{
  if (device == nil || out_mesh == nullptr) {
    return OCC_NULL_ARGUMENT;
  }
  *out_mesh = nullptr;
  occ_mesh_packed_layout_t layout{};
  occ_status_t status = occ_mesh_packed_layout(view, options_or_null, &layout);
  if (status != OCC_OK) {
    return status;
  }
  auto* mesh = new (std::nothrow) occ_apple_metal_mesh_t{};
  if (mesh == nullptr) {
    return OCC_OUT_OF_MEMORY;
  }
  mesh->vertices = apple_buffer(device, layout.vertex_bytes);
  mesh->indices = apple_buffer(device, layout.index_bytes);
  if ((layout.vertex_bytes != 0 && mesh->vertices == nil) || (layout.index_bytes != 0 && mesh->indices == nil)) {
    apple_mesh_destroy(mesh);
    return OCC_OUT_OF_MEMORY;
  }
  status = occ_mesh_write_packed(view, options_or_null, layout.vertex_bytes == 0 ? nullptr : [mesh->vertices contents], layout.vertex_bytes, layout.index_bytes == 0 ? nullptr : [mesh->indices contents], layout.index_bytes, &mesh->layout);
  if (status != OCC_OK) {
    apple_mesh_destroy(mesh);
    return status;
  }
  *out_mesh = mesh;
  return OCC_OK;
}

OCC_APPLE_API void OCC_APPLE_CALL occ_apple_metal_mesh_destroy(occ_apple_metal_mesh_t* mesh)
{
  apple_mesh_destroy(mesh);
}

OCC_APPLE_API id<MTLBuffer> OCC_APPLE_CALL occ_apple_metal_mesh_vertices(const occ_apple_metal_mesh_t* mesh)
{
  return mesh == nullptr ? nil : mesh->vertices;
}

OCC_APPLE_API id<MTLBuffer> OCC_APPLE_CALL occ_apple_metal_mesh_positions(const occ_apple_metal_mesh_t* mesh)
{
  return mesh == nullptr ? nil : mesh->positions;
}

OCC_APPLE_API id<MTLBuffer> OCC_APPLE_CALL occ_apple_metal_mesh_normals(const occ_apple_metal_mesh_t* mesh)
{
  return mesh == nullptr ? nil : mesh->normals;
}

OCC_APPLE_API id<MTLBuffer> OCC_APPLE_CALL occ_apple_metal_mesh_uvs(const occ_apple_metal_mesh_t* mesh)
{
  return mesh == nullptr ? nil : mesh->uvs;
}

OCC_APPLE_API id<MTLBuffer> OCC_APPLE_CALL occ_apple_metal_mesh_indices(const occ_apple_metal_mesh_t* mesh)
{
  return mesh == nullptr ? nil : mesh->indices;
}

OCC_APPLE_API NSUInteger OCC_APPLE_CALL occ_apple_metal_mesh_vertex_stride(const occ_apple_metal_mesh_t* mesh)
{
  return mesh == nullptr ? 0 : (NSUInteger)mesh->layout.vertex_stride;
}

OCC_APPLE_API NSUInteger OCC_APPLE_CALL occ_apple_metal_mesh_vertex_count(const occ_apple_metal_mesh_t* mesh)
{
  return mesh == nullptr ? 0 : (NSUInteger)mesh->layout.vertex_count;
}

OCC_APPLE_API NSUInteger OCC_APPLE_CALL occ_apple_metal_mesh_triangle_count(const occ_apple_metal_mesh_t* mesh)
{
  return mesh == nullptr ? 0 : (NSUInteger)(mesh->layout.index_count / 3);
}

OCC_APPLE_API NSUInteger OCC_APPLE_CALL occ_apple_metal_mesh_index_count(const occ_apple_metal_mesh_t* mesh)
{
  return mesh == nullptr ? 0 : (NSUInteger)mesh->layout.index_count;
}

OCC_APPLE_API occ_mesh_vertex_format_t OCC_APPLE_CALL occ_apple_metal_mesh_vertex_format(const occ_apple_metal_mesh_t* mesh)
{
  return mesh == nullptr ? (occ_mesh_vertex_format_t)0 : mesh->layout.vertex_format;
}

OCC_APPLE_API occ_mesh_index_format_t OCC_APPLE_CALL occ_apple_metal_mesh_index_format(const occ_apple_metal_mesh_t* mesh)
{
  return mesh == nullptr ? (occ_mesh_index_format_t)0 : mesh->layout.index_format;
}

OCC_APPLE_API MTLIndexType OCC_APPLE_CALL occ_apple_metal_mesh_index_type(const occ_apple_metal_mesh_t* mesh)
{
  return mesh != nullptr && mesh->layout.index_format == OCC_MESH_INDEX_UINT16 ? MTLIndexTypeUInt16 : MTLIndexTypeUInt32;
}
