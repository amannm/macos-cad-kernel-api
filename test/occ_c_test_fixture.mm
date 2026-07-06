#include "occ_c.h"
#import "occ_apple.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

void fail(const char *message)
{
  std::fputs(message, stderr);
  std::fputc('\n', stderr);
  std::exit(1);
}

void check(occ_context_t *context, occ_status_t status, const char *call)
{
  if (status == OCC_OK) {
    return;
  }
  const char *error = nullptr;
  if (context != nullptr && occ_context_last_error(context, &error) == OCC_OK && error != nullptr && error[0] != 0) {
    std::fprintf(stderr, "%s: %s: %s\n", call, occ_status_string(status), error);
  } else {
    std::fprintf(stderr, "%s: %s\n", call, occ_status_string(status));
  }
  std::exit(1);
}

void expect(bool condition, const char *message)
{
  if (!condition) {
    fail(message);
  }
}

template<class T>
struct handle {
  T *p = nullptr;
  ~handle() { occ_release(p); }
  T **out() { return &p; }
  T *get() const { return p; }
};

struct context {
  occ_context_t *p = nullptr;
  context() { check(nullptr, occ_context_create(&p), "occ_context_create"); }
  ~context() { occ_context_destroy(p); }
  occ_context_t *get() const { return p; }
};

struct metal_mesh {
  occ_apple_metal_mesh_t *p = nullptr;
  ~metal_mesh() { occ_apple_metal_mesh_destroy(p); }
  occ_apple_metal_mesh_t **out() { return &p; }
  occ_apple_metal_mesh_t *get() const { return p; }
};

double absdiff(double a, double b)
{
  return std::fabs(a - b);
}

}

int main()
{
  @autoreleasepool {
  const occ_version_t version = occ_version();
  expect(version.abi_major == OCC_C_ABI_VERSION_MAJOR, "unexpected ABI major");
  expect(version.abi_minor == OCC_C_ABI_VERSION_MINOR, "unexpected ABI minor");
  expect(version.abi_patch == OCC_C_ABI_VERSION_PATCH, "unexpected ABI patch");
  expect(occ_status_string(OCC_OK) != nullptr, "missing status string");

  context c;
  handle<occ_shape_t> box;
  check(c.get(), occ_make_box(c.get(), 2.0, 3.0, 4.0, box.out()), "occ_make_box");
  expect(box.get() != nullptr, "box was not created");
  expect(occ_handle_kind(box.get()) == OCC_HANDLE_SHAPE, "box handle kind mismatch");

  occ_shape_type_t type = OCC_SHAPE_UNKNOWN;
  check(c.get(), occ_shape_type(box.get(), &type), "occ_shape_type");
  expect(type == OCC_SHAPE_SOLID, "box is not a solid");

  occ_shape_validity_t validity = OCC_SHAPE_VALIDITY_UNKNOWN;
  check(c.get(), occ_shape_check(c.get(), box.get(), &validity), "occ_shape_check");
  expect(validity == OCC_SHAPE_VALID, "box is invalid");

  size_t face_count = 0;
  check(c.get(), occ_shape_count_subshapes(c.get(), box.get(), OCC_SHAPE_FACE, OCC_TRAVERSE_RECURSIVE_UNIQUE, &face_count), "occ_shape_count_subshapes");
  expect(face_count == 6, "box should have six unique faces");

  occ_mass_properties_t volume{};
  check(c.get(), occ_shape_volume_properties(c.get(), box.get(), &volume), "occ_shape_volume_properties");
  expect(absdiff(volume.measure, 24.0) < 1e-7, "box volume mismatch");
  expect(absdiff(volume.center_of_mass.x, 1.0) < 1e-7, "box center x mismatch");
  expect(absdiff(volume.center_of_mass.y, 1.5) < 1e-7, "box center y mismatch");
  expect(absdiff(volume.center_of_mass.z, 2.0) < 1e-7, "box center z mismatch");

  occ_bbox_t bbox{};
  check(c.get(), occ_shape_bbox(c.get(), box.get(), OCC_C_FALSE, &bbox), "occ_shape_bbox");
  expect(bbox.is_empty == OCC_C_FALSE, "box bbox is empty");
  expect(bbox.xmin <= 0.0 && bbox.ymin <= 0.0 && bbox.zmin <= 0.0, "box bbox min mismatch");
  expect(bbox.xmax >= 2.0 && bbox.ymax >= 3.0 && bbox.zmax >= 4.0, "box bbox max mismatch");

  void *brep = nullptr;
  size_t brep_size = 0;
  check(c.get(), occ_shape_write_brep_memory(c.get(), box.get(), OCC_BREP_ASCII, &brep, &brep_size), "occ_shape_write_brep_memory");
  expect(brep != nullptr && brep_size != 0, "empty BREP payload");
  handle<occ_shape_t> roundtrip;
  check(c.get(), occ_shape_read_brep_memory(c.get(), brep, brep_size, roundtrip.out()), "occ_shape_read_brep_memory");
  occ_free(brep);
  check(c.get(), occ_shape_type(roundtrip.get(), &type), "roundtrip occ_shape_type");
  expect(type == OCC_SHAPE_SOLID, "roundtrip shape is not a solid");

  handle<occ_shape_t> triangulated;
  occ_mesh_options_t mesh_options = occ_mesh_options_default();
  mesh_options.linear_deflection = 0.25;
  mesh_options.angular_deflection = 0.5;
  check(c.get(), occ_shape_triangulate(c.get(), box.get(), &mesh_options, triangulated.out()), "occ_shape_triangulate");

  handle<occ_shape_t> face;
  check(c.get(), occ_shape_subshape_at(c.get(), triangulated.get(), OCC_SHAPE_FACE, OCC_TRAVERSE_RECURSIVE_UNIQUE, 0, face.out()), "occ_shape_subshape_at");
  handle<occ_mesh_t> mesh;
  check(c.get(), occ_face_triangulation(c.get(), face.get(), OCC_C_TRUE, mesh.out()), "occ_face_triangulation");

  occ_mesh_view_t view{};
  check(c.get(), occ_mesh_view(mesh.get(), &view), "occ_mesh_view");
  expect(view.positions != nullptr && view.vertex_count >= 4, "mesh vertices missing");
  expect(view.triangles != nullptr && view.triangle_count >= 2, "mesh triangles missing");

  id<MTLDevice> device = MTLCreateSystemDefaultDevice();
  expect(device != nil, "missing Metal device");

  metal_mesh metal;
  check(c.get(), occ_apple_metal_mesh_create(device, view, metal.out()), "occ_apple_metal_mesh_create");
  expect(metal.get() != nullptr, "Metal mesh was not created");
  expect(occ_apple_metal_mesh_positions(metal.get()) != nil, "Metal positions buffer missing");
  expect(occ_apple_metal_mesh_indices(metal.get()) != nil, "Metal indices buffer missing");
  expect(occ_apple_metal_mesh_vertex_stride(metal.get()) == sizeof(occ_vec3_t), "Metal vertex stride mismatch");
  expect(occ_apple_metal_mesh_vertex_count(metal.get()) == view.vertex_count, "Metal vertex count mismatch");
  expect(occ_apple_metal_mesh_triangle_count(metal.get()) == view.triangle_count, "Metal triangle count mismatch");
  expect(occ_apple_metal_mesh_index_count(metal.get()) == view.triangle_count * 3, "Metal index count mismatch");
  expect(occ_apple_metal_mesh_index_type(metal.get()) == MTLIndexTypeUInt32, "Metal index type mismatch");

  occ_mesh_packed_options_t packed_options = occ_mesh_packed_options_default();
  packed_options.vertex_format = OCC_MESH_VERTEX_FLOAT32_P;
  packed_options.index_format = OCC_MESH_INDEX_UINT32;
  occ_mesh_packed_layout_t layout{};
  check(c.get(), occ_mesh_packed_layout(view, &packed_options, &layout), "occ_mesh_packed_layout");
  expect(layout.vertex_count == view.vertex_count, "packed vertex count mismatch");
  expect(layout.index_count == view.triangle_count * 3, "packed index count mismatch");

  std::vector<uint8_t> vertices(layout.vertex_bytes);
  std::vector<uint8_t> indices(layout.index_bytes);
  occ_mesh_packed_layout_t written{};
  check(c.get(), occ_mesh_write_packed(view, &packed_options, vertices.data(), vertices.size(), indices.data(), indices.size(), &written), "occ_mesh_write_packed");
  expect(written.vertex_bytes == layout.vertex_bytes, "written vertex byte count mismatch");
  expect(written.index_bytes == layout.index_bytes, "written index byte count mismatch");

  metal_mesh packed_metal;
  check(c.get(), occ_apple_metal_mesh_create_packed(device, view, &packed_options, packed_metal.out()), "occ_apple_metal_mesh_create_packed");
  id<MTLBuffer> metal_vertices = occ_apple_metal_mesh_vertices(packed_metal.get());
  id<MTLBuffer> metal_indices = occ_apple_metal_mesh_indices(packed_metal.get());
  expect(metal_vertices != nil, "packed Metal vertices buffer missing");
  expect(metal_indices != nil, "packed Metal indices buffer missing");
  expect([metal_vertices length] == layout.vertex_bytes, "packed Metal vertex byte count mismatch");
  expect([metal_indices length] == layout.index_bytes, "packed Metal index byte count mismatch");
  expect(occ_apple_metal_mesh_vertex_format(packed_metal.get()) == OCC_MESH_VERTEX_FLOAT32_P, "packed Metal vertex format mismatch");
  expect(occ_apple_metal_mesh_index_format(packed_metal.get()) == OCC_MESH_INDEX_UINT32, "packed Metal index format mismatch");
  expect(occ_apple_metal_mesh_vertex_stride(packed_metal.get()) == sizeof(occ_mesh_vertex_float32_p_t), "packed Metal vertex stride mismatch");
  expect(occ_apple_metal_mesh_vertex_count(packed_metal.get()) == view.vertex_count, "packed Metal vertex count mismatch");
  expect(occ_apple_metal_mesh_index_count(packed_metal.get()) == view.triangle_count * 3, "packed Metal index count mismatch");
  expect(std::memcmp([metal_vertices contents], vertices.data(), layout.vertex_bytes) == 0, "packed Metal vertex contents mismatch");
  expect(std::memcmp([metal_indices contents], indices.data(), layout.index_bytes) == 0, "packed Metal index contents mismatch");

  return 0;
  }
}
