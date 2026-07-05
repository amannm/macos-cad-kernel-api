#ifndef OCC_C_H
#define OCC_C_H

/*
  occ_c.h - A stable C ABI facade for a practical subset of Open CASCADE OCCT.

  Target OCCT toolkits:
    TKernel, TKMath, TKG2d, TKG3d, TKGeomBase, TKBRep,
    TKGeomAlgo, TKTopAlgo, TKPrim, TKBO, TKBool

  Design principles:
    - C99-compatible public API.
    - No C++ types, exceptions, STL containers, RTTI, or OCCT class layouts cross the ABI.
    - All OCCT-backed values are opaque handles.
    - All returned handles are released with occ_object_release().
    - All returned memory buffers are released with occ_free_memory().
    - All strings are UTF-8.
    - All angles are radians.
    - All transforms are row-major 4x4 matrices.

  This is a header-only declaration file. Implementations should be provided
  by a C++ shared library that links against OCCT.
*/

#include <stdint.h>
#include <stddef.h>

#if defined(_WIN32) || defined(__CYGWIN__)
  #if defined(OCC_C_BUILD)
    #define OCC_C_API __declspec(dllexport)
  #else
    #define OCC_C_API __declspec(dllimport)
  #endif
  #define OCC_C_CALL __cdecl
#else
  #if defined(__GNUC__) || defined(__clang__)
    #define OCC_C_API __attribute__((visibility("default")))
  #else
    #define OCC_C_API
  #endif
  #define OCC_C_CALL
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define OCC_C_ABI_VERSION_MAJOR 1u
#define OCC_C_ABI_VERSION_MINOR 0u
#define OCC_C_ABI_VERSION_PATCH 0u

/* -------------------------------------------------------------------------- */
/* Opaque handle types                                                        */
/* -------------------------------------------------------------------------- */

typedef struct occ_context_t  occ_context_t;
typedef struct occ_object_t   occ_object_t;

typedef occ_object_t occ_geom2d_t;
typedef occ_object_t occ_geom3d_t;
typedef occ_object_t occ_curve2d_t;
typedef occ_object_t occ_curve3d_t;
typedef occ_object_t occ_surface_t;
typedef occ_object_t occ_shape_t;
typedef occ_object_t occ_mesh_t;
typedef occ_object_t occ_iterator_t;
typedef occ_object_t occ_builder_t;
typedef occ_object_t occ_error_t;

/* -------------------------------------------------------------------------- */
/* Status, versioning, logging                                                */
/* -------------------------------------------------------------------------- */

typedef enum occ_status_t {
  OCC_STATUS_OK = 0,
  OCC_STATUS_ERROR = 1,
  OCC_STATUS_NULL_ARGUMENT = 2,
  OCC_STATUS_BAD_TYPE = 3,
  OCC_STATUS_OUT_OF_MEMORY = 4,
  OCC_STATUS_INVALID_ARGUMENT = 5,
  OCC_STATUS_OCCT_EXCEPTION = 6,
  OCC_STATUS_NOT_IMPLEMENTED = 7,
  OCC_STATUS_EMPTY_RESULT = 8,
  OCC_STATUS_NON_MANIFOLD = 9,
  OCC_STATUS_INVALID_SHAPE = 10,
  OCC_STATUS_INDEX_OUT_OF_RANGE = 11,
  OCC_STATUS_IO_ERROR = 12,
  OCC_STATUS_PARSE_ERROR = 13
} occ_status_t;

typedef enum occ_log_level_t {
  OCC_LOG_TRACE = 0,
  OCC_LOG_INFO = 1,
  OCC_LOG_WARNING = 2,
  OCC_LOG_ERROR = 3
} occ_log_level_t;

typedef void (OCC_C_CALL *occ_log_callback_t)(
  occ_log_level_t level,
  const char* message,
  void* user_data
);

typedef struct occ_version_t {
  uint32_t abi_major;
  uint32_t abi_minor;
  uint32_t abi_patch;
  uint32_t occt_major;
  uint32_t occt_minor;
  uint32_t occt_patch;
} occ_version_t;

OCC_C_API occ_version_t OCC_C_CALL occ_get_version(void);
OCC_C_API const char*   OCC_C_CALL occ_status_name(occ_status_t status);

OCC_C_API occ_status_t OCC_C_CALL occ_context_create(occ_context_t** out_ctx);
OCC_C_API void         OCC_C_CALL occ_context_destroy(occ_context_t* ctx);

OCC_C_API occ_status_t OCC_C_CALL occ_context_set_log_callback(
  occ_context_t* ctx,
  occ_log_callback_t callback,
  void* user_data
);

OCC_C_API occ_status_t OCC_C_CALL occ_context_get_last_error(
  occ_context_t* ctx,
  const char** out_message
);

/* -------------------------------------------------------------------------- */
/* Object lifetime                                                            */
/* -------------------------------------------------------------------------- */

typedef enum occ_object_kind_t {
  OCC_OBJECT_UNKNOWN = 0,
  OCC_OBJECT_CURVE2D = 1,
  OCC_OBJECT_CURVE3D = 2,
  OCC_OBJECT_SURFACE = 3,
  OCC_OBJECT_SHAPE = 4,
  OCC_OBJECT_MESH = 5,
  OCC_OBJECT_ITERATOR = 6,
  OCC_OBJECT_BUILDER = 7,
  OCC_OBJECT_ERROR = 8
} occ_object_kind_t;

OCC_C_API void              OCC_C_CALL occ_object_retain(occ_object_t* object);
OCC_C_API void              OCC_C_CALL occ_object_release(occ_object_t* object);
OCC_C_API occ_object_kind_t OCC_C_CALL occ_object_kind(const occ_object_t* object);

OCC_C_API void OCC_C_CALL occ_free_memory(void* ptr);

/* -------------------------------------------------------------------------- */
/* Math value types                                                           */
/* -------------------------------------------------------------------------- */

typedef struct occ_vec2_t {
  double x;
  double y;
} occ_vec2_t;

typedef struct occ_vec3_t {
  double x;
  double y;
  double z;
} occ_vec3_t;

typedef struct occ_quat_t {
  double x;
  double y;
  double z;
  double w;
} occ_quat_t;

typedef struct occ_axis1_t {
  occ_vec3_t origin;
  occ_vec3_t direction;
} occ_axis1_t;

typedef struct occ_axis2_t {
  occ_vec3_t origin;
  occ_vec3_t z_direction;
  occ_vec3_t x_direction;
} occ_axis2_t;

typedef struct occ_mat4_t {
  double m[16]; /* row-major */
} occ_mat4_t;

typedef struct occ_bbox_t {
  double xmin;
  double ymin;
  double zmin;
  double xmax;
  double ymax;
  double zmax;
  int32_t is_void;
} occ_bbox_t;

typedef struct occ_tolerances_t {
  double linear;
  double angular;
} occ_tolerances_t;

OCC_C_API occ_vec3_t OCC_C_CALL occ_vec3_add(occ_vec3_t a, occ_vec3_t b);
OCC_C_API occ_vec3_t OCC_C_CALL occ_vec3_sub(occ_vec3_t a, occ_vec3_t b);
OCC_C_API occ_vec3_t OCC_C_CALL occ_vec3_cross(occ_vec3_t a, occ_vec3_t b);
OCC_C_API double     OCC_C_CALL occ_vec3_dot(occ_vec3_t a, occ_vec3_t b);
OCC_C_API double     OCC_C_CALL occ_vec3_length(occ_vec3_t v);

OCC_C_API occ_mat4_t OCC_C_CALL occ_mat4_identity(void);
OCC_C_API occ_mat4_t OCC_C_CALL occ_mat4_translation(double x, double y, double z);
OCC_C_API occ_mat4_t OCC_C_CALL occ_mat4_rotation_axis_angle(occ_vec3_t axis, double radians);
OCC_C_API occ_mat4_t OCC_C_CALL occ_mat4_scale(double sx, double sy, double sz);
OCC_C_API occ_mat4_t OCC_C_CALL occ_mat4_mul(occ_mat4_t a, occ_mat4_t b);

/* -------------------------------------------------------------------------- */
/* Geometry                                                                   */
/* -------------------------------------------------------------------------- */

typedef enum occ_curve_type_t {
  OCC_CURVE_UNKNOWN = 0,
  OCC_CURVE_LINE = 1,
  OCC_CURVE_CIRCLE = 2,
  OCC_CURVE_ELLIPSE = 3,
  OCC_CURVE_PARABOLA = 4,
  OCC_CURVE_HYPERBOLA = 5,
  OCC_CURVE_BEZIER = 6,
  OCC_CURVE_BSPLINE = 7,
  OCC_CURVE_OFFSET = 8
} occ_curve_type_t;

typedef enum occ_surface_type_t {
  OCC_SURFACE_UNKNOWN = 0,
  OCC_SURFACE_PLANE = 1,
  OCC_SURFACE_CYLINDER = 2,
  OCC_SURFACE_CONE = 3,
  OCC_SURFACE_SPHERE = 4,
  OCC_SURFACE_TORUS = 5,
  OCC_SURFACE_BEZIER = 6,
  OCC_SURFACE_BSPLINE = 7,
  OCC_SURFACE_REVOLUTION = 8,
  OCC_SURFACE_EXTRUSION = 9,
  OCC_SURFACE_OFFSET = 10
} occ_surface_type_t;

OCC_C_API occ_status_t OCC_C_CALL occ_curve3d_make_line(
  occ_context_t* ctx,
  occ_vec3_t point,
  occ_vec3_t direction,
  occ_curve3d_t** out_curve
);

OCC_C_API occ_status_t OCC_C_CALL occ_curve3d_make_circle(
  occ_context_t* ctx,
  occ_axis2_t frame,
  double radius,
  occ_curve3d_t** out_curve
);

OCC_C_API occ_status_t OCC_C_CALL occ_curve3d_type(
  const occ_curve3d_t* curve,
  occ_curve_type_t* out_type
);

OCC_C_API occ_status_t OCC_C_CALL occ_curve3d_bounds(
  const occ_curve3d_t* curve,
  double* out_first,
  double* out_last
);

OCC_C_API occ_status_t OCC_C_CALL occ_curve3d_eval(
  const occ_curve3d_t* curve,
  double u,
  occ_vec3_t* out_point
);

OCC_C_API occ_status_t OCC_C_CALL occ_curve3d_eval_d1(
  const occ_curve3d_t* curve,
  double u,
  occ_vec3_t* out_point,
  occ_vec3_t* out_du
);

OCC_C_API occ_status_t OCC_C_CALL occ_surface_make_plane(
  occ_context_t* ctx,
  occ_axis2_t frame,
  occ_surface_t** out_surface
);

OCC_C_API occ_status_t OCC_C_CALL occ_surface_make_cylinder(
  occ_context_t* ctx,
  occ_axis2_t frame,
  double radius,
  occ_surface_t** out_surface
);

OCC_C_API occ_status_t OCC_C_CALL occ_surface_make_sphere(
  occ_context_t* ctx,
  occ_axis2_t frame,
  double radius,
  occ_surface_t** out_surface
);

OCC_C_API occ_status_t OCC_C_CALL occ_surface_type(
  const occ_surface_t* surface,
  occ_surface_type_t* out_type
);

OCC_C_API occ_status_t OCC_C_CALL occ_surface_eval(
  const occ_surface_t* surface,
  double u,
  double v,
  occ_vec3_t* out_point
);

OCC_C_API occ_status_t OCC_C_CALL occ_surface_eval_d1(
  const occ_surface_t* surface,
  double u,
  double v,
  occ_vec3_t* out_point,
  occ_vec3_t* out_du,
  occ_vec3_t* out_dv
);

OCC_C_API occ_status_t OCC_C_CALL occ_project_point_on_curve3d(
  occ_context_t* ctx,
  occ_vec3_t point,
  const occ_curve3d_t* curve,
  double* out_u,
  occ_vec3_t* out_nearest,
  double* out_distance
);

OCC_C_API occ_status_t OCC_C_CALL occ_project_point_on_surface(
  occ_context_t* ctx,
  occ_vec3_t point,
  const occ_surface_t* surface,
  double* out_u,
  double* out_v,
  occ_vec3_t* out_nearest,
  double* out_distance
);

/* -------------------------------------------------------------------------- */
/* BRep and topology                                                          */
/* -------------------------------------------------------------------------- */

typedef enum occ_shape_type_t {
  OCC_SHAPE_COMPOUND = 0,
  OCC_SHAPE_COMPSOLID = 1,
  OCC_SHAPE_SOLID = 2,
  OCC_SHAPE_SHELL = 3,
  OCC_SHAPE_FACE = 4,
  OCC_SHAPE_WIRE = 5,
  OCC_SHAPE_EDGE = 6,
  OCC_SHAPE_VERTEX = 7,
  OCC_SHAPE_UNKNOWN = 8
} occ_shape_type_t;

typedef enum occ_orientation_t {
  OCC_ORIENTATION_FORWARD = 0,
  OCC_ORIENTATION_REVERSED = 1,
  OCC_ORIENTATION_INTERNAL = 2,
  OCC_ORIENTATION_EXTERNAL = 3
} occ_orientation_t;

OCC_C_API occ_status_t OCC_C_CALL occ_shape_type(
  const occ_shape_t* shape,
  occ_shape_type_t* out_type
);

OCC_C_API occ_status_t OCC_C_CALL occ_shape_is_null(
  const occ_shape_t* shape,
  int32_t* out_is_null
);

OCC_C_API occ_status_t OCC_C_CALL occ_shape_orientation(
  const occ_shape_t* shape,
  occ_orientation_t* out_orientation
);

OCC_C_API occ_status_t OCC_C_CALL occ_shape_reversed(
  occ_context_t* ctx,
  const occ_shape_t* shape,
  occ_shape_t** out_shape
);

OCC_C_API occ_status_t OCC_C_CALL occ_shape_transformed(
  occ_context_t* ctx,
  const occ_shape_t* shape,
  occ_mat4_t transform,
  occ_shape_t** out_shape
);

OCC_C_API occ_status_t OCC_C_CALL occ_shape_bbox(
  occ_context_t* ctx,
  const occ_shape_t* shape,
  occ_bbox_t* out_bbox
);

OCC_C_API occ_status_t OCC_C_CALL occ_shape_count_subshapes(
  occ_context_t* ctx,
  const occ_shape_t* shape,
  occ_shape_type_t type,
  uint64_t* out_count
);

OCC_C_API occ_status_t OCC_C_CALL occ_shape_subshape_at(
  occ_context_t* ctx,
  const occ_shape_t* shape,
  occ_shape_type_t type,
  uint64_t index,
  occ_shape_t** out_subshape
);

OCC_C_API occ_status_t OCC_C_CALL occ_shape_iterator_create(
  occ_context_t* ctx,
  const occ_shape_t* shape,
  occ_shape_type_t type,
  occ_iterator_t** out_iterator
);

OCC_C_API occ_status_t OCC_C_CALL occ_iterator_next_shape(
  occ_iterator_t* iterator,
  occ_shape_t** out_shape,
  int32_t* out_has_value
);

OCC_C_API occ_status_t OCC_C_CALL occ_vertex_point(
  const occ_shape_t* vertex,
  occ_vec3_t* out_point
);

OCC_C_API occ_status_t OCC_C_CALL occ_edge_curve3d(
  occ_context_t* ctx,
  const occ_shape_t* edge,
  occ_curve3d_t** out_curve,
  double* out_first,
  double* out_last
);

OCC_C_API occ_status_t OCC_C_CALL occ_face_surface(
  occ_context_t* ctx,
  const occ_shape_t* face,
  occ_surface_t** out_surface
);

OCC_C_API occ_status_t OCC_C_CALL occ_face_uv_bounds(
  occ_context_t* ctx,
  const occ_shape_t* face,
  double* out_umin,
  double* out_umax,
  double* out_vmin,
  double* out_vmax
);

/* -------------------------------------------------------------------------- */
/* Shape builders                                                             */
/* -------------------------------------------------------------------------- */

OCC_C_API occ_status_t OCC_C_CALL occ_make_vertex(
  occ_context_t* ctx,
  occ_vec3_t point,
  occ_shape_t** out_vertex
);

OCC_C_API occ_status_t OCC_C_CALL occ_make_edge_from_points(
  occ_context_t* ctx,
  occ_vec3_t p0,
  occ_vec3_t p1,
  occ_shape_t** out_edge
);

OCC_C_API occ_status_t OCC_C_CALL occ_make_edge_from_curve3d(
  occ_context_t* ctx,
  const occ_curve3d_t* curve,
  double first,
  double last,
  occ_shape_t** out_edge
);

OCC_C_API occ_status_t OCC_C_CALL occ_make_wire_from_edges(
  occ_context_t* ctx,
  const occ_shape_t* const* edges,
  size_t edge_count,
  occ_shape_t** out_wire
);

OCC_C_API occ_status_t OCC_C_CALL occ_make_face_from_wire(
  occ_context_t* ctx,
  const occ_shape_t* wire,
  int32_t planar_only,
  occ_shape_t** out_face
);

OCC_C_API occ_status_t OCC_C_CALL occ_make_face_from_surface(
  occ_context_t* ctx,
  const occ_surface_t* surface,
  double umin,
  double umax,
  double vmin,
  double vmax,
  occ_shape_t** out_face
);

OCC_C_API occ_status_t OCC_C_CALL occ_make_shell_from_faces(
  occ_context_t* ctx,
  const occ_shape_t* const* faces,
  size_t face_count,
  occ_shape_t** out_shell
);

OCC_C_API occ_status_t OCC_C_CALL occ_make_solid_from_shell(
  occ_context_t* ctx,
  const occ_shape_t* shell,
  occ_shape_t** out_solid
);

/* -------------------------------------------------------------------------- */
/* Topological algorithms                                                     */
/* -------------------------------------------------------------------------- */

typedef struct occ_mass_props_t {
  double mass;
  occ_vec3_t center_of_mass;
  double inertia[9]; /* row-major 3x3 */
} occ_mass_props_t;

typedef enum occ_shape_validity_t {
  OCC_SHAPE_VALID = 0,
  OCC_SHAPE_INVALID = 1,
  OCC_SHAPE_VALIDITY_UNKNOWN = 2
} occ_shape_validity_t;

OCC_C_API occ_status_t OCC_C_CALL occ_shape_check(
  occ_context_t* ctx,
  const occ_shape_t* shape,
  occ_shape_validity_t* out_validity
);

OCC_C_API occ_status_t OCC_C_CALL occ_shape_fix_basic(
  occ_context_t* ctx,
  const occ_shape_t* shape,
  occ_shape_t** out_fixed
);

OCC_C_API occ_status_t OCC_C_CALL occ_shape_linear_properties(
  occ_context_t* ctx,
  const occ_shape_t* shape,
  occ_mass_props_t* out_props
);

OCC_C_API occ_status_t OCC_C_CALL occ_shape_surface_properties(
  occ_context_t* ctx,
  const occ_shape_t* shape,
  occ_mass_props_t* out_props
);

OCC_C_API occ_status_t OCC_C_CALL occ_shape_volume_properties(
  occ_context_t* ctx,
  const occ_shape_t* shape,
  occ_mass_props_t* out_props
);

OCC_C_API occ_status_t OCC_C_CALL occ_distance_shape_shape(
  occ_context_t* ctx,
  const occ_shape_t* a,
  const occ_shape_t* b,
  double* out_distance,
  occ_vec3_t* out_point_a,
  occ_vec3_t* out_point_b
);

/* -------------------------------------------------------------------------- */
/* Primitives                                                                 */
/* -------------------------------------------------------------------------- */

OCC_C_API occ_status_t OCC_C_CALL occ_make_box(
  occ_context_t* ctx,
  double dx,
  double dy,
  double dz,
  occ_shape_t** out_shape
);

OCC_C_API occ_status_t OCC_C_CALL occ_make_box_from_corners(
  occ_context_t* ctx,
  occ_vec3_t min_corner,
  occ_vec3_t max_corner,
  occ_shape_t** out_shape
);

OCC_C_API occ_status_t OCC_C_CALL occ_make_sphere(
  occ_context_t* ctx,
  occ_vec3_t center,
  double radius,
  occ_shape_t** out_shape
);

OCC_C_API occ_status_t OCC_C_CALL occ_make_cylinder(
  occ_context_t* ctx,
  occ_axis2_t axis,
  double radius,
  double height,
  occ_shape_t** out_shape
);

OCC_C_API occ_status_t OCC_C_CALL occ_make_cone(
  occ_context_t* ctx,
  occ_axis2_t axis,
  double radius1,
  double radius2,
  double height,
  occ_shape_t** out_shape
);

OCC_C_API occ_status_t OCC_C_CALL occ_make_torus(
  occ_context_t* ctx,
  occ_axis2_t axis,
  double major_radius,
  double minor_radius,
  occ_shape_t** out_shape
);

OCC_C_API occ_status_t OCC_C_CALL occ_make_prism(
  occ_context_t* ctx,
  const occ_shape_t* profile,
  occ_vec3_t direction,
  occ_shape_t** out_shape
);

OCC_C_API occ_status_t OCC_C_CALL occ_make_revolution(
  occ_context_t* ctx,
  const occ_shape_t* profile,
  occ_axis1_t axis,
  double angle_radians,
  occ_shape_t** out_shape
);

/* -------------------------------------------------------------------------- */
/* Boolean operations                                                         */
/* -------------------------------------------------------------------------- */

typedef enum occ_boolean_op_t {
  OCC_BOOL_FUSE = 0,
  OCC_BOOL_CUT = 1,
  OCC_BOOL_COMMON = 2,
  OCC_BOOL_SECTION = 3
} occ_boolean_op_t;

typedef struct occ_boolean_options_t {
  double fuzzy_value;
  int32_t run_parallel;
  int32_t non_destructive;
  int32_t glue;          /* 0 none, 1 shift, 2 full */
  int32_t check_inverted;
} occ_boolean_options_t;

OCC_C_API occ_boolean_options_t OCC_C_CALL occ_boolean_options_default(void);

OCC_C_API occ_status_t OCC_C_CALL occ_boolean_apply(
  occ_context_t* ctx,
  occ_boolean_op_t op,
  const occ_shape_t* a,
  const occ_shape_t* b,
  const occ_boolean_options_t* options,
  occ_shape_t** out_shape
);

OCC_C_API occ_status_t OCC_C_CALL occ_boolean_fuse_many(
  occ_context_t* ctx,
  const occ_shape_t* const* shapes,
  size_t shape_count,
  const occ_boolean_options_t* options,
  occ_shape_t** out_shape
);

OCC_C_API occ_status_t OCC_C_CALL occ_boolean_cut_many(
  occ_context_t* ctx,
  const occ_shape_t* base,
  const occ_shape_t* const* tools,
  size_t tool_count,
  const occ_boolean_options_t* options,
  occ_shape_t** out_shape
);

/* -------------------------------------------------------------------------- */
/* BRep serialization                                                         */
/* -------------------------------------------------------------------------- */

typedef enum occ_brep_format_t {
  OCC_BREP_ASCII = 0,
  OCC_BREP_BINARY = 1
} occ_brep_format_t;

OCC_C_API occ_status_t OCC_C_CALL occ_shape_read_brep_file(
  occ_context_t* ctx,
  const char* path_utf8,
  occ_shape_t** out_shape
);

OCC_C_API occ_status_t OCC_C_CALL occ_shape_write_brep_file(
  occ_context_t* ctx,
  const occ_shape_t* shape,
  const char* path_utf8,
  occ_brep_format_t format
);

OCC_C_API occ_status_t OCC_C_CALL occ_shape_read_brep_memory(
  occ_context_t* ctx,
  const void* data,
  size_t size,
  occ_shape_t** out_shape
);

OCC_C_API occ_status_t OCC_C_CALL occ_shape_write_brep_memory(
  occ_context_t* ctx,
  const occ_shape_t* shape,
  occ_brep_format_t format,
  void** out_data,
  size_t* out_size
);

/* -------------------------------------------------------------------------- */
/* Existing triangulation readback                                            */
/* -------------------------------------------------------------------------- */

typedef struct occ_triangle_t {
  uint32_t i0;
  uint32_t i1;
  uint32_t i2;
} occ_triangle_t;

typedef struct occ_triangle_mesh_view_t {
  const occ_vec3_t* points;
  size_t point_count;
  const occ_triangle_t* triangles;
  size_t triangle_count;
} occ_triangle_mesh_view_t;

OCC_C_API occ_status_t OCC_C_CALL occ_face_get_existing_triangulation(
  occ_context_t* ctx,
  const occ_shape_t* face,
  occ_mesh_t** out_mesh
);

OCC_C_API occ_status_t OCC_C_CALL occ_mesh_view(
  const occ_mesh_t* mesh,
  occ_triangle_mesh_view_t* out_view
);

/* -------------------------------------------------------------------------- */
/* Convenience macros                                                         */
/* -------------------------------------------------------------------------- */

#define OCC_C_RELEASE_NULL(object_ptr_)                \
  do {                                                 \
    if ((object_ptr_) != NULL) {                       \
      occ_object_release((occ_object_t*)(object_ptr_));\
      (object_ptr_) = NULL;                            \
    }                                                  \
  } while (0)

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* OCC_C_H */
