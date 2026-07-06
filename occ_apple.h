#ifndef OCC_APPLE_H_INCLUDED
#define OCC_APPLE_H_INCLUDED

#ifndef __OBJC__
#error occ_apple.h requires Objective-C
#endif

#include "occ_defs.h"

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <ModelIO/ModelIO.h>
#include <simd/simd.h>

#ifndef OCC_APPLE_API
#define OCC_APPLE_API OCC_C_API
#endif

#ifndef OCC_APPLE_CALL
#define OCC_APPLE_CALL OCC_C_CALL
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct occ_apple_mesh_t occ_apple_mesh_t;
typedef struct occ_apple_metal_mesh_t occ_apple_metal_mesh_t;

OCC_APPLE_API vector_double2 OCC_APPLE_CALL occ_apple_simd_vec2(occ_vec2_t value);
OCC_APPLE_API vector_double3 OCC_APPLE_CALL occ_apple_simd_vec3(occ_vec3_t value);
OCC_APPLE_API matrix_double4x4 OCC_APPLE_CALL occ_apple_simd_mat4(occ_mat4_t value);
OCC_APPLE_API occ_vec2_t OCC_APPLE_CALL occ_apple_occ_vec2(vector_double2 value);
OCC_APPLE_API occ_vec3_t OCC_APPLE_CALL occ_apple_occ_vec3(vector_double3 value);
OCC_APPLE_API occ_mat4_t OCC_APPLE_CALL occ_apple_occ_mat4(matrix_double4x4 value);

OCC_APPLE_API occ_status_t OCC_APPLE_CALL occ_apple_mesh_create(occ_mesh_view_t view, occ_apple_mesh_t** out_mesh);
OCC_APPLE_API void OCC_APPLE_CALL occ_apple_mesh_destroy(occ_apple_mesh_t* mesh);
OCC_APPLE_API occ_status_t OCC_APPLE_CALL occ_apple_mesh_view(const occ_apple_mesh_t* mesh, occ_mesh_view_t* out_view);
OCC_APPLE_API occ_status_t OCC_APPLE_CALL occ_apple_mesh_transform(occ_mesh_view_t view, matrix_double4x4 transform, occ_apple_mesh_t** out_mesh);
OCC_APPLE_API occ_status_t OCC_APPLE_CALL occ_apple_mesh_bbox(occ_mesh_view_t view, occ_bbox_t* out_bbox);

OCC_APPLE_API occ_status_t OCC_APPLE_CALL occ_apple_metal_mesh_create(id<MTLDevice> device, occ_mesh_view_t view, occ_apple_metal_mesh_t** out_mesh);
OCC_APPLE_API void OCC_APPLE_CALL occ_apple_metal_mesh_destroy(occ_apple_metal_mesh_t* mesh);
OCC_APPLE_API id<MTLBuffer> OCC_APPLE_CALL occ_apple_metal_mesh_positions(const occ_apple_metal_mesh_t* mesh);
OCC_APPLE_API id<MTLBuffer> OCC_APPLE_CALL occ_apple_metal_mesh_normals(const occ_apple_metal_mesh_t* mesh);
OCC_APPLE_API id<MTLBuffer> OCC_APPLE_CALL occ_apple_metal_mesh_uvs(const occ_apple_metal_mesh_t* mesh);
OCC_APPLE_API id<MTLBuffer> OCC_APPLE_CALL occ_apple_metal_mesh_indices(const occ_apple_metal_mesh_t* mesh);
OCC_APPLE_API NSUInteger OCC_APPLE_CALL occ_apple_metal_mesh_vertex_count(const occ_apple_metal_mesh_t* mesh);
OCC_APPLE_API NSUInteger OCC_APPLE_CALL occ_apple_metal_mesh_triangle_count(const occ_apple_metal_mesh_t* mesh);
OCC_APPLE_API MTLIndexType OCC_APPLE_CALL occ_apple_metal_mesh_index_type(const occ_apple_metal_mesh_t* mesh);

OCC_APPLE_API MDLMesh* OCC_APPLE_CALL occ_apple_modelio_mesh_create(id<MTLDevice> device, occ_mesh_view_t view);
OCC_APPLE_API MTKMesh* OCC_APPLE_CALL occ_apple_metalkit_mesh_create(id<MTLDevice> device, occ_mesh_view_t view);

OCC_APPLE_API occ_vec3_t OCC_APPLE_CALL occ_apple_line_eval(occ_vec3_t point, occ_vec3_t direction, double parameter);
OCC_APPLE_API occ_vec3_t OCC_APPLE_CALL occ_apple_circle_eval(occ_axis2_t frame, double radius, double parameter);
OCC_APPLE_API occ_vec3_t OCC_APPLE_CALL occ_apple_plane_eval(occ_axis2_t frame, double u, double v);
OCC_APPLE_API occ_vec3_t OCC_APPLE_CALL occ_apple_cylinder_eval(occ_axis2_t frame, double radius, double u, double v);
OCC_APPLE_API occ_vec3_t OCC_APPLE_CALL occ_apple_sphere_eval(occ_axis2_t frame, double radius, double u, double v);

OCC_APPLE_API occ_status_t OCC_APPLE_CALL occ_apple_project_point_line(occ_vec3_t point, occ_vec3_t line_point, occ_vec3_t line_direction, double* out_parameter, occ_vec3_t* out_nearest, double* out_distance);
OCC_APPLE_API occ_status_t OCC_APPLE_CALL occ_apple_project_point_circle(occ_vec3_t point, occ_axis2_t frame, double radius, double* out_parameter, occ_vec3_t* out_nearest, double* out_distance);
OCC_APPLE_API occ_status_t OCC_APPLE_CALL occ_apple_project_point_plane(occ_vec3_t point, occ_axis2_t frame, double* out_u, double* out_v, occ_vec3_t* out_nearest, double* out_distance);
OCC_APPLE_API occ_status_t OCC_APPLE_CALL occ_apple_project_point_cylinder(occ_vec3_t point, occ_axis2_t frame, double radius, double* out_u, double* out_v, occ_vec3_t* out_nearest, double* out_distance);
OCC_APPLE_API occ_status_t OCC_APPLE_CALL occ_apple_project_point_sphere(occ_vec3_t point, occ_axis2_t frame, double radius, double* out_u, double* out_v, occ_vec3_t* out_nearest, double* out_distance);

OCC_APPLE_API occ_status_t OCC_APPLE_CALL occ_apple_make_box_mesh(double dx, double dy, double dz, occ_apple_mesh_t** out_mesh);
OCC_APPLE_API occ_status_t OCC_APPLE_CALL occ_apple_make_box_between_mesh(occ_vec3_t min_corner, occ_vec3_t max_corner, occ_apple_mesh_t** out_mesh);
OCC_APPLE_API occ_status_t OCC_APPLE_CALL occ_apple_make_sphere_mesh(occ_vec3_t center, double radius, size_t segment_count, size_t ring_count, occ_apple_mesh_t** out_mesh);
OCC_APPLE_API occ_status_t OCC_APPLE_CALL occ_apple_make_cylinder_mesh(occ_axis2_t axis, double radius, double height, size_t segment_count, occ_apple_mesh_t** out_mesh);
OCC_APPLE_API occ_status_t OCC_APPLE_CALL occ_apple_make_cone_mesh(occ_axis2_t axis, double radius1, double radius2, double height, size_t segment_count, occ_apple_mesh_t** out_mesh);
OCC_APPLE_API occ_status_t OCC_APPLE_CALL occ_apple_make_torus_mesh(occ_axis2_t axis, double major_radius, double minor_radius, size_t major_segment_count, size_t minor_segment_count, occ_apple_mesh_t** out_mesh);

#ifdef __cplusplus
}
#endif

#endif
