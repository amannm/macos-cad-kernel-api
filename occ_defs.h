#ifndef OCC_DEFS_H_INCLUDED
#define OCC_DEFS_H_INCLUDED

#include <stddef.h>
#include <stdint.h>

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

#define OCC_C_ABI_VERSION_MAJOR 1u
#define OCC_C_ABI_VERSION_MINOR 1u
#define OCC_C_ABI_VERSION_PATCH 0u

#ifndef OCC_C_FALSE
#define OCC_C_FALSE 0
#endif

#ifndef OCC_C_TRUE
#define OCC_C_TRUE 1
#endif

typedef enum occ_status_t {
    OCC_OK = 0,
    OCC_ERROR = 1,
    OCC_NULL_ARGUMENT = 2,
    OCC_INVALID_ARGUMENT = 3,
    OCC_OUT_OF_MEMORY = 4,
    OCC_BAD_HANDLE = 5,
    OCC_BAD_HANDLE_TYPE = 6,
    OCC_INDEX_OUT_OF_RANGE = 7,
    OCC_EMPTY_RESULT = 8,
    OCC_INVALID_SHAPE = 9,
    OCC_NON_MANIFOLD = 10,
    OCC_IO_ERROR = 11,
    OCC_PARSE_ERROR = 12,
    OCC_UNSUPPORTED = 13,
    OCC_OCCT_EXCEPTION = 14
} occ_status_t;

typedef struct occ_version_t {
    uint32_t abi_major;
    uint32_t abi_minor;
    uint32_t abi_patch;
    uint32_t occt_major;
    uint32_t occt_minor;
    uint32_t occt_patch;
} occ_version_t;

typedef struct occ_vec2_t {
    double x;
    double y;
} occ_vec2_t;

typedef struct occ_vec3_t {
    double x;
    double y;
    double z;
} occ_vec3_t;

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
    double m[16];
} occ_mat4_t;

typedef struct occ_bbox_t {
    double xmin;
    double ymin;
    double zmin;
    double xmax;
    double ymax;
    double zmax;
    int32_t is_empty;
} occ_bbox_t;

typedef struct occ_mass_properties_t {
    double measure;
    occ_vec3_t center_of_mass;
    double inertia[9];
} occ_mass_properties_t;

typedef struct occ_triangle_t {
    uint32_t i0;
    uint32_t i1;
    uint32_t i2;
} occ_triangle_t;

typedef struct occ_mesh_view_t {
    const occ_vec3_t *positions;
    const occ_vec3_t *normals;
    const occ_vec2_t *uvs;
    size_t vertex_count;
    const occ_triangle_t *triangles;
    size_t triangle_count;
} occ_mesh_view_t;

typedef enum occ_mesh_vertex_format_t {
    OCC_MESH_VERTEX_FLOAT32_P = 1,
    OCC_MESH_VERTEX_FLOAT32_PN = 2,
    OCC_MESH_VERTEX_FLOAT32_PNT = 3
} occ_mesh_vertex_format_t;

typedef enum occ_mesh_index_format_t {
    OCC_MESH_INDEX_UINT16 = 1,
    OCC_MESH_INDEX_UINT32 = 2
} occ_mesh_index_format_t;

typedef struct occ_mesh_vertex_float32_p_t {
    float px;
    float py;
    float pz;
} occ_mesh_vertex_float32_p_t;

typedef struct occ_mesh_vertex_float32_pn_t {
    float px;
    float py;
    float pz;
    float nx;
    float ny;
    float nz;
} occ_mesh_vertex_float32_pn_t;

typedef struct occ_mesh_vertex_float32_pnt_t {
    float px;
    float py;
    float pz;
    float nx;
    float ny;
    float nz;
    float tx;
    float ty;
} occ_mesh_vertex_float32_pnt_t;

typedef struct occ_mesh_packed_options_t {
    occ_mesh_vertex_format_t vertex_format;
    occ_mesh_index_format_t index_format;
} occ_mesh_packed_options_t;

typedef struct occ_mesh_packed_layout_t {
    occ_mesh_vertex_format_t vertex_format;
    occ_mesh_index_format_t index_format;
    size_t vertex_stride;
    size_t vertex_count;
    size_t vertex_bytes;
    size_t index_stride;
    size_t index_count;
    size_t index_bytes;
} occ_mesh_packed_layout_t;

#endif
