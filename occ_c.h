#ifndef OCC_C_H_INCLUDED
#define OCC_C_H_INCLUDED

/*
  occ_c.h

  A small, self-contained C99 ABI for the CAD-kernel subset of Open CASCADE
  Technology (OCCT).

  Scope
  -----
  This API intentionally wraps only the parts of OCCT that are hard to replace
  with general-purpose libraries or native platform frameworks:

    - analytic 3D curves and surfaces used by B-Rep topology
    - TopoDS_Shape-style boundary representation handles
    - primitive solid construction
    - edge/wire/face/shell/solid construction
    - topology traversal and shape metadata
    - geometric queries, mass properties, tolerance queries
    - shape validation and basic shape healing
    - boolean operations
    - OCCT BREP file/memory serialization
    - optional OCCT tessellation of B-Rep shapes for rendering handoff

  Deliberately out of scope
  -------------------------
  Do not put the following into this ABI unless you create separate optional
  modules later:

    - rendering, scene graphs, cameras, materials, textures, GPU buffers
    - general vector/matrix/quaternion math libraries
    - UI interaction, selection widgets, native view integration
    - image I/O or texture loading
    - mesh asset pipelines such as glTF/OBJ/USDZ/STL display workflows
    - platform-specific frameworks such as Metal, SceneKit, RealityKit,
      Model I/O, CoreGraphics, or Accelerate

  Notes
  -----
  - C99-compatible public API.
  - No C++ types, exceptions, STL containers, RTTI, or OCCT layouts cross the ABI.
  - Opaque handle types are distinct incomplete C struct types.
  - All strings are UTF-8.
  - All angles are radians.
  - All distances use the active OCCT model units chosen by the caller.
  - Matrices are row-major 4x4 affine transforms.
  - Returned handles are owned by the caller and must be released with occ_release().
  - Returned memory buffers are owned by the caller and must be released with occ_free().
  - Views returned by this API are borrowed; they remain valid until the owning
    handle is released or mutated by another call.

  Suggested minimal OCCT linkage for a full implementation
  --------------------------------------------------------
  Core geometry/topology:
    TKernel, TKMath, TKG2d, TKG3d, TKGeomBase, TKBRep,
    TKGeomAlgo, TKTopAlgo, TKPrim

  Booleans:
    TKBO, TKBool

  Shape healing:
    TKShHealing

  Tessellation:
    TKMesh

  This header contains declarations only. Implement it in a C++ shared library
  that links against OCCT and catches all C++ exceptions at the ABI boundary.
*/

#include "occ_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------------- */
/* Status and versioning                                                     */
/* ------------------------------------------------------------------------- */

OCC_C_API occ_version_t OCC_C_CALL occ_version(void);

OCC_C_API const char * OCC_C_CALL occ_status_string(occ_status_t status);

/* ------------------------------------------------------------------------- */
/* Opaque handles                                                            */
/* ------------------------------------------------------------------------- */

typedef struct occ_context_t occ_context_t;
typedef struct occ_curve2d_t occ_curve2d_t;
typedef struct occ_curve3d_t occ_curve3d_t;
typedef struct occ_surface_t occ_surface_t;
typedef struct occ_shape_t occ_shape_t;
typedef struct occ_mesh_t occ_mesh_t;

typedef enum occ_handle_kind_t {
    OCC_HANDLE_UNKNOWN = 0,
    OCC_HANDLE_CURVE2D = 1,
    OCC_HANDLE_CURVE3D = 2,
    OCC_HANDLE_SURFACE = 3,
    OCC_HANDLE_SHAPE = 4,
    OCC_HANDLE_MESH = 5
} occ_handle_kind_t;

/*
  Releases any non-context handle returned by this API. Passing NULL is valid.
  Contexts are released with occ_context_destroy(), not occ_release().
*/
OCC_C_API void OCC_C_CALL occ_release(void *handle);

OCC_C_API void OCC_C_CALL occ_retain(void *handle);

OCC_C_API occ_handle_kind_t OCC_C_CALL occ_handle_kind(const void *handle);

OCC_C_API void OCC_C_CALL occ_free(void *memory);

/* ------------------------------------------------------------------------- */
/* Context, errors, and diagnostics                                          */
/* ------------------------------------------------------------------------- */

typedef enum occ_log_level_t {
    OCC_LOG_TRACE = 0,
    OCC_LOG_INFO = 1,
    OCC_LOG_WARNING = 2,
    OCC_LOG_ERROR = 3
} occ_log_level_t;

typedef void (OCC_C_CALL *occ_log_callback_t)(
    occ_log_level_t level,
    const char *message_utf8,
    void *user_data
);

OCC_C_API occ_status_t OCC_C_CALL occ_context_create(occ_context_t **out_context);

OCC_C_API void OCC_C_CALL occ_context_destroy(occ_context_t *context);

OCC_C_API occ_status_t OCC_C_CALL occ_context_set_log_callback(
    occ_context_t *context,
    occ_log_callback_t callback,
    void *user_data
);

/*
  The returned string is borrowed and owned by the context. It remains valid
  until the next call using the same context or until the context is destroyed.
*/
OCC_C_API occ_status_t OCC_C_CALL occ_context_last_error(
    const occ_context_t *context,
    const char **out_message_utf8
);

OCC_C_API occ_status_t OCC_C_CALL occ_context_last_diagnostic_count(
    const occ_context_t *context,
    size_t *out_count
);

OCC_C_API occ_status_t OCC_C_CALL occ_context_last_diagnostic_at(
    const occ_context_t *context,
    size_t index,
    occ_log_level_t *out_level,
    const char **out_message_utf8
);

/* ------------------------------------------------------------------------- */
/* Geometry                                                                  */
/* ------------------------------------------------------------------------- */

typedef enum occ_curve_type_t {
    OCC_CURVE_UNKNOWN = 0,
    OCC_CURVE_LINE = 1,
    OCC_CURVE_CIRCLE = 2,
    OCC_CURVE_ELLIPSE = 3,
    OCC_CURVE_BEZIER = 4,
    OCC_CURVE_BSPLINE = 5,
    OCC_CURVE_OFFSET = 6,
    OCC_CURVE_OTHER = 255
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
    OCC_SURFACE_OFFSET = 10,
    OCC_SURFACE_OTHER = 255
} occ_surface_type_t;

OCC_C_API occ_status_t OCC_C_CALL occ_curve3d_line(
    occ_context_t *context,
    occ_vec3_t point,
    occ_vec3_t direction,
    occ_curve3d_t **out_curve
);

OCC_C_API occ_status_t OCC_C_CALL occ_curve3d_circle(
    occ_context_t *context,
    occ_axis2_t frame,
    double radius,
    occ_curve3d_t **out_curve
);

OCC_C_API occ_status_t OCC_C_CALL occ_curve3d_type(
    const occ_curve3d_t *curve,
    occ_curve_type_t *out_type
);

OCC_C_API occ_status_t OCC_C_CALL occ_curve3d_bounds(
    const occ_curve3d_t *curve,
    double *out_first,
    double *out_last
);

OCC_C_API occ_status_t OCC_C_CALL occ_curve3d_eval(
    const occ_curve3d_t *curve,
    double parameter,
    occ_vec3_t *out_point
);

OCC_C_API occ_status_t OCC_C_CALL occ_curve3d_eval_d1(
    const occ_curve3d_t *curve,
    double parameter,
    occ_vec3_t *out_point,
    occ_vec3_t *out_derivative
);

OCC_C_API occ_status_t OCC_C_CALL occ_surface_plane(
    occ_context_t *context,
    occ_axis2_t frame,
    occ_surface_t **out_surface
);

OCC_C_API occ_status_t OCC_C_CALL occ_surface_cylinder(
    occ_context_t *context,
    occ_axis2_t frame,
    double radius,
    occ_surface_t **out_surface
);

OCC_C_API occ_status_t OCC_C_CALL occ_surface_sphere(
    occ_context_t *context,
    occ_axis2_t frame,
    double radius,
    occ_surface_t **out_surface
);

OCC_C_API occ_status_t OCC_C_CALL occ_surface_type(
    const occ_surface_t *surface,
    occ_surface_type_t *out_type
);

OCC_C_API occ_status_t OCC_C_CALL occ_surface_bounds(
    const occ_surface_t *surface,
    double *out_umin,
    double *out_umax,
    double *out_vmin,
    double *out_vmax
);

OCC_C_API occ_status_t OCC_C_CALL occ_surface_eval(
    const occ_surface_t *surface,
    double u,
    double v,
    occ_vec3_t *out_point
);

OCC_C_API occ_status_t OCC_C_CALL occ_surface_eval_d1(
    const occ_surface_t *surface,
    double u,
    double v,
    occ_vec3_t *out_point,
    occ_vec3_t *out_du,
    occ_vec3_t *out_dv
);

OCC_C_API occ_status_t OCC_C_CALL occ_project_point_curve3d(
    occ_context_t *context,
    occ_vec3_t point,
    const occ_curve3d_t *curve,
    double *out_parameter,
    occ_vec3_t *out_nearest,
    double *out_distance
);

OCC_C_API occ_status_t OCC_C_CALL occ_project_point_surface(
    occ_context_t *context,
    occ_vec3_t point,
    const occ_surface_t *surface,
    double *out_u,
    double *out_v,
    occ_vec3_t *out_nearest,
    double *out_distance
);

/* ------------------------------------------------------------------------- */
/* Shapes and topology                                                       */
/* ------------------------------------------------------------------------- */

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
    OCC_FORWARD = 0,
    OCC_REVERSED = 1,
    OCC_INTERNAL = 2,
    OCC_EXTERNAL = 3
} occ_orientation_t;

typedef enum occ_traversal_t {
    OCC_TRAVERSE_DIRECT = 0,
    OCC_TRAVERSE_RECURSIVE = 1,
    OCC_TRAVERSE_RECURSIVE_UNIQUE = 2
} occ_traversal_t;

typedef occ_status_t (OCC_C_CALL *occ_shape_visit_fn)(
    const occ_shape_t *shape,
    occ_shape_type_t type,
    void *user_data
);

OCC_C_API occ_status_t OCC_C_CALL occ_shape_type(
    const occ_shape_t *shape,
    occ_shape_type_t *out_type
);

OCC_C_API occ_status_t OCC_C_CALL occ_shape_is_null(
    const occ_shape_t *shape,
    int32_t *out_is_null
);

OCC_C_API occ_status_t OCC_C_CALL occ_shape_orientation(
    const occ_shape_t *shape,
    occ_orientation_t *out_orientation
);

OCC_C_API occ_status_t OCC_C_CALL occ_shape_location(
    const occ_shape_t *shape,
    occ_mat4_t *out_location
);

OCC_C_API occ_status_t OCC_C_CALL occ_shape_set_location(
    occ_context_t *context,
    const occ_shape_t *shape,
    occ_mat4_t location,
    occ_shape_t **out_shape
);

OCC_C_API occ_status_t OCC_C_CALL occ_shape_reversed(
    occ_context_t *context,
    const occ_shape_t *shape,
    occ_shape_t **out_shape
);

OCC_C_API occ_status_t OCC_C_CALL occ_shape_transformed(
    occ_context_t *context,
    const occ_shape_t *shape,
    occ_mat4_t transform,
    int32_t copy_geometry,
    occ_shape_t **out_shape
);

OCC_C_API occ_status_t OCC_C_CALL occ_shape_bbox(
    occ_context_t *context,
    const occ_shape_t *shape,
    int32_t use_triangulation,
    occ_bbox_t *out_bbox
);

OCC_C_API occ_status_t OCC_C_CALL occ_shape_tolerance(
    const occ_shape_t *shape,
    double *out_tolerance
);

OCC_C_API occ_status_t OCC_C_CALL occ_shape_count_subshapes(
    occ_context_t *context,
    const occ_shape_t *shape,
    occ_shape_type_t type,
    occ_traversal_t traversal,
    size_t *out_count
);

OCC_C_API occ_status_t OCC_C_CALL occ_shape_subshape_at(
    occ_context_t *context,
    const occ_shape_t *shape,
    occ_shape_type_t type,
    occ_traversal_t traversal,
    size_t index,
    occ_shape_t **out_subshape
);

OCC_C_API occ_status_t OCC_C_CALL occ_shape_visit_subshapes(
    occ_context_t *context,
    const occ_shape_t *shape,
    occ_shape_type_t type,
    occ_traversal_t traversal,
    occ_shape_visit_fn visitor,
    void *user_data
);

OCC_C_API occ_status_t OCC_C_CALL occ_vertex_point(
    const occ_shape_t *vertex,
    occ_vec3_t *out_point
);

OCC_C_API occ_status_t OCC_C_CALL occ_edge_curve3d(
    occ_context_t *context,
    const occ_shape_t *edge,
    occ_curve3d_t **out_curve,
    double *out_first,
    double *out_last
);

OCC_C_API occ_status_t OCC_C_CALL occ_edge_curve2d_on_face(
    occ_context_t *context,
    const occ_shape_t *edge,
    const occ_shape_t *face,
    occ_curve2d_t **out_curve,
    double *out_first,
    double *out_last
);

OCC_C_API occ_status_t OCC_C_CALL occ_curve2d_type(
    const occ_curve2d_t *curve,
    occ_curve_type_t *out_type
);

OCC_C_API occ_status_t OCC_C_CALL occ_curve2d_eval(
    const occ_curve2d_t *curve,
    double parameter,
    occ_vec2_t *out_point
);

OCC_C_API occ_status_t OCC_C_CALL occ_curve2d_eval_d1(
    const occ_curve2d_t *curve,
    double parameter,
    occ_vec2_t *out_point,
    occ_vec2_t *out_derivative
);

OCC_C_API occ_status_t OCC_C_CALL occ_face_surface(
    occ_context_t *context,
    const occ_shape_t *face,
    occ_surface_t **out_surface
);

OCC_C_API occ_status_t OCC_C_CALL occ_face_uv_bounds(
    occ_context_t *context,
    const occ_shape_t *face,
    double *out_umin,
    double *out_umax,
    double *out_vmin,
    double *out_vmax
);

/* ------------------------------------------------------------------------- */
/* Shape construction                                                        */
/* ------------------------------------------------------------------------- */

OCC_C_API occ_status_t OCC_C_CALL occ_make_vertex(
    occ_context_t *context,
    occ_vec3_t point,
    occ_shape_t **out_vertex
);

OCC_C_API occ_status_t OCC_C_CALL occ_make_edge_points(
    occ_context_t *context,
    occ_vec3_t start,
    occ_vec3_t end,
    occ_shape_t **out_edge
);

OCC_C_API occ_status_t OCC_C_CALL occ_make_edge_curve3d(
    occ_context_t *context,
    const occ_curve3d_t *curve,
    double first,
    double last,
    occ_shape_t **out_edge
);

OCC_C_API occ_status_t OCC_C_CALL occ_make_wire(
    occ_context_t *context,
    const occ_shape_t *const*edges,
    size_t edge_count,
    occ_shape_t **out_wire
);

OCC_C_API occ_status_t OCC_C_CALL occ_make_face_wire(
    occ_context_t *context,
    const occ_shape_t *wire,
    int32_t force_planar,
    occ_shape_t **out_face
);

OCC_C_API occ_status_t OCC_C_CALL occ_make_face_surface_bounds(
    occ_context_t *context,
    const occ_surface_t *surface,
    double umin,
    double umax,
    double vmin,
    double vmax,
    double tolerance,
    occ_shape_t **out_face
);

OCC_C_API occ_status_t OCC_C_CALL occ_make_shell(
    occ_context_t *context,
    const occ_shape_t *const*faces,
    size_t face_count,
    occ_shape_t **out_shell
);

OCC_C_API occ_status_t OCC_C_CALL occ_make_solid(
    occ_context_t *context,
    const occ_shape_t *shell,
    occ_shape_t **out_solid
);

OCC_C_API occ_status_t OCC_C_CALL occ_make_compound(
    occ_context_t *context,
    const occ_shape_t *const*shapes,
    size_t shape_count,
    occ_shape_t **out_compound
);

/* ------------------------------------------------------------------------- */
/* Primitive solids and sweep operations                                     */
/* ------------------------------------------------------------------------- */

OCC_C_API occ_status_t OCC_C_CALL occ_make_box(
    occ_context_t *context,
    double dx,
    double dy,
    double dz,
    occ_shape_t **out_shape
);

OCC_C_API occ_status_t OCC_C_CALL occ_make_box_between(
    occ_context_t *context,
    occ_vec3_t min_corner,
    occ_vec3_t max_corner,
    occ_shape_t **out_shape
);

OCC_C_API occ_status_t OCC_C_CALL occ_make_sphere(
    occ_context_t *context,
    occ_vec3_t center,
    double radius,
    occ_shape_t **out_shape
);

OCC_C_API occ_status_t OCC_C_CALL occ_make_cylinder(
    occ_context_t *context,
    occ_axis2_t axis,
    double radius,
    double height,
    occ_shape_t **out_shape
);

OCC_C_API occ_status_t OCC_C_CALL occ_make_cone(
    occ_context_t *context,
    occ_axis2_t axis,
    double radius1,
    double radius2,
    double height,
    occ_shape_t **out_shape
);

OCC_C_API occ_status_t OCC_C_CALL occ_make_torus(
    occ_context_t *context,
    occ_axis2_t axis,
    double major_radius,
    double minor_radius,
    occ_shape_t **out_shape
);

OCC_C_API occ_status_t OCC_C_CALL occ_make_prism(
    occ_context_t *context,
    const occ_shape_t *profile,
    occ_vec3_t vector,
    int32_t copy_profile,
    occ_shape_t **out_shape
);

OCC_C_API occ_status_t OCC_C_CALL occ_make_revolution(
    occ_context_t *context,
    const occ_shape_t *profile,
    occ_axis1_t axis,
    double angle_radians,
    int32_t copy_profile,
    occ_shape_t **out_shape
);

/* ------------------------------------------------------------------------- */
/* Shape algorithms                                                          */
/* ------------------------------------------------------------------------- */

typedef enum occ_shape_validity_t {
    OCC_SHAPE_VALID = 0,
    OCC_SHAPE_INVALID = 1,
    OCC_SHAPE_VALIDITY_UNKNOWN = 2
} occ_shape_validity_t;

OCC_C_API occ_status_t OCC_C_CALL occ_shape_check(
    occ_context_t *context,
    const occ_shape_t *shape,
    occ_shape_validity_t *out_validity
);

OCC_C_API occ_status_t OCC_C_CALL occ_shape_fix_basic(
    occ_context_t *context,
    const occ_shape_t *shape,
    occ_shape_t **out_fixed_shape
);

OCC_C_API occ_status_t OCC_C_CALL occ_shape_linear_properties(
    occ_context_t *context,
    const occ_shape_t *shape,
    occ_mass_properties_t *out_properties
);

OCC_C_API occ_status_t OCC_C_CALL occ_shape_surface_properties(
    occ_context_t *context,
    const occ_shape_t *shape,
    occ_mass_properties_t *out_properties
);

OCC_C_API occ_status_t OCC_C_CALL occ_shape_volume_properties(
    occ_context_t *context,
    const occ_shape_t *shape,
    occ_mass_properties_t *out_properties
);

OCC_C_API occ_status_t OCC_C_CALL occ_shape_distance(
    occ_context_t *context,
    const occ_shape_t *a,
    const occ_shape_t *b,
    double *out_distance,
    occ_vec3_t *out_nearest_on_a,
    occ_vec3_t *out_nearest_on_b
);

/* ------------------------------------------------------------------------- */
/* Boolean operations                                                        */
/* ------------------------------------------------------------------------- */

typedef enum occ_boolean_operation_t {
    OCC_BOOLEAN_FUSE = 0,
    OCC_BOOLEAN_CUT = 1,
    OCC_BOOLEAN_COMMON = 2,
    OCC_BOOLEAN_SECTION = 3
} occ_boolean_operation_t;

typedef enum occ_boolean_glue_t {
    OCC_BOOLEAN_GLUE_OFF = 0,
    OCC_BOOLEAN_GLUE_SHIFT = 1,
    OCC_BOOLEAN_GLUE_FULL = 2
} occ_boolean_glue_t;

typedef struct occ_boolean_options_t {
    double fuzzy_value;
    int32_t run_parallel;
    int32_t non_destructive;
    occ_boolean_glue_t glue;
    int32_t check_inverted;
} occ_boolean_options_t;

OCC_C_API occ_boolean_options_t OCC_C_CALL occ_boolean_options_default(void);

OCC_C_API occ_status_t OCC_C_CALL occ_boolean_apply(
    occ_context_t *context,
    occ_boolean_operation_t operation,
    const occ_shape_t *a,
    const occ_shape_t *b,
    const occ_boolean_options_t *options_or_null,
    occ_shape_t **out_shape
);

OCC_C_API occ_status_t OCC_C_CALL occ_boolean_fuse_many(
    occ_context_t *context,
    const occ_shape_t *const*shapes,
    size_t shape_count,
    const occ_boolean_options_t *options_or_null,
    occ_shape_t **out_shape
);

OCC_C_API occ_status_t OCC_C_CALL occ_boolean_cut_many(
    occ_context_t *context,
    const occ_shape_t *base,
    const occ_shape_t *const*tools,
    size_t tool_count,
    const occ_boolean_options_t *options_or_null,
    occ_shape_t **out_shape
);

/* ------------------------------------------------------------------------- */
/* OCCT BREP serialization                                                   */
/* ------------------------------------------------------------------------- */

typedef enum occ_brep_format_t {
    OCC_BREP_ASCII = 0,
    OCC_BREP_BINARY = 1
} occ_brep_format_t;

OCC_C_API occ_status_t OCC_C_CALL occ_shape_read_brep_file(
    occ_context_t *context,
    const char *path_utf8,
    occ_shape_t **out_shape
);

OCC_C_API occ_status_t OCC_C_CALL occ_shape_write_brep_file(
    occ_context_t *context,
    const occ_shape_t *shape,
    const char *path_utf8,
    occ_brep_format_t format
);

OCC_C_API occ_status_t OCC_C_CALL occ_shape_read_brep_memory(
    occ_context_t *context,
    const void *data,
    size_t size,
    occ_shape_t **out_shape
);

OCC_C_API occ_status_t OCC_C_CALL occ_shape_write_brep_memory(
    occ_context_t *context,
    const occ_shape_t *shape,
    occ_brep_format_t format,
    void **out_data,
    size_t *out_size
);

/* ------------------------------------------------------------------------- */
/* Tessellation for rendering handoff                                        */
/* ------------------------------------------------------------------------- */

/*
  This is the only mesh-related feature in the core ABI because it is the
  bridge between OCCT B-Rep geometry and native rendering/asset systems.
  The ABI does not attempt to become a general mesh-processing library.
*/

typedef struct occ_mesh_options_t {
    double linear_deflection;
    double angular_deflection;
    int32_t relative_deflection;
    int32_t run_parallel;
} occ_mesh_options_t;

OCC_C_API occ_mesh_options_t OCC_C_CALL occ_mesh_options_default(void);

OCC_C_API occ_status_t OCC_C_CALL occ_shape_triangulate(
    occ_context_t *context,
    const occ_shape_t *shape,
    const occ_mesh_options_t *options_or_null,
    occ_shape_t **out_shape_with_triangulation
);

OCC_C_API occ_status_t OCC_C_CALL occ_face_triangulation(
    occ_context_t *context,
    const occ_shape_t *face,
    int32_t apply_face_location,
    occ_mesh_t **out_mesh
);

OCC_C_API occ_status_t OCC_C_CALL occ_mesh_view(
    const occ_mesh_t *mesh,
    occ_mesh_view_t *out_view
);

OCC_C_API occ_mesh_packed_options_t OCC_C_CALL occ_mesh_packed_options_default(void);

OCC_C_API occ_status_t OCC_C_CALL occ_mesh_packed_layout(
    occ_mesh_view_t view,
    const occ_mesh_packed_options_t *options_or_null,
    occ_mesh_packed_layout_t *out_layout
);

OCC_C_API occ_status_t OCC_C_CALL occ_mesh_write_packed(
    occ_mesh_view_t view,
    const occ_mesh_packed_options_t *options_or_null,
    void *vertex_dst,
    size_t vertex_dst_size,
    void *index_dst,
    size_t index_dst_size,
    occ_mesh_packed_layout_t *out_layout
);

/* ------------------------------------------------------------------------- */
/* Convenience                                                               */
/* ------------------------------------------------------------------------- */

#define OCC_RELEASE_NULL(handle_)       \
  do {                                  \
    if ((handle_) != NULL) {            \
      occ_release((void*)(handle_));    \
      (handle_) = NULL;                 \
    }                                   \
  } while (0)

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* OCC_C_H_INCLUDED */
