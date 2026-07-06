I inspected the uploaded occ_c(2).h. The main answer is: the ABI is mostly wrapping OCCT’s modeling kernel, not things Metal/Apple frameworks replace well. The parts that Apple frameworks can easily handle are mostly math, rendering handoff, mesh display/storage, simple analytic evaluation, and app-side object bookkeeping. The actual B-Rep topology, healing, booleans, BREP persistence, and robust CAD queries should stay OCCT-backed.

OCCT itself is a CAD/CAM/CAE C++ library for 2D/3D geometric modeling, and its B-Rep model stores topology bound to mathematical curves/surfaces. That is exactly the hard part Apple’s rendering frameworks do not provide.

Good candidates for Metal / Apple implementation

ABI area	Functions / types	Apple replacement	How easy?	Notes
Plain math types	occ_vec2_t, occ_vec3_t, occ_mat4_t, occ_axis1_t, occ_axis2_t, bbox math	simd, maybe Accelerate	Easy	Use Apple simd for vector/matrix operations; Accelerate for heavier dense linear algebra. Apple’s SIMD docs explicitly cover small vector/matrix computations, and Accelerate provides BLAS/LAPACK-style linear algebra.    
Transforming already-triangulated geometry	occ_shape_location, occ_shape_set_location, occ_shape_transformed only for mesh instances	simd, Metal vertex shader, RealityKit transforms	Easy	Fine for display transforms. Not equivalent to transforming OCCT B-Rep geometry/topology unless OCCT remains source of truth.
Mesh view and rendering handoff	occ_mesh_t, occ_mesh_view_t, occ_mesh_view, output of occ_shape_triangulate / occ_face_triangulation	Metal buffers, MetalKit, RealityKit ModelComponent, Model I/O meshes	Easy	The ABI’s mesh output is ideal for Apple-side rendering. Metal gives direct GPU access for rendering/compute; RealityKit renders meshes/materials as model components; Model I/O imports/exports/manages 3D assets.      
Bounding boxes of meshes	occ_shape_bbox when use_triangulation = true	CPU SIMD, Metal reduction kernel, Model I/O bounding boxes	Easy	Fast and simple for triangulated data. Exact CAD bbox of analytic B-Rep is still better done in OCCT.
Analytic curve/surface evaluation for simple cases	occ_curve3d_line, occ_curve3d_circle, occ_curve3d_eval, occ_surface_plane, occ_surface_cylinder, occ_surface_sphere, occ_surface_eval	Swift/C++ math, simd; Metal compute for bulk sampling	Easy to medium	Lines, circles, planes, cylinders, spheres are straightforward formulas. Worth doing Apple-side only for bulk sampling/rendering; still keep OCCT objects for topology.
Projection onto simple analytic geometry	occ_project_point_curve3d, occ_project_point_surface for line/circle/plane/sphere/cylinder	CPU SIMD or Metal compute	Medium	Easy for plane/line/sphere; more subtle for periodic surfaces, trimming, tolerances, and closest-point edge cases.
Primitive mesh previews	visual versions of occ_make_box, occ_make_sphere, occ_make_cylinder, occ_make_cone, occ_make_torus	RealityKit mesh primitives, Model I/O, custom Metal mesh generation	Easy	Good for preview/placeholder meshes. Not a replacement for OCCT solids, because a CAD “box” is not just triangles; it is topological faces/edges/vertices with analytic surfaces.
Status/context/logging	occ_status_t, occ_context_*, occ_release, occ_retain, occ_free	Native Swift/C/Obj-C infrastructure	Easy	These are ABI plumbing, not OCCT-specific algorithms.

Not good candidates for Apple frameworks

These should remain OCCT-backed unless you want to write a CAD kernel.

ABI area	Functions	Why not Apple/Metal?
B-Rep topology	occ_shape_type, occ_shape_count_subshapes, occ_shape_subshape_at, occ_shape_visit_subshapes, occ_edge_curve3d, occ_edge_curve2d_on_face, occ_face_surface, occ_face_uv_bounds	Apple frameworks generally deal in meshes/scenes/assets, not CAD topology. OCCT’s B-Rep links topology to exact analytic/NURBS geometry.  
Shape construction as CAD topology	occ_make_vertex, occ_make_edge_points, occ_make_edge_curve3d, occ_make_wire, occ_make_face_wire, occ_make_face_surface_bounds, occ_make_shell, occ_make_solid, occ_make_compound	Making a render mesh is easy; making valid toleranced CAD topology is not.
True primitive solids and sweeps	occ_make_box, occ_make_sphere, occ_make_cylinder, occ_make_cone, occ_make_torus, occ_make_prism, occ_make_revolution	Apple can make triangle meshes, but OCCT makes editable B-Rep solids with analytic faces and edges.
Validation and healing	occ_shape_check, occ_shape_fix_basic	Shape healing is CAD-kernel logic. OCCT has a dedicated Shape Healing module in its user guides.  
Exact mass properties	occ_shape_linear_properties, occ_shape_surface_properties, occ_shape_volume_properties	Mesh approximations are possible, but OCCT’s BRepGProp computes global properties over edges/faces/volumes of shapes.  
Shape-to-shape distance	occ_shape_distance	AABB/mesh distance can be Metal-accelerated, but exact or tolerance-aware B-Rep distance is kernel work. OCCT Modeling Data includes extrema/distance services.  
Boolean operations	occ_boolean_apply, occ_boolean_fuse_many, occ_boolean_cut_many	Metal can accelerate mesh booleans in theory, but robust CAD booleans on B-Reps are exactly OCCT territory. OCCT’s Boolean Operations component includes general fuse, boolean, section, and splitter operators.  
OCCT BREP serialization	occ_shape_read_brep_file, occ_shape_write_brep_file, occ_shape_read_brep_memory, occ_shape_write_brep_memory	Apple frameworks handle formats like USD/USDZ/mesh assets; OCCT BREP is OCCT’s native topological/geometry persistence.
Real B-Rep tessellation	occ_shape_triangulate, occ_face_triangulation	The resulting mesh belongs in Metal/RealityKit, but the tessellation from trimmed analytic B-Rep faces should stay OCCT. OCCT’s BRepMesh_IncrementalMesh is its default meshing path with linear/angular deflection controls.

Practical split I’d recommend

Use OCCT for the model, Apple frameworks for presentation and acceleration around the model:

1. OCCT-backed core ABI
   Keep all occ_shape_*, occ_make_*, occ_boolean_*, occ_shape_check/fix, BREP read/write, and exact properties/distance.
2. Apple-native adapter layer
   Convert occ_mesh_view_t into:
   * MTLBuffer vertex/index buffers for Metal.
   * MDLMesh for Model I/O export/import pipelines.
   * MeshResource / ModelComponent for RealityKit display.
3. Optional Apple fast paths
   Add fast paths for:
   * analytic line/circle/plane/sphere/cylinder evaluation,
   * mesh bounding boxes,
   * mesh-only mass estimates,
   * GPU sampling of surfaces for preview,
   * GPU picking against triangles,
   * GPU culling/selection/highlighting.
4. Do not replace
   Do not try to replace B-Rep booleans, healing, topology traversal, exact mass properties, or native BREP serialization with Metal/RealityKit/Model I/O. Those frameworks are rendering/asset/simulation frameworks, not CAD kernels. RealityKit, for example, is positioned as a high-performance 3D/AR rendering and simulation framework across Apple platforms.

Bottom line

The easy Apple-side implementation surface is roughly the mesh/rendering/math layer, not the CAD kernel layer. In this ABI, that means occ_mesh_*, Apple-native representations of occ_vec*/occ_mat4, display transforms, mesh bboxes, simple analytic evaluators, and primitive mesh previews. Everything involving TopoDS-style shapes, wires/faces/shells/solids, trimmed surfaces, healing, booleans, BREP files, and exact CAD queries should remain OCCT.