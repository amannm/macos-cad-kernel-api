#ifndef CAD_ABI_H
#define CAD_ABI_H

/*
    CAD C ABI
    =========

    Stable C ABI for a parametric CAD document runtime implemented internally in
    C++, intended to use Open CASCADE Technology (OCCT) as an implementation
    backend without exposing OCCT C++ types, STL types, exceptions, templates,
    inheritance, or compiler ABI details.

    This ABI models the product-level CAD runtime, not OCCT directly. Public
    identity belongs to documents, objects, features, snapshots, and stable-ish
    references. Exact OCCT shapes are implementation details.

    Draft status:
      - Unreleased ABI draft.
      - Source and binary compatibility are not promised for this draft.
      - All extensible public structs include struct_size for future ABI growth.

    Design goals:
      - Stable binary boundary once released.
      - Swift/Rust/C/Python/plugin-friendly.
      - Document- and feature-oriented parametric API.
      - No OCCT types in public headers.
      - Explicit ownership and lifetime rules.
      - Exceptions never cross the ABI.
      - All returned resources have matching destroy/release functions.

    General ownership and lifetime rules:
      - Functions returning newly-created handles transfer ownership to caller.
      - Ref-counted handles use retain/release. Unique resources use destroy.
      - Borrowed handles are valid only while their owner remains alive.
      - Borrowed memory returned through view structs is immutable and remains
        valid only while the owning ABI object remains alive.
      - Unless otherwise stated, input cad_string_view, cad_bytes_view, arrays,
        cad_param values, descriptors, and callbacks are consumed only for the
        duration of the call. Implementations must copy data or retain handles if
        they need to keep them after the call returns.
      - Passing NULL for required pointers returns CAD_ERROR_NULL_HANDLE or
        CAD_ERROR_INVALID_ARGUMENT.
      - All functions returning cad_result set the relevant context/document last
        error on failure where possible.

    Threading:
      - cad_context is the root runtime object.
      - cad_document mutation is not thread-safe unless the implementation is in
        CAD_THREADING_CONTEXT_SERIALIZED mode or explicitly documents otherwise.
      - Snapshots are immutable, revisioned, and intended for concurrent
        read/render/pick use.

    Units:
      - Geometric values use double precision.
      - Documents have length and angle units.
      - Angles are radians internally unless a parameter explicitly declares a
        different unit.

    Transactions:
      - Nested transactions are supported.
      - Only committing the outermost transaction creates an undo step.
      - Rolling back any active transaction rolls the document back to the state
        at the matching begin_transaction call and closes nested scopes.
*/

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
   Export / calling convention
   ========================================================================== */

#ifndef CAD_API
  #if defined(_WIN32)
    #if defined(CAD_ABI_BUILD)
      #define CAD_API __declspec(dllexport)
    #else
      #define CAD_API __declspec(dllimport)
    #endif
  #else
    #define CAD_API __attribute__((visibility("default")))
  #endif
#endif

#ifndef CAD_CALL
  #define CAD_CALL
#endif

#define CAD_ABI_VERSION_MAJOR 1u
#define CAD_ABI_VERSION_MINOR 0u
#define CAD_ABI_VERSION_PATCH 0u

#define CAD_STRUCT_SIZE(type_) ((uint32_t)sizeof(type_))

/* ============================================================================
   Opaque handles
   ========================================================================== */

typedef struct cad_context_t*              cad_context;
typedef struct cad_document_t*             cad_document;
typedef struct cad_snapshot_t*             cad_snapshot;
typedef struct cad_object_t*               cad_object;
typedef struct cad_shape_t*                cad_shape;
typedef struct cad_mesh_t*                 cad_mesh;
typedef struct cad_string_t*               cad_string;
typedef struct cad_bytes_t*                cad_bytes;
typedef struct cad_error_t*                cad_error;
typedef struct cad_object_array_t*         cad_object_array;
typedef struct cad_string_array_t*         cad_string_array;
typedef struct cad_diagnostic_array_t*     cad_diagnostic_array;
typedef struct cad_param_array_t*          cad_param_array;
typedef struct cad_param_value_t*          cad_param_value;
typedef struct cad_subshape_array_t*       cad_subshape_array;
typedef struct cad_sketch_element_array_t* cad_sketch_element_array;
typedef struct cad_constraint_array_t*     cad_constraint_array;
typedef struct cad_selection_t*            cad_selection;
typedef struct cad_sketch_element_t*       cad_sketch_element;
typedef struct cad_constraint_t*           cad_constraint;
typedef struct cad_plugin_registry_t*      cad_plugin_registry;

/* ============================================================================
   Primitive data types
   ========================================================================== */

typedef struct cad_version_t {
    uint32_t struct_size;

    uint32_t abi_major;
    uint32_t abi_minor;
    uint32_t abi_patch;

    uint32_t kernel_major;
    uint32_t kernel_minor;
    uint32_t kernel_patch;

    uint32_t reserved0;
} cad_version;

typedef struct cad_uuid_t {
    uint8_t bytes[16];
} cad_uuid;

typedef uint64_t cad_revision;

typedef struct cad_string_view_t {
    const char* data;
    size_t size;
} cad_string_view;

typedef struct cad_bytes_view_t {
    const uint8_t* data;
    size_t size;
} cad_bytes_view;

typedef struct cad_vec2_t { double x; double y; } cad_vec2;
typedef struct cad_vec3_t { double x; double y; double z; } cad_vec3;
typedef struct cad_quat_t { double x; double y; double z; double w; } cad_quat;

typedef struct cad_mat4_t {
    /* Column-major 4x4 matrix: m[col * 4 + row]. */
    double m[16];
} cad_mat4;

typedef struct cad_plane_t {
    cad_vec3 origin;
    cad_vec3 normal;
    cad_vec3 x_axis;
} cad_plane;

typedef struct cad_box3_t { cad_vec3 min; cad_vec3 max; } cad_box3;
typedef struct cad_ray_t { cad_vec3 origin; cad_vec3 direction; } cad_ray;

typedef struct cad_color_rgba_t {
    float r;
    float g;
    float b;
    float a;
} cad_color_rgba;

/* ============================================================================
   Result / errors / diagnostics basics
   ========================================================================== */

typedef enum cad_result_t {
    CAD_OK = 0,

    CAD_ERROR_UNKNOWN = 1,
    CAD_ERROR_INVALID_ARGUMENT = 2,
    CAD_ERROR_NULL_HANDLE = 3,
    CAD_ERROR_OUT_OF_MEMORY = 4,
    CAD_ERROR_INDEX_OUT_OF_RANGE = 5,
    CAD_ERROR_NOT_FOUND = 6,
    CAD_ERROR_ALREADY_EXISTS = 7,
    CAD_ERROR_UNSUPPORTED = 8,
    CAD_ERROR_IO = 9,
    CAD_ERROR_CANCELLED = 10,
    CAD_ERROR_BAD_STATE = 11,
    CAD_ERROR_ABI_MISMATCH = 12,

    CAD_ERROR_OCCT_EXCEPTION = 100,
    CAD_ERROR_MODELING_FAILED = 101,
    CAD_ERROR_RECOMPUTE_FAILED = 102,
    CAD_ERROR_INVALID_TOPOLOGY = 103,
    CAD_ERROR_INVALID_GEOMETRY = 104,
    CAD_ERROR_BOOLEAN_FAILED = 105,
    CAD_ERROR_FILLET_FAILED = 106,
    CAD_ERROR_CHAMFER_FAILED = 107,
    CAD_ERROR_SKETCH_SOLVE_FAILED = 108,
    CAD_ERROR_CONSTRAINT_CONFLICT = 109,
    CAD_ERROR_REFERENCE_LOST = 110,

    CAD_ERROR_DOCUMENT_CORRUPT = 200,
    CAD_ERROR_DOCUMENT_VERSION_TOO_NEW = 201,
    CAD_ERROR_DOCUMENT_VERSION_TOO_OLD = 202,
    CAD_ERROR_IMPORT_FAILED = 203,
    CAD_ERROR_EXPORT_FAILED = 204,

    CAD_ERROR_PLUGIN_FAILED = 300,
    CAD_ERROR_PLUGIN_ABI_MISMATCH = 301,
    CAD_ERROR_PLUGIN_NOT_LOADED = 302,
    CAD_ERROR_PLUGIN_IN_USE = 303
} cad_result;

typedef enum cad_severity_t {
    CAD_SEVERITY_INFO = 0,
    CAD_SEVERITY_WARNING = 1,
    CAD_SEVERITY_ERROR = 2
} cad_severity;

CAD_API void CAD_CALL cad_error_destroy(cad_error error);
CAD_API cad_result CAD_CALL cad_error_get_code(cad_error error, cad_result* out_code);
CAD_API cad_result CAD_CALL cad_error_get_message(cad_error error, cad_string* out_message);
CAD_API cad_result CAD_CALL cad_error_get_source(cad_error error, cad_string* out_source);

/* ============================================================================
   Units
   ========================================================================== */

typedef enum cad_length_unit_t {
    CAD_LENGTH_UNIT_MILLIMETER = 0,
    CAD_LENGTH_UNIT_CENTIMETER = 1,
    CAD_LENGTH_UNIT_METER = 2,
    CAD_LENGTH_UNIT_INCH = 3,
    CAD_LENGTH_UNIT_FOOT = 4
} cad_length_unit;

typedef enum cad_angle_unit_t {
    CAD_ANGLE_UNIT_RADIAN = 0,
    CAD_ANGLE_UNIT_DEGREE = 1
} cad_angle_unit;

/* ============================================================================
   Topology and persistent subshape references
   ========================================================================== */

typedef enum cad_topology_type_t {
    CAD_TOPOLOGY_UNKNOWN = 0,
    CAD_TOPOLOGY_VERTEX = 1,
    CAD_TOPOLOGY_EDGE = 2,
    CAD_TOPOLOGY_WIRE = 3,
    CAD_TOPOLOGY_FACE = 4,
    CAD_TOPOLOGY_SHELL = 5,
    CAD_TOPOLOGY_SOLID = 6,
    CAD_TOPOLOGY_COMPSOLID = 7,
    CAD_TOPOLOGY_COMPOUND = 8
} cad_topology_type;

/*
    Subshape references are best-effort persistent references. They are not a
    guarantee that a face/edge survives arbitrary feature edits. Callers must
    check status after recompute and handle split/merged/replaced references.
*/
typedef enum cad_subshape_status_t {
    CAD_SUBSHAPE_STATUS_RESOLVED = 0,
    CAD_SUBSHAPE_STATUS_MODIFIED = 1,
    CAD_SUBSHAPE_STATUS_REPLACED = 2,
    CAD_SUBSHAPE_STATUS_SPLIT = 3,
    CAD_SUBSHAPE_STATUS_MERGED = 4,
    CAD_SUBSHAPE_STATUS_REORDERED = 5,
    CAD_SUBSHAPE_STATUS_DELETED = 6,
    CAD_SUBSHAPE_STATUS_AMBIGUOUS = 7,
    CAD_SUBSHAPE_STATUS_UNRESOLVED = 8
} cad_subshape_status;

typedef struct cad_subshape_ref_t {
    uint32_t struct_size;
    uint32_t reserved0;

    cad_uuid owner_object_id;
    cad_uuid persistent_id;
    cad_topology_type topology_type;
    uint32_t reserved1;

    uint64_t transient_index;
    uint64_t generation;
} cad_subshape_ref;

/* ============================================================================
   Allocator and context lifecycle
   ========================================================================== */

typedef void* (CAD_CALL *cad_alloc_fn)(void* user_data, size_t size, size_t alignment);
typedef void  (CAD_CALL *cad_free_fn)(void* user_data, void* ptr);

typedef struct cad_allocator_t {
    uint32_t struct_size;
    uint32_t reserved0;
    void* user_data;
    cad_alloc_fn alloc;
    cad_free_fn free;
} cad_allocator;

typedef enum cad_threading_mode_t {
    CAD_THREADING_SINGLE_THREADED = 0,
    CAD_THREADING_CONTEXT_SERIALIZED = 1
} cad_threading_mode;

CAD_API cad_result CAD_CALL cad_get_version(cad_version* out_version);

CAD_API cad_result CAD_CALL cad_context_create(cad_context* out_context);
CAD_API cad_result CAD_CALL cad_context_create_with_allocator(
    const cad_allocator* allocator,
    cad_context* out_context
);
CAD_API void CAD_CALL cad_context_destroy(cad_context context);

CAD_API cad_result CAD_CALL cad_context_get_last_error(cad_context context, cad_error* out_error);
CAD_API cad_result CAD_CALL cad_context_get_last_error_code(cad_context context, cad_result* out_code);
CAD_API cad_result CAD_CALL cad_context_get_last_error_message(cad_context context, cad_string* out_message);
CAD_API cad_result CAD_CALL cad_context_clear_last_error(cad_context context);

CAD_API cad_result CAD_CALL cad_context_set_threading_mode(cad_context context, cad_threading_mode mode);
CAD_API cad_result CAD_CALL cad_context_get_threading_mode(cad_context context, cad_threading_mode* out_mode);

/* ============================================================================
   Strings and byte buffers
   ========================================================================== */

CAD_API void CAD_CALL cad_string_destroy(cad_string string);
CAD_API cad_result CAD_CALL cad_string_data(cad_string string, cad_string_view* out_view);

CAD_API void CAD_CALL cad_bytes_destroy(cad_bytes bytes);
CAD_API cad_result CAD_CALL cad_bytes_data(cad_bytes bytes, cad_bytes_view* out_view);

CAD_API void CAD_CALL cad_string_array_destroy(cad_string_array array);
CAD_API cad_result CAD_CALL cad_string_array_count(cad_string_array array, size_t* out_count);
CAD_API cad_result CAD_CALL cad_string_array_get_view(cad_string_array array, size_t index, cad_string_view* out_view);
CAD_API cad_result CAD_CALL cad_string_array_get_owned(cad_string_array array, size_t index, cad_string* out_string);

/* ============================================================================
   Document lifecycle and persistence
   ========================================================================== */

typedef enum cad_document_open_flags_t {
    CAD_DOCUMENT_OPEN_NONE = 0,
    CAD_DOCUMENT_OPEN_READ_ONLY = 1 << 0,
    CAD_DOCUMENT_OPEN_RECOVER = 1 << 1
} cad_document_open_flags;

typedef enum cad_document_save_flags_t {
    CAD_DOCUMENT_SAVE_NONE = 0,
    CAD_DOCUMENT_SAVE_COMPACT = 1 << 0,
    CAD_DOCUMENT_SAVE_INCLUDE_SHAPE_CACHE = 1 << 1
} cad_document_save_flags;

typedef enum cad_recompute_flags_t {
    CAD_RECOMPUTE_NONE = 0,
    CAD_RECOMPUTE_FORCE = 1 << 0,
    CAD_RECOMPUTE_STOP_ON_ERROR = 1 << 1,
    CAD_RECOMPUTE_ALLOW_PARTIAL = 1 << 2
} cad_recompute_flags;

CAD_API cad_result CAD_CALL cad_document_create(cad_context context, cad_document* out_document);
CAD_API cad_result CAD_CALL cad_document_open(cad_context context, cad_string_view path, uint32_t open_flags, cad_document* out_document);
CAD_API cad_result CAD_CALL cad_document_save(cad_document document, cad_string_view path, uint32_t save_flags);
CAD_API cad_result CAD_CALL cad_document_close(cad_document document);
CAD_API void CAD_CALL cad_document_destroy(cad_document document);

CAD_API cad_result CAD_CALL cad_document_get_last_error(cad_document document, cad_error* out_error);
CAD_API cad_result CAD_CALL cad_document_get_last_error_code(cad_document document, cad_result* out_code);
CAD_API cad_result CAD_CALL cad_document_get_last_error_message(cad_document document, cad_string* out_message);

CAD_API cad_result CAD_CALL cad_document_get_id(cad_document document, cad_uuid* out_id);
CAD_API cad_result CAD_CALL cad_document_get_revision(cad_document document, cad_revision* out_revision);
CAD_API cad_result CAD_CALL cad_document_set_name(cad_document document, cad_string_view name);
CAD_API cad_result CAD_CALL cad_document_get_name(cad_document document, cad_string* out_name);
CAD_API cad_result CAD_CALL cad_document_set_length_unit(cad_document document, cad_length_unit unit);
CAD_API cad_result CAD_CALL cad_document_get_length_unit(cad_document document, cad_length_unit* out_unit);
CAD_API cad_result CAD_CALL cad_document_set_angle_unit(cad_document document, cad_angle_unit unit);
CAD_API cad_result CAD_CALL cad_document_get_angle_unit(cad_document document, cad_angle_unit* out_unit);

/* ============================================================================
   Transactions, undo, redo
   ========================================================================== */

CAD_API cad_result CAD_CALL cad_document_begin_transaction(cad_document document, cad_string_view name);
CAD_API cad_result CAD_CALL cad_document_commit_transaction(cad_document document);
CAD_API cad_result CAD_CALL cad_document_rollback_transaction(cad_document document);
CAD_API cad_result CAD_CALL cad_document_get_transaction_depth(cad_document document, uint32_t* out_depth);

CAD_API cad_result CAD_CALL cad_document_can_undo(cad_document document, bool* out_can_undo);
CAD_API cad_result CAD_CALL cad_document_can_redo(cad_document document, bool* out_can_redo);
CAD_API cad_result CAD_CALL cad_document_undo(cad_document document);
CAD_API cad_result CAD_CALL cad_document_redo(cad_document document);

/* ============================================================================
   Object model
   ========================================================================== */

typedef enum cad_object_kind_t {
    CAD_OBJECT_KIND_UNKNOWN = 0,
    CAD_OBJECT_KIND_BODY = 1,
    CAD_OBJECT_KIND_FEATURE = 2,
    CAD_OBJECT_KIND_SKETCH = 3,
    CAD_OBJECT_KIND_GROUP = 4,
    CAD_OBJECT_KIND_DATUM = 5,
    CAD_OBJECT_KIND_MATERIAL = 6,
    CAD_OBJECT_KIND_IMPORTED = 7,
    CAD_OBJECT_KIND_ASSEMBLY = 8,
    CAD_OBJECT_KIND_COMPONENT = 9,
    CAD_OBJECT_KIND_INSTANCE = 10
} cad_object_kind;

typedef enum cad_object_capability_t {
    CAD_OBJECT_CAP_NONE = 0,
    CAD_OBJECT_CAP_SHAPE = 1u << 0,
    CAD_OBJECT_CAP_FEATURE = 1u << 1,
    CAD_OBJECT_CAP_SKETCH = 1u << 2,
    CAD_OBJECT_CAP_CHILDREN = 1u << 3,
    CAD_OBJECT_CAP_TRANSFORM = 1u << 4,
    CAD_OBJECT_CAP_MATERIAL = 1u << 5,
    CAD_OBJECT_CAP_SELECTION = 1u << 6,
    CAD_OBJECT_CAP_SUPPRESSION = 1u << 7
} cad_object_capability;

CAD_API void CAD_CALL cad_object_release(cad_object object);
CAD_API cad_result CAD_CALL cad_object_retain(cad_object object, cad_object* out_object);
CAD_API cad_result CAD_CALL cad_object_get_id(cad_object object, cad_uuid* out_id);
CAD_API cad_result CAD_CALL cad_object_get_revision(cad_object object, cad_revision* out_revision);
CAD_API cad_result CAD_CALL cad_object_get_kind(cad_object object, cad_object_kind* out_kind);
CAD_API cad_result CAD_CALL cad_object_get_capabilities(cad_object object, uint64_t* out_capabilities);
CAD_API cad_result CAD_CALL cad_object_set_name(cad_object object, cad_string_view name);
CAD_API cad_result CAD_CALL cad_object_get_name(cad_object object, cad_string* out_name);
CAD_API cad_result CAD_CALL cad_object_set_visible(cad_object object, bool visible);
CAD_API cad_result CAD_CALL cad_object_get_visible(cad_object object, bool* out_visible);
CAD_API cad_result CAD_CALL cad_object_set_color(cad_object object, cad_color_rgba color);
CAD_API cad_result CAD_CALL cad_object_get_color(cad_object object, cad_color_rgba* out_color);
CAD_API cad_result CAD_CALL cad_object_get_parent(cad_object object, cad_object* out_parent);
CAD_API cad_result CAD_CALL cad_object_get_children(cad_object object, cad_object_array* out_children);
CAD_API cad_result CAD_CALL cad_object_set_parent(cad_object object, cad_object parent);
CAD_API cad_result CAD_CALL cad_object_get_local_transform(cad_object object, cad_mat4* out_transform);
CAD_API cad_result CAD_CALL cad_object_set_local_transform(cad_object object, cad_mat4 transform);
CAD_API cad_result CAD_CALL cad_object_get_world_transform(cad_object object, cad_mat4* out_transform);

CAD_API cad_result CAD_CALL cad_document_find_object(cad_document document, cad_uuid id, cad_object* out_object);
CAD_API cad_result CAD_CALL cad_document_get_root_objects(cad_document document, cad_object_array* out_objects);
CAD_API cad_result CAD_CALL cad_document_delete_object(cad_document document, cad_object object);

CAD_API void CAD_CALL cad_object_array_destroy(cad_object_array array);
CAD_API cad_result CAD_CALL cad_object_array_count(cad_object_array array, size_t* out_count);
CAD_API cad_result CAD_CALL cad_object_array_get_borrowed(cad_object_array array, size_t index, cad_object* out_object);
CAD_API cad_result CAD_CALL cad_object_array_get_retained(cad_object_array array, size_t index, cad_object* out_object);

/* ============================================================================
   Parameters
   ========================================================================== */

typedef enum cad_param_type_t {
    CAD_PARAM_NULL = 0,
    CAD_PARAM_BOOL = 1,
    CAD_PARAM_INT = 2,
    CAD_PARAM_DOUBLE = 3,
    CAD_PARAM_LENGTH = 4,
    CAD_PARAM_ANGLE = 5,
    CAD_PARAM_STRING = 6,
    CAD_PARAM_UUID = 7,
    CAD_PARAM_VEC2 = 8,
    CAD_PARAM_VEC3 = 9,
    CAD_PARAM_PLANE = 10,
    CAD_PARAM_OBJECT = 11,
    CAD_PARAM_SUBSHAPE = 12,
    CAD_PARAM_ENUM = 13
} cad_param_type;

typedef enum cad_param_value_ownership_t {
    CAD_PARAM_VALUE_BORROWED = 0,
    CAD_PARAM_VALUE_OWNED = 1
} cad_param_value_ownership;

typedef struct cad_param_t {
    uint32_t struct_size;
    cad_param_type type;

    cad_string_view name;

    union {
        bool bool_value;
        int64_t int_value;
        double double_value;
        cad_string_view string_value;
        cad_uuid uuid_value;
        cad_vec2 vec2_value;
        cad_vec3 vec3_value;
        cad_plane plane_value;
        cad_object object_value;
        cad_subshape_ref subshape_value;
        int32_t enum_value;
    } value;

    cad_length_unit length_unit;
    cad_angle_unit angle_unit;
    uint32_t reserved0;
} cad_param;

typedef struct cad_param_value_desc_t {
    uint32_t struct_size;
    cad_param_type type;

    union {
        bool bool_value;
        int64_t int_value;
        double double_value;
        cad_string_view string_value;
        cad_uuid uuid_value;
        cad_vec2 vec2_value;
        cad_vec3 vec3_value;
        cad_plane plane_value;
        cad_object object_value;
        cad_subshape_ref subshape_value;
        int32_t enum_value;
    } value;

    cad_length_unit length_unit;
    cad_angle_unit angle_unit;
    cad_param_value_ownership ownership;
    uint32_t reserved0;
} cad_param_value_desc;

CAD_API void CAD_CALL cad_param_array_destroy(cad_param_array array);
CAD_API cad_result CAD_CALL cad_param_array_count(cad_param_array array, size_t* out_count);
CAD_API cad_result CAD_CALL cad_param_array_get(cad_param_array array, size_t index, cad_param* out_param);
CAD_API void CAD_CALL cad_param_value_destroy(cad_param_value value);
CAD_API cad_result CAD_CALL cad_param_value_data(cad_param_value value, cad_param_value_desc* out_desc);

/* ============================================================================
   Features and parametric graph
   ========================================================================== */

typedef enum cad_feature_type_t {
    CAD_FEATURE_UNKNOWN = 0,
    CAD_FEATURE_GROUP = 1,
    CAD_FEATURE_DATUM_PLANE = 2,
    CAD_FEATURE_DATUM_AXIS = 3,
    CAD_FEATURE_DATUM_POINT = 4,

    CAD_FEATURE_SKETCH = 100,
    CAD_FEATURE_EXTRUDE = 101,
    CAD_FEATURE_REVOLVE = 102,
    CAD_FEATURE_SWEEP = 103,
    CAD_FEATURE_LOFT = 104,

    CAD_FEATURE_BOX = 200,
    CAD_FEATURE_CYLINDER = 201,
    CAD_FEATURE_CONE = 202,
    CAD_FEATURE_SPHERE = 203,
    CAD_FEATURE_TORUS = 204,

    CAD_FEATURE_BOOLEAN = 300,
    CAD_FEATURE_FILLET = 301,
    CAD_FEATURE_CHAMFER = 302,
    CAD_FEATURE_SHELL = 303,
    CAD_FEATURE_DRAFT = 304,
    CAD_FEATURE_THICKEN = 305,
    CAD_FEATURE_OFFSET = 306,

    CAD_FEATURE_LINEAR_PATTERN = 400,
    CAD_FEATURE_CIRCULAR_PATTERN = 401,
    CAD_FEATURE_MIRROR = 402,

    CAD_FEATURE_IMPORTED = 900,
    CAD_FEATURE_CUSTOM = 10000
} cad_feature_type;

typedef enum cad_feature_state_t {
    CAD_FEATURE_STATE_OK = 0,
    CAD_FEATURE_STATE_DIRTY = 1,
    CAD_FEATURE_STATE_SUPPRESSED = 2,
    CAD_FEATURE_STATE_FAILED = 3,
    CAD_FEATURE_STATE_REFERENCE_LOST = 4,
    CAD_FEATURE_STATE_MISSING_PLUGIN = 5
} cad_feature_state;

typedef enum cad_boolean_op_t {
    CAD_BOOLEAN_UNION = 0,
    CAD_BOOLEAN_SUBTRACT = 1,
    CAD_BOOLEAN_INTERSECT = 2
} cad_boolean_op;

typedef enum cad_extrude_mode_t {
    CAD_EXTRUDE_ONE_SIDED = 0,
    CAD_EXTRUDE_TWO_SIDED = 1,
    CAD_EXTRUDE_SYMMETRIC = 2,
    CAD_EXTRUDE_TO_OBJECT = 3
} cad_extrude_mode;

typedef enum cad_revolve_mode_t {
    CAD_REVOLVE_ANGLE = 0,
    CAD_REVOLVE_FULL = 1,
    CAD_REVOLVE_TO_OBJECT = 2
} cad_revolve_mode;

CAD_API cad_result CAD_CALL cad_feature_create(cad_document document, cad_feature_type type, const cad_param* params, size_t param_count, cad_object* out_feature);
CAD_API cad_result CAD_CALL cad_feature_create_custom(cad_document document, cad_string_view custom_type, const cad_param* params, size_t param_count, cad_object* out_feature);
CAD_API cad_result CAD_CALL cad_feature_get_type(cad_object feature, cad_feature_type* out_type);
CAD_API cad_result CAD_CALL cad_feature_get_custom_type(cad_object feature, cad_string* out_custom_type);
CAD_API cad_result CAD_CALL cad_feature_get_state(cad_object feature, cad_feature_state* out_state);
CAD_API cad_result CAD_CALL cad_feature_set_suppressed(cad_object feature, bool suppressed);
CAD_API cad_result CAD_CALL cad_feature_get_suppressed(cad_object feature, bool* out_suppressed);
CAD_API cad_result CAD_CALL cad_feature_set_param(cad_object feature, cad_string_view name, const cad_param* value);
CAD_API cad_result CAD_CALL cad_feature_set_param_expression(cad_object feature, cad_string_view name, cad_string_view expression);
CAD_API cad_result CAD_CALL cad_feature_get_param_value(cad_object feature, cad_string_view name, cad_param_value* out_value);
CAD_API cad_result CAD_CALL cad_feature_get_param_expression(cad_object feature, cad_string_view name, cad_string* out_expression);
CAD_API cad_result CAD_CALL cad_feature_get_params(cad_object feature, cad_param_array* out_params);
CAD_API cad_result CAD_CALL cad_feature_get_param_names(cad_object feature, cad_string_array* out_names);
CAD_API cad_result CAD_CALL cad_feature_add_input(cad_object feature, cad_string_view role, cad_object input);
CAD_API cad_result CAD_CALL cad_feature_remove_input(cad_object feature, cad_string_view role, cad_object input);
CAD_API cad_result CAD_CALL cad_feature_clear_inputs(cad_object feature, cad_string_view role);
CAD_API cad_result CAD_CALL cad_feature_get_inputs(cad_object feature, cad_string_view role, cad_object_array* out_inputs);
CAD_API cad_result CAD_CALL cad_feature_get_input_roles(cad_object feature, cad_string_array* out_roles);

CAD_API cad_result CAD_CALL cad_document_recompute(cad_document document, uint32_t recompute_flags);
CAD_API cad_result CAD_CALL cad_document_recompute_object(cad_document document, cad_object object, uint32_t recompute_flags);
CAD_API cad_result CAD_CALL cad_document_set_variable(cad_document document, cad_string_view name, cad_string_view expression);
CAD_API cad_result CAD_CALL cad_document_get_variable_expression(cad_document document, cad_string_view name, cad_string* out_expression);
CAD_API cad_result CAD_CALL cad_document_evaluate_expression(cad_document document, cad_string_view expression, cad_param_value* out_value);
CAD_API cad_result CAD_CALL cad_document_get_variable_names(cad_document document, cad_string_array* out_names);

CAD_API cad_result CAD_CALL cad_feature_create_box(cad_document document, double width, double depth, double height, cad_object* out_feature);
CAD_API cad_result CAD_CALL cad_feature_create_cylinder(cad_document document, double radius, double height, cad_object* out_feature);
CAD_API cad_result CAD_CALL cad_feature_create_sphere(cad_document document, double radius, cad_object* out_feature);
CAD_API cad_result CAD_CALL cad_feature_create_extrude(cad_document document, cad_object profile_or_sketch, double distance, cad_extrude_mode mode, cad_object* out_feature);
CAD_API cad_result CAD_CALL cad_feature_create_revolve(cad_document document, cad_object profile_or_sketch, cad_vec3 axis_origin, cad_vec3 axis_direction, double angle_radians, cad_revolve_mode mode, cad_object* out_feature);
CAD_API cad_result CAD_CALL cad_feature_create_boolean(cad_document document, cad_boolean_op operation, cad_object target, cad_object tool, cad_object* out_feature);
CAD_API cad_result CAD_CALL cad_feature_create_fillet(cad_document document, cad_object body, const cad_subshape_ref* edges, size_t edge_count, double radius, cad_object* out_feature);
CAD_API cad_result CAD_CALL cad_feature_create_chamfer(cad_document document, cad_object body, const cad_subshape_ref* edges, size_t edge_count, double distance, cad_object* out_feature);

/* ============================================================================
   Sketches and constraints
   ========================================================================== */

typedef enum cad_sketch_element_type_t {
    CAD_SKETCH_ELEMENT_UNKNOWN = 0,
    CAD_SKETCH_ELEMENT_POINT = 1,
    CAD_SKETCH_ELEMENT_LINE = 2,
    CAD_SKETCH_ELEMENT_CIRCLE = 3,
    CAD_SKETCH_ELEMENT_ARC = 4,
    CAD_SKETCH_ELEMENT_ELLIPSE = 5,
    CAD_SKETCH_ELEMENT_SPLINE = 6,
    CAD_SKETCH_ELEMENT_CONSTRUCTION_LINE = 7
} cad_sketch_element_type;

typedef enum cad_constraint_type_t {
    CAD_CONSTRAINT_UNKNOWN = 0,
    CAD_CONSTRAINT_COINCIDENT = 1,
    CAD_CONSTRAINT_HORIZONTAL = 2,
    CAD_CONSTRAINT_VERTICAL = 3,
    CAD_CONSTRAINT_PARALLEL = 4,
    CAD_CONSTRAINT_PERPENDICULAR = 5,
    CAD_CONSTRAINT_TANGENT = 6,
    CAD_CONSTRAINT_EQUAL = 7,
    CAD_CONSTRAINT_DISTANCE = 8,
    CAD_CONSTRAINT_ANGLE = 9,
    CAD_CONSTRAINT_RADIUS = 10,
    CAD_CONSTRAINT_DIAMETER = 11,
    CAD_CONSTRAINT_MIDPOINT = 12,
    CAD_CONSTRAINT_SYMMETRIC = 13,
    CAD_CONSTRAINT_FIX = 14
} cad_constraint_type;

typedef enum cad_sketch_solve_status_t {
    CAD_SKETCH_SOLVE_OK = 0,
    CAD_SKETCH_SOLVE_UNDER_CONSTRAINED = 1,
    CAD_SKETCH_SOLVE_FULLY_CONSTRAINED = 2,
    CAD_SKETCH_SOLVE_OVER_CONSTRAINED = 3,
    CAD_SKETCH_SOLVE_INCONSISTENT = 4,
    CAD_SKETCH_SOLVE_FAILED = 5
} cad_sketch_solve_status;

CAD_API cad_result CAD_CALL cad_sketch_create_on_plane(cad_document document, cad_plane plane, cad_object* out_sketch);
CAD_API cad_result CAD_CALL cad_sketch_get_plane(cad_object sketch, cad_plane* out_plane);
CAD_API cad_result CAD_CALL cad_sketch_set_plane(cad_object sketch, cad_plane plane);
CAD_API cad_result CAD_CALL cad_sketch_add_point(cad_object sketch, cad_vec2 point, cad_sketch_element* out_element);
CAD_API cad_result CAD_CALL cad_sketch_add_line(cad_object sketch, cad_vec2 start, cad_vec2 end, cad_sketch_element* out_element);
CAD_API cad_result CAD_CALL cad_sketch_add_circle(cad_object sketch, cad_vec2 center, double radius, cad_sketch_element* out_element);
CAD_API cad_result CAD_CALL cad_sketch_add_arc_three_points(cad_object sketch, cad_vec2 start, cad_vec2 mid, cad_vec2 end, cad_sketch_element* out_element);
CAD_API cad_result CAD_CALL cad_sketch_add_arc_center_start_end(cad_object sketch, cad_vec2 center, cad_vec2 start, cad_vec2 end, cad_sketch_element* out_element);
CAD_API cad_result CAD_CALL cad_sketch_delete_element(cad_object sketch, cad_sketch_element element);
CAD_API cad_result CAD_CALL cad_sketch_get_element_count(cad_object sketch, size_t* out_count);
CAD_API cad_result CAD_CALL cad_sketch_get_elements(cad_object sketch, cad_sketch_element_array* out_elements);
CAD_API cad_result CAD_CALL cad_sketch_element_get_id(cad_sketch_element element, cad_uuid* out_id);
CAD_API cad_result CAD_CALL cad_sketch_element_get_type(cad_sketch_element element, cad_sketch_element_type* out_type);
CAD_API cad_result CAD_CALL cad_sketch_element_get_points(cad_sketch_element element, cad_vec2* out_a, cad_vec2* out_b, cad_vec2* out_c);
CAD_API cad_result CAD_CALL cad_sketch_element_set_points(cad_sketch_element element, cad_vec2 a, cad_vec2 b, cad_vec2 c);
CAD_API cad_result CAD_CALL cad_sketch_element_get_radius(cad_sketch_element element, double* out_radius);
CAD_API cad_result CAD_CALL cad_sketch_element_set_radius(cad_sketch_element element, double radius);
CAD_API void CAD_CALL cad_sketch_element_array_destroy(cad_sketch_element_array array);
CAD_API cad_result CAD_CALL cad_sketch_element_array_count(cad_sketch_element_array array, size_t* out_count);
CAD_API cad_result CAD_CALL cad_sketch_element_array_get_borrowed(cad_sketch_element_array array, size_t index, cad_sketch_element* out_element);
CAD_API cad_result CAD_CALL cad_sketch_add_constraint(cad_object sketch, cad_constraint_type type, const cad_param* params, size_t param_count, cad_constraint* out_constraint);
CAD_API cad_result CAD_CALL cad_sketch_delete_constraint(cad_object sketch, cad_constraint constraint);
CAD_API cad_result CAD_CALL cad_sketch_get_constraints(cad_object sketch, cad_constraint_array* out_constraints);
CAD_API cad_result CAD_CALL cad_constraint_get_id(cad_constraint constraint, cad_uuid* out_id);
CAD_API cad_result CAD_CALL cad_constraint_get_type(cad_constraint constraint, cad_constraint_type* out_type);
CAD_API cad_result CAD_CALL cad_constraint_get_params(cad_constraint constraint, cad_param_array* out_params);
CAD_API void CAD_CALL cad_constraint_array_destroy(cad_constraint_array array);
CAD_API cad_result CAD_CALL cad_constraint_array_count(cad_constraint_array array, size_t* out_count);
CAD_API cad_result CAD_CALL cad_constraint_array_get_borrowed(cad_constraint_array array, size_t index, cad_constraint* out_constraint);
CAD_API cad_result CAD_CALL cad_sketch_solve(cad_object sketch, cad_sketch_solve_status* out_status);

/* ============================================================================
   Shapes and subshape provenance
   ========================================================================== */

CAD_API void CAD_CALL cad_shape_destroy(cad_shape shape);
CAD_API cad_result CAD_CALL cad_object_get_shape(cad_object object, cad_shape* out_shape);
CAD_API cad_result CAD_CALL cad_shape_get_owner_object_id(cad_shape shape, cad_uuid* out_owner_id);
CAD_API cad_result CAD_CALL cad_shape_get_revision(cad_shape shape, cad_revision* out_revision);
CAD_API cad_result CAD_CALL cad_shape_get_topology_type(cad_shape shape, cad_topology_type* out_type);
CAD_API cad_result CAD_CALL cad_shape_get_bounding_box(cad_shape shape, cad_box3* out_box);
CAD_API cad_result CAD_CALL cad_shape_is_null(cad_shape shape, bool* out_is_null);
CAD_API cad_result CAD_CALL cad_shape_is_valid(cad_shape shape, bool* out_is_valid);
CAD_API cad_result CAD_CALL cad_shape_get_subshapes(cad_shape shape, cad_topology_type type, cad_subshape_array* out_subshapes);
CAD_API void CAD_CALL cad_subshape_array_destroy(cad_subshape_array array);
CAD_API cad_result CAD_CALL cad_subshape_array_count(cad_subshape_array array, size_t* out_count);
CAD_API cad_result CAD_CALL cad_subshape_array_get(cad_subshape_array array, size_t index, cad_subshape_ref* out_ref);
CAD_API cad_result CAD_CALL cad_shape_get_subshape_bounding_box(cad_shape shape, const cad_subshape_ref* subshape, cad_box3* out_box);
CAD_API cad_result CAD_CALL cad_shape_get_subshape_centroid(cad_shape shape, const cad_subshape_ref* subshape, cad_vec3* out_centroid);
CAD_API cad_result CAD_CALL cad_shape_resolve_subshape(cad_shape shape, const cad_subshape_ref* subshape, cad_shape* out_subshape);
CAD_API cad_result CAD_CALL cad_shape_get_subshape_status(cad_shape shape, const cad_subshape_ref* subshape, cad_subshape_status* out_status);
CAD_API cad_result CAD_CALL cad_shape_get_generated_subshapes(cad_shape shape, cad_uuid feature_id, cad_topology_type type, cad_subshape_array* out_subshapes);
CAD_API cad_result CAD_CALL cad_shape_get_modified_subshapes(cad_shape shape, const cad_subshape_ref* source_subshape, cad_subshape_array* out_subshapes);
CAD_API cad_result CAD_CALL cad_shape_get_ancestor_subshapes(cad_shape shape, const cad_subshape_ref* subshape, cad_subshape_array* out_ancestors);
CAD_API cad_result CAD_CALL cad_shape_get_descendant_subshapes(cad_shape shape, const cad_subshape_ref* subshape, cad_subshape_array* out_descendants);

/* ============================================================================
   Tessellation and render buffers
   ========================================================================== */

typedef enum cad_mesh_flags_t {
    CAD_MESH_FLAG_NONE = 0,
    CAD_MESH_FLAG_INCLUDE_NORMALS = 1 << 0,
    CAD_MESH_FLAG_INCLUDE_UVS = 1 << 1,
    CAD_MESH_FLAG_INCLUDE_FACE_IDS = 1 << 2,
    CAD_MESH_FLAG_INCLUDE_EDGE_LINES = 1 << 3,
    CAD_MESH_FLAG_WELD_VERTICES = 1 << 4
} cad_mesh_flags;

typedef struct cad_mesh_desc_t {
    uint32_t struct_size;
    uint32_t flags;
    double linear_deflection;
    double angular_deflection;
    uint32_t reserved0;
    uint32_t reserved1;
} cad_mesh_desc;

typedef enum cad_index_type_t {
    CAD_INDEX_UINT16 = 0,
    CAD_INDEX_UINT32 = 1
} cad_index_type;

typedef struct cad_mesh_buffers_t {
    uint32_t struct_size;
    uint32_t reserved0;

    const float* positions;      /* xyz, vertex_count * 3 */
    const float* normals;        /* xyz, vertex_count * 3, optional */
    const float* uvs;            /* uv, vertex_count * 2, optional */
    const void* indices;         /* uint16_t or uint32_t triangle indices */
    const uint32_t* face_ids;    /* per triangle, triangle_count, optional */

    size_t vertex_count;
    size_t index_count;
    size_t triangle_count;

    const float* edge_positions; /* xyz pairs, optional */
    size_t edge_vertex_count;

    uint32_t position_stride_bytes;
    uint32_t normal_stride_bytes;
    uint32_t uv_stride_bytes;
    cad_index_type index_type;
} cad_mesh_buffers;

CAD_API cad_result CAD_CALL cad_shape_tessellate(cad_shape shape, const cad_mesh_desc* desc, cad_mesh* out_mesh);
CAD_API void CAD_CALL cad_mesh_destroy(cad_mesh mesh);
CAD_API cad_result CAD_CALL cad_mesh_get_buffers(cad_mesh mesh, cad_mesh_buffers* out_buffers);
CAD_API cad_result CAD_CALL cad_mesh_get_bounding_box(cad_mesh mesh, cad_box3* out_box);
CAD_API cad_result CAD_CALL cad_mesh_get_triangle_subshape(cad_mesh mesh, size_t triangle_index, cad_subshape_ref* out_face);

/* ============================================================================
   Selection and picking
   ========================================================================== */

typedef struct cad_pick_result_t {
    uint32_t struct_size;
    uint32_t triangle_index;

    cad_uuid object_id;
    cad_subshape_ref subshape;

    double distance;
    cad_vec3 position;
    cad_vec3 normal;
} cad_pick_result;

CAD_API cad_result CAD_CALL cad_document_pick(cad_document document, cad_ray ray, cad_pick_result* out_pick);
CAD_API cad_result CAD_CALL cad_snapshot_pick(cad_snapshot snapshot, cad_ray ray, cad_pick_result* out_pick);
CAD_API cad_result CAD_CALL cad_selection_create(cad_document document, cad_selection* out_selection);
CAD_API void CAD_CALL cad_selection_destroy(cad_selection selection);
CAD_API cad_result CAD_CALL cad_selection_clear(cad_selection selection);
CAD_API cad_result CAD_CALL cad_selection_add_object(cad_selection selection, cad_object object);
CAD_API cad_result CAD_CALL cad_selection_add_subshape(cad_selection selection, const cad_subshape_ref* subshape);
CAD_API cad_result CAD_CALL cad_selection_get_objects(cad_selection selection, cad_object_array* out_objects);
CAD_API cad_result CAD_CALL cad_selection_get_subshapes(cad_selection selection, cad_subshape_array* out_subshapes);

/* ============================================================================
   Snapshots
   ========================================================================== */

CAD_API cad_result CAD_CALL cad_document_create_snapshot(cad_document document, cad_snapshot* out_snapshot);
CAD_API cad_result CAD_CALL cad_snapshot_get_revision(cad_snapshot snapshot, cad_revision* out_revision);
CAD_API void CAD_CALL cad_snapshot_destroy(cad_snapshot snapshot);
CAD_API cad_result CAD_CALL cad_snapshot_get_object_shape(cad_snapshot snapshot, cad_uuid object_id, cad_shape* out_shape);
CAD_API cad_result CAD_CALL cad_snapshot_get_objects(cad_snapshot snapshot, cad_object_array* out_objects);

/* ============================================================================
   Import / export
   ========================================================================== */

typedef enum cad_file_format_t {
    CAD_FILE_FORMAT_NATIVE = 0,
    CAD_FILE_FORMAT_STEP = 1,
    CAD_FILE_FORMAT_IGES = 2,
    CAD_FILE_FORMAT_BREP = 3,
    CAD_FILE_FORMAT_STL = 4,
    CAD_FILE_FORMAT_OBJ = 5,
    CAD_FILE_FORMAT_GLTF = 6,
    CAD_FILE_FORMAT_3MF = 7,
    CAD_FILE_FORMAT_DXF = 8
} cad_file_format;

typedef enum cad_import_flags_t {
    CAD_IMPORT_NONE = 0,
    CAD_IMPORT_HEAL_SHAPES = 1 << 0,
    CAD_IMPORT_SEW_FACES = 1 << 1,
    CAD_IMPORT_PRESERVE_COLORS = 1 << 2,
    CAD_IMPORT_PRESERVE_NAMES = 1 << 3,
    CAD_IMPORT_AS_SINGLE_BODY = 1 << 4
} cad_import_flags;

typedef enum cad_export_flags_t {
    CAD_EXPORT_NONE = 0,
    CAD_EXPORT_SELECTED_ONLY = 1 << 0,
    CAD_EXPORT_INCLUDE_HIDDEN = 1 << 1,
    CAD_EXPORT_BINARY = 1 << 2,
    CAD_EXPORT_ASCII = 1 << 3
} cad_export_flags;

CAD_API cad_result CAD_CALL cad_document_import_file(cad_document document, cad_file_format format, cad_string_view path, uint32_t import_flags, cad_object_array* out_imported_objects);
CAD_API cad_result CAD_CALL cad_document_import_bytes(cad_document document, cad_file_format format, cad_bytes_view bytes, uint32_t import_flags, cad_object_array* out_imported_objects);
CAD_API cad_result CAD_CALL cad_document_export_file(cad_document document, cad_file_format format, cad_string_view path, uint32_t export_flags);
CAD_API cad_result CAD_CALL cad_document_export_bytes(cad_document document, cad_file_format format, uint32_t export_flags, cad_bytes* out_bytes);
CAD_API cad_result CAD_CALL cad_shape_export_file(cad_shape shape, cad_file_format format, cad_string_view path, uint32_t export_flags);
CAD_API cad_result CAD_CALL cad_shape_export_bytes(cad_shape shape, cad_file_format format, uint32_t export_flags, cad_bytes* out_bytes);

/* ============================================================================
   Diagnostics
   ========================================================================== */

typedef enum cad_diagnostic_code_t {
    CAD_DIAG_NONE = 0,
    CAD_DIAG_GENERAL = 1,
    CAD_DIAG_INVALID_ARGUMENT = 2,
    CAD_DIAG_MODELING_FAILED = 3,
    CAD_DIAG_RECOMPUTE_FAILED = 4,
    CAD_DIAG_REFERENCE_LOST = 5,
    CAD_DIAG_SKETCH_UNDER_CONSTRAINED = 6,
    CAD_DIAG_SKETCH_OVER_CONSTRAINED = 7,
    CAD_DIAG_BOOLEAN_FAILED = 8,
    CAD_DIAG_FILLET_RADIUS_TOO_LARGE = 9,
    CAD_DIAG_IMPORT_WARNING = 10,
    CAD_DIAG_MISSING_PLUGIN = 11,
    CAD_DIAG_TOPOLOGY_REPLACED = 12,
    CAD_DIAG_TOPOLOGY_SPLIT = 13,
    CAD_DIAG_TOPOLOGY_MERGED = 14
} cad_diagnostic_code;

typedef struct cad_diagnostic_t {
    uint32_t struct_size;
    cad_severity severity;
    cad_diagnostic_code code;
    uint32_t reserved0;
    cad_uuid object_id;
    cad_uuid related_id;
    cad_string message;
} cad_diagnostic;

CAD_API void CAD_CALL cad_diagnostic_array_destroy(cad_diagnostic_array array);
CAD_API cad_result CAD_CALL cad_diagnostic_array_count(cad_diagnostic_array array, size_t* out_count);
CAD_API cad_result CAD_CALL cad_diagnostic_array_get(cad_diagnostic_array array, size_t index, cad_diagnostic* out_diagnostic);
CAD_API cad_result CAD_CALL cad_document_get_diagnostics(cad_document document, cad_diagnostic_array* out_diagnostics);
CAD_API cad_result CAD_CALL cad_object_get_diagnostics(cad_object object, cad_diagnostic_array* out_diagnostics);

/* ============================================================================
   Metadata / custom properties
   ========================================================================== */

CAD_API cad_result CAD_CALL cad_object_set_property(cad_object object, cad_string_view key, cad_string_view value);
CAD_API cad_result CAD_CALL cad_object_get_property(cad_object object, cad_string_view key, cad_string* out_value);
CAD_API cad_result CAD_CALL cad_object_remove_property(cad_object object, cad_string_view key);
CAD_API cad_result CAD_CALL cad_object_get_property_keys(cad_object object, cad_string_array* out_keys);

/* ============================================================================
   Cancellation / progress
   ========================================================================== */

typedef struct cad_progress_t cad_progress;
typedef bool (CAD_CALL *cad_cancel_fn)(void* user_data);
typedef void (CAD_CALL *cad_progress_fn)(void* user_data, double fraction, cad_string_view message);

struct cad_progress_t {
    uint32_t struct_size;
    uint32_t reserved0;
    void* user_data;
    cad_cancel_fn cancel;
    cad_progress_fn progress;
};

CAD_API cad_result CAD_CALL cad_context_set_progress_callback(cad_context context, const cad_progress* progress);

/* ============================================================================
   Plugin ABI
   ========================================================================== */

typedef struct cad_feature_descriptor_t {
    uint32_t struct_size;
    uint32_t reserved0;

    cad_string_view type_name;
    cad_string_view display_name;
    cad_string_view description;
    cad_string_view parameter_schema_json;

    uint32_t version_major;
    uint32_t version_minor;
    uint32_t version_patch;
    uint32_t reserved1;
} cad_feature_descriptor;

typedef cad_result (CAD_CALL *cad_plugin_feature_eval_fn)(cad_document document, cad_object feature, void* user_data);

typedef struct cad_plugin_api_t {
    uint32_t struct_size;
    uint32_t abi_major;
    uint32_t abi_minor;
    uint32_t abi_patch;

    cad_result (CAD_CALL *register_feature_type)(
        cad_context context,
        const cad_feature_descriptor* descriptor,
        cad_plugin_feature_eval_fn eval_fn,
        void* user_data
    );

    cad_result (CAD_CALL *set_last_error)(cad_context context, cad_string_view message);
} cad_plugin_api;

typedef cad_result (CAD_CALL *cad_plugin_init_fn)(cad_context context, const cad_plugin_api* api);
typedef void (CAD_CALL *cad_plugin_shutdown_fn)(cad_context context);

CAD_API cad_result CAD_CALL cad_context_load_plugin(cad_context context, cad_string_view path);
CAD_API cad_result CAD_CALL cad_context_unload_plugin(cad_context context, cad_string_view path);

/* ============================================================================
   ABI extension mechanism
   ========================================================================== */

/*
    Allows future optional interfaces without breaking the released core ABI.
    Implementations may return CAD_ERROR_NOT_FOUND for unknown names.

    Example names:
      "cad.geometry.v1"
      "cad.sketch.v1"
      "cad.sheet_metal.v1"
      "cad.cam.v1"
      "cad.drawings.v1"
      "cad.render.metal.v1"
*/
CAD_API cad_result CAD_CALL cad_context_get_interface(
    cad_context context,
    cad_string_view interface_name,
    uint32_t interface_version,
    void** out_interface
);

/* ============================================================================
   Utility constructors for common cad_param values
   ========================================================================== */

static inline cad_string_view cad_sv(const char* c_string) {
    cad_string_view v;
    const char* p = c_string;
    size_t n = 0;
    if (p) {
        while (p[n] != '\0') { ++n; }
    }
    v.data = c_string;
    v.size = n;
    return v;
}

static inline cad_param cad_param_zero(cad_string_view name, cad_param_type type) {
    cad_param p;
    p.struct_size = CAD_STRUCT_SIZE(cad_param);
    p.type = type;
    p.name = name;
    p.value.int_value = 0;
    p.length_unit = CAD_LENGTH_UNIT_MILLIMETER;
    p.angle_unit = CAD_ANGLE_UNIT_RADIAN;
    p.reserved0 = 0;
    return p;
}

static inline cad_param cad_param_bool(cad_string_view name, bool value) {
    cad_param p = cad_param_zero(name, CAD_PARAM_BOOL);
    p.value.bool_value = value;
    return p;
}

static inline cad_param cad_param_int(cad_string_view name, int64_t value) {
    cad_param p = cad_param_zero(name, CAD_PARAM_INT);
    p.value.int_value = value;
    return p;
}

static inline cad_param cad_param_double(cad_string_view name, double value) {
    cad_param p = cad_param_zero(name, CAD_PARAM_DOUBLE);
    p.value.double_value = value;
    return p;
}

static inline cad_param cad_param_length(cad_string_view name, double value, cad_length_unit unit) {
    cad_param p = cad_param_zero(name, CAD_PARAM_LENGTH);
    p.value.double_value = value;
    p.length_unit = unit;
    return p;
}

static inline cad_param cad_param_angle(cad_string_view name, double value, cad_angle_unit unit) {
    cad_param p = cad_param_zero(name, CAD_PARAM_ANGLE);
    p.value.double_value = value;
    p.angle_unit = unit;
    return p;
}

static inline cad_param cad_param_string(cad_string_view name, cad_string_view value) {
    cad_param p = cad_param_zero(name, CAD_PARAM_STRING);
    p.value.string_value = value;
    return p;
}

static inline cad_param cad_param_vec2(cad_string_view name, cad_vec2 value) {
    cad_param p = cad_param_zero(name, CAD_PARAM_VEC2);
    p.value.vec2_value = value;
    return p;
}

static inline cad_param cad_param_vec3(cad_string_view name, cad_vec3 value) {
    cad_param p = cad_param_zero(name, CAD_PARAM_VEC3);
    p.value.vec3_value = value;
    return p;
}

static inline cad_param cad_param_object(cad_string_view name, cad_object value) {
    cad_param p = cad_param_zero(name, CAD_PARAM_OBJECT);
    p.value.object_value = value;
    return p;
}

static inline cad_param cad_param_subshape(cad_string_view name, cad_subshape_ref value) {
    cad_param p = cad_param_zero(name, CAD_PARAM_SUBSHAPE);
    p.value.subshape_value = value;
    return p;
}

#ifdef __cplusplus
}
#endif

#endif /* CAD_ABI_H */
