It depends what “this portion” means. Based on the uploaded headers, this is not a Metal port of OCCT. It is a small C99 ABI wrapper around the CPU CAD-kernel subset of OCCT, with only one rendering-adjacent feature: tessellation handoff via occ_shape_triangulate(), occ_face_triangulation(), and occ_mesh_view().

My estimate:

Goal	Difficulty	Why
Build the uploaded C ABI around OCCT on macOS/Apple Silicon	Medium	Mostly C++ wrapper work around OCCT handles, exception boundaries, reference counting, and build/linking.
Use OCCT CPU tessellation output with a Metal renderer	Medium	Very doable: triangulate B-Reps on CPU, convert mesh arrays into MTLBuffers, render with Metal.
Make the mesh handoff efficient on Apple unified memory	Medium	Use MTLStorageModeShared buffers, but you still likely need format conversion from OCCT-style double vectors to GPU-friendly float/packed vertex data.
Zero-copy OCCT-to-Metal vertex buffers	Medium–Hard	Possible only if you change the ABI/implementation so tessellation writes directly into caller-provided Metal-compatible memory. Current occ_mesh_view_t exposes borrowed CPU arrays, not Metal buffers.
Port OCCT geometry/booleans/healing/mass-properties themselves to Metal compute	Very Hard / impractical	OCCT is a large C++ CAD kernel with complex topology, exact geometry, tolerances, booleans, healing, and many pointer-heavy algorithms. Unified memory does not magically make those GPU-friendly.
Replace OCCT’s viewer with a native Metal backend	Hard	Separate from this header. OCCT visualization is historically OpenGL-based; an OCCT forum reply in 2025 said Vulkan support is planned long-term, not soon. Metal is not the stated upstream direction.

For your uploaded API specifically, I would call it very feasible if the goal is “OCCT CPU kernel + Metal rendering handoff.” I would not try to “implement OCCT on unified memory” as a GPU port.

The clean architecture would be:

OCCT C++ CPU kernel
↓
your C ABI: occ_shape_t / occ_mesh_t
↓
occ_shape_triangulate()
↓
Metal bridge layer
↓
MTLBuffer vertex/index buffers
↓
Metal renderer / SceneKit / RealityKit / custom renderer

Apple unified memory helps mainly at the handoff boundary. Metal’s shared storage mode defines memory accessible by both CPU and GPU, and Apple documents it as the default storage mode for buffers on integrated GPUs and for buffers/textures on Apple silicon GPUs.   That means you can allocate a shared MTLBuffer, fill it from CPU code, and render from it without a discrete-GPU-style staging upload. But you still need to respect synchronization and cache/resource-use rules.

The main mismatch in your current ABI is this:

typedef struct occ_vec3_t {
double x;
double y;
double z;
} occ_vec3_t;

Metal rendering usually wants something more like:

struct MetalVertex {
float position[3];
float normal[3];
float uv[2];
};

So the current occ_mesh_view_t is good for correctness and portability, but not ideal for direct Metal consumption:

typedef struct occ_mesh_view_t {
const occ_vec3_t* positions;
const occ_vec3_t* normals;
const occ_vec2_t* uvs;
size_t vertex_count;
const occ_triangle_t* triangles;
size_t triangle_count;
} occ_mesh_view_t;

I would add a separate optional Metal-friendly export layer rather than contaminate the core ABI with Metal types, especially because your header explicitly says rendering, GPU buffers, Metal, SceneKit, RealityKit, and platform frameworks are out of scope.

A good extension would be something like:

typedef enum occ_mesh_vertex_format_t {
OCC_MESH_VERTEX_FLOAT32_PNT = 1,   // position, normal, texcoord
OCC_MESH_VERTEX_FLOAT32_PN  = 2,
OCC_MESH_VERTEX_FLOAT64_RAW = 3
} occ_mesh_vertex_format_t;
typedef struct occ_mesh_packed_options_t {
occ_mesh_vertex_format_t format;
uint32_t interleaved;
uint32_t index_u32;
} occ_mesh_packed_options_t;
OCC_C_API occ_status_t occ_mesh_write_packed(
const occ_mesh_t* mesh,
const occ_mesh_packed_options_t* options,
void* vertex_dst,
size_t vertex_dst_size,
void* index_dst,
size_t index_dst_size,
size_t* out_vertex_bytes,
size_t* out_index_bytes
);

Then your Swift/Objective-C++ layer can do:

id<MTLBuffer> vertexBuffer =
[device newBufferWithLength:vertexBytes
options:MTLResourceStorageModeShared];
id<MTLBuffer> indexBuffer =
[device newBufferWithLength:indexBytes
options:MTLResourceStorageModeShared];
occ_mesh_write_packed(mesh, &options,
vertexBuffer.contents, vertexBytes,
indexBuffer.contents, indexBytes,
&writtenVertexBytes, &writtenIndexBytes);

That gives you near-zero-copy behavior from the app’s perspective: OCCT still computes on CPU, but the output is written directly into unified-memory-backed Metal buffers.

The hardest parts are not Metal itself. The harder parts are:

1. OCCT lifetime management
   You need safe opaque handles over OCCT Handle(...), TopoDS_Shape, Poly_Triangulation, etc.
2. Topology traversal semantics
   Subshape identity, location transforms, orientation, and unique traversal are easy to get subtly wrong.
3. Triangulation correctness
   Each face may have its own location/orientation. You need consistent winding, normals, UVs, and transform application.
4. Boolean/healing robustness
   Wrapping OCCT booleans is straightforward; making diagnostics useful is the real work.
5. Threading
   Your ABI exposes run_parallel options. You will need clear rules about whether occ_context_t is thread-safe, whether handles can be shared, and whether diagnostics are per-context or per-call.
6. Apple/Metal vertex format choices
   double geometry is right for CAD, but float mesh output is usually right for rendering. Keep both.

OCCT itself is a large C++ CAD/CAM/CAE platform; the GitHub README describes it as mostly C++ libraries for modeling, data exchange, and visualization.   The public repo is also overwhelmingly C++ by language breakdown, which matches the wrapper approach better than any Metal-native rewrite.

Bottom line: implementing the uploaded C ABI as a CPU OCCT wrapper and adding a Metal/unified-memory mesh bridge is reasonable, maybe a few weeks for a strong C++/OCCT/Metal engineer to get a useful MVP, and a few months to harden. Porting OCCT’s actual modeling algorithms to Metal is not reasonable unless you are starting a multi-year geometry-kernel research project.