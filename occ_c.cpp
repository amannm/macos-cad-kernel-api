#include "occ_c.h"

#if __has_include(<Standard_Version.hxx>)
#include <Standard_Version.hxx>
#endif

#include <BinTools.hxx>
#include <Bnd_Box.hxx>
#include <BOPAlgo_GlueEnum.hxx>
#include <BOPAlgo_Operation.hxx>
#include <BRepAlgoAPI_BooleanOperation.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_GTransform.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeSolid.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepGProp.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepPrimAPI_MakeRevol.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRepPrimAPI_MakeTorus.hxx>
#include <BRepTools.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <Geom2dAdaptor_Curve.hxx>
#include <GeomAPI_ProjectPointOnCurve.hxx>
#include <GeomAPI_ProjectPointOnSurf.hxx>
#include <GeomAdaptor_Curve.hxx>
#include <GeomAdaptor_Surface.hxx>
#include <Geom_Circle.hxx>
#include <Geom_Curve.hxx>
#include <Geom_CylindricalSurface.hxx>
#include <Geom_Line.hxx>
#include <Geom_Plane.hxx>
#include <Geom_SphericalSurface.hxx>
#include <Geom_Surface.hxx>
#include <GProp_GProps.hxx>
#include <IMeshTools_Parameters.hxx>
#include <NCollection_IndexedMap.hxx>
#include <NCollection_List.hxx>
#include <Poly_Triangle.hxx>
#include <Poly_Triangulation.hxx>
#include <Precision.hxx>
#include <ShapeFix_Shape.hxx>
#include <Standard_Failure.hxx>
#include <Standard_Handle.hxx>
#include <TopAbs_Orientation.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Iterator.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Shell.hxx>
#include <TopTools_FormatVersion.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <gp_Ax1.hxx>
#include <gp_Ax2.hxx>
#include <gp_Ax3.hxx>
#include <gp_Dir.hxx>
#include <gp_GTrsf.hxx>
#include <gp_Mat.hxx>
#include <gp_Pnt.hxx>
#include <gp_Pnt2d.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <fstream>
#include <ios>
#include <limits>
#include <new>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {
    constexpr uint32_t OCC_MAGIC = 0x4f434343u;
    constexpr uint32_t OCC_DEAD = 0x4f434358u;

    struct occ_header_t {
        uint32_t magic;
        occ_handle_kind_t kind;
        std::atomic_size_t refs;

        explicit occ_header_t(occ_handle_kind_t k) : magic(OCC_MAGIC), kind(k), refs(1) {
        }
    };

    struct diagnostic_t {
        occ_log_level_t level;
        std::string message;
    };
}

struct occ_context_t {
    occ_log_callback_t log = nullptr;
    void *log_user = nullptr;
    std::string last_error;
    std::vector<diagnostic_t> diagnostics;

    void reset() {
        last_error.clear();
        diagnostics.clear();
    }

    occ_status_t fail(occ_status_t status, const char *message) {
        last_error = message != nullptr ? message : occ_status_string(status);
        diagnostics.push_back({OCC_LOG_ERROR, last_error});
        if (log != nullptr) {
            log(OCC_LOG_ERROR, last_error.c_str(), log_user);
        }
        return status;
    }
};

struct occ_curve2d_t {
    occ_header_t h;
    occ::handle<Geom2d_Curve> curve;

    explicit occ_curve2d_t(const occ::handle<Geom2d_Curve> &c) : h(OCC_HANDLE_CURVE2D), curve(c) {
    }
};

struct occ_curve3d_t {
    occ_header_t h;
    occ::handle<Geom_Curve> curve;

    explicit occ_curve3d_t(const occ::handle<Geom_Curve> &c) : h(OCC_HANDLE_CURVE3D), curve(c) {
    }
};

struct occ_surface_t {
    occ_header_t h;
    occ::handle<Geom_Surface> surface;

    explicit occ_surface_t(const occ::handle<Geom_Surface> &s) : h(OCC_HANDLE_SURFACE), surface(s) {
    }
};

struct occ_shape_t {
    occ_header_t h;
    TopoDS_Shape shape;

    explicit occ_shape_t(TopoDS_Shape s) : h(OCC_HANDLE_SHAPE), shape(std::move(s)) {
    }
};

struct occ_mesh_t {
    occ_header_t h;
    std::vector<occ_vec3_t> positions;
    std::vector<occ_vec3_t> normals;
    std::vector<occ_vec2_t> uvs;
    std::vector<occ_triangle_t> triangles;

    occ_mesh_t() : h(OCC_HANDLE_MESH) {
    }
};

namespace {
    occ_status_t fail(occ_context_t *c, occ_status_t status, const char *message) {
        return c != nullptr ? c->fail(status, message) : status;
    }

    template<class F>
    occ_status_t guard(occ_context_t *c, F &&f) noexcept {
        try {
            if (c != nullptr) {
                c->reset();
            }
            return f();
        } catch (const std::bad_alloc &) {
            return fail(c, OCC_OUT_OF_MEMORY, "out of memory");
        } catch (const Standard_Failure &e) {
            return fail(c, OCC_OCCT_EXCEPTION, e.what());
        } catch (const std::exception &e) {
            return fail(c, OCC_ERROR, e.what());
        } catch (...) {
            return fail(c, OCC_ERROR, "unknown exception");
        }
    }

    const occ_header_t *header_of(const void *p) {
        return static_cast<const occ_header_t *>(p);
    }

    occ_header_t *header_of(void *p) {
        return static_cast<occ_header_t *>(p);
    }

    occ_status_t check_kind(const void *p, occ_handle_kind_t kind) {
        if (p == nullptr) {
            return OCC_NULL_ARGUMENT;
        }
        const occ_header_t *h = header_of(p);
        if (h->magic != OCC_MAGIC) {
            return OCC_BAD_HANDLE;
        }
        if (h->kind != kind) {
            return OCC_BAD_HANDLE_TYPE;
        }
        return OCC_OK;
    }

    template<class T>
    occ_status_t get_handle(const T *p, occ_handle_kind_t kind, const T **out) {
        const occ_status_t s = check_kind(p, kind);
        if (s != OCC_OK) {
            return s;
        }
        *out = p;
        return OCC_OK;
    }

    template<class T>
    occ_status_t get_handle(T *p, const occ_handle_kind_t kind, T **out) {
        const occ_status_t s = check_kind(p, kind);
        if (s != OCC_OK) {
            return s;
        }
        *out = p;
        return OCC_OK;
    }

    gp_Pnt point(occ_vec3_t v) {
        return {v.x, v.y, v.z};
    }

    gp_Vec vector(occ_vec3_t v) {
        return {v.x, v.y, v.z};
    }

    gp_Dir direction(occ_vec3_t v) {
        return {v.x, v.y, v.z};
    }

    gp_Ax1 axis1(occ_axis1_t a) {
        return {point(a.origin), direction(a.direction)};
    }

    gp_Ax2 axis2(occ_axis2_t a) {
        return {point(a.origin), direction(a.z_direction), direction(a.x_direction)};
    }

    gp_Ax3 axis3(occ_axis2_t a) {
        return {axis2(a)};
    }

    occ_vec2_t vec2(const gp_Pnt2d &p) {
        return {p.X(), p.Y()};
    }

    occ_vec2_t vec2(const gp_Vec2d &v) {
        return {v.X(), v.Y()};
    }

    occ_vec3_t vec3(const gp_Pnt &p) {
        return {p.X(), p.Y(), p.Z()};
    }

    occ_vec3_t vec3(const gp_Vec &v) {
        return {v.X(), v.Y(), v.Z()};
    }

    occ_vec3_t vec3(const gp_Dir &v) {
        return {v.X(), v.Y(), v.Z()};
    }

    gp_Trsf trsf(occ_mat4_t m) {
        if (m.m[12] != 0.0 || m.m[13] != 0.0 || m.m[14] != 0.0 || m.m[15] != 1.0) {
            throw Standard_ConstructionError("non-affine transform");
        }
        gp_Trsf t;
        t.SetValues(m.m[0], m.m[1], m.m[2], m.m[3],
                    m.m[4], m.m[5], m.m[6], m.m[7],
                    m.m[8], m.m[9], m.m[10], m.m[11]);
        return t;
    }

    gp_GTrsf gtrsf(occ_mat4_t m) {
        if (m.m[12] != 0.0 || m.m[13] != 0.0 || m.m[14] != 0.0 || m.m[15] != 1.0) {
            throw Standard_ConstructionError("non-affine transform");
        }
        gp_GTrsf t;
        t.SetValue(1, 1, m.m[0]);
        t.SetValue(1, 2, m.m[1]);
        t.SetValue(1, 3, m.m[2]);
        t.SetValue(1, 4, m.m[3]);
        t.SetValue(2, 1, m.m[4]);
        t.SetValue(2, 2, m.m[5]);
        t.SetValue(2, 3, m.m[6]);
        t.SetValue(2, 4, m.m[7]);
        t.SetValue(3, 1, m.m[8]);
        t.SetValue(3, 2, m.m[9]);
        t.SetValue(3, 3, m.m[10]);
        t.SetValue(3, 4, m.m[11]);
        return t;
    }

    occ_mat4_t mat4(const gp_Trsf &t) {
        occ_mat4_t m{};
        m.m[0] = t.Value(1, 1);
        m.m[1] = t.Value(1, 2);
        m.m[2] = t.Value(1, 3);
        m.m[3] = t.Value(1, 4);
        m.m[4] = t.Value(2, 1);
        m.m[5] = t.Value(2, 2);
        m.m[6] = t.Value(2, 3);
        m.m[7] = t.Value(2, 4);
        m.m[8] = t.Value(3, 1);
        m.m[9] = t.Value(3, 2);
        m.m[10] = t.Value(3, 3);
        m.m[11] = t.Value(3, 4);
        m.m[12] = 0.0;
        m.m[13] = 0.0;
        m.m[14] = 0.0;
        m.m[15] = 1.0;
        return m;
    }

    occ_shape_type_t shape_type(TopAbs_ShapeEnum t) {
        return static_cast<occ_shape_type_t>(t);
    }

    TopAbs_ShapeEnum shape_type(occ_shape_type_t t) {
        switch (t) {
            case OCC_SHAPE_COMPOUND: return TopAbs_COMPOUND;
            case OCC_SHAPE_COMPSOLID: return TopAbs_COMPSOLID;
            case OCC_SHAPE_SOLID: return TopAbs_SOLID;
            case OCC_SHAPE_SHELL: return TopAbs_SHELL;
            case OCC_SHAPE_FACE: return TopAbs_FACE;
            case OCC_SHAPE_WIRE: return TopAbs_WIRE;
            case OCC_SHAPE_EDGE: return TopAbs_EDGE;
            case OCC_SHAPE_VERTEX: return TopAbs_VERTEX;
            case OCC_SHAPE_UNKNOWN: return TopAbs_SHAPE;
        }
        return TopAbs_SHAPE;
    }

    occ_orientation_t orientation(TopAbs_Orientation o) {
        return static_cast<occ_orientation_t>(o);
    }

    BOPAlgo_GlueEnum glue(occ_boolean_glue_t g) {
        switch (g) {
            case OCC_BOOLEAN_GLUE_OFF: return BOPAlgo_GlueOff;
            case OCC_BOOLEAN_GLUE_SHIFT: return BOPAlgo_GlueShift;
            case OCC_BOOLEAN_GLUE_FULL: return BOPAlgo_GlueFull;
        }
        return BOPAlgo_GlueOff;
    }

    BOPAlgo_Operation boolean_operation(occ_boolean_operation_t op) {
        switch (op) {
            case OCC_BOOLEAN_FUSE: return BOPAlgo_FUSE;
            case OCC_BOOLEAN_CUT: return BOPAlgo_CUT;
            case OCC_BOOLEAN_COMMON: return BOPAlgo_COMMON;
            case OCC_BOOLEAN_SECTION: return BOPAlgo_SECTION;
        }
        return BOPAlgo_UNKNOWN;
    }

    occ_curve_type_t curve_type(GeomAbs_CurveType t) {
        switch (t) {
            case GeomAbs_Line: return OCC_CURVE_LINE;
            case GeomAbs_Circle: return OCC_CURVE_CIRCLE;
            case GeomAbs_Ellipse: return OCC_CURVE_ELLIPSE;
            case GeomAbs_BezierCurve: return OCC_CURVE_BEZIER;
            case GeomAbs_BSplineCurve: return OCC_CURVE_BSPLINE;
            case GeomAbs_OffsetCurve: return OCC_CURVE_OFFSET;
            case GeomAbs_Hyperbola:
            case GeomAbs_Parabola:
            case GeomAbs_OtherCurve: return OCC_CURVE_OTHER;
        }
        return OCC_CURVE_UNKNOWN;
    }

    occ_surface_type_t surface_type(GeomAbs_SurfaceType t) {
        switch (t) {
            case GeomAbs_Plane: return OCC_SURFACE_PLANE;
            case GeomAbs_Cylinder: return OCC_SURFACE_CYLINDER;
            case GeomAbs_Cone: return OCC_SURFACE_CONE;
            case GeomAbs_Sphere: return OCC_SURFACE_SPHERE;
            case GeomAbs_Torus: return OCC_SURFACE_TORUS;
            case GeomAbs_BezierSurface: return OCC_SURFACE_BEZIER;
            case GeomAbs_BSplineSurface: return OCC_SURFACE_BSPLINE;
            case GeomAbs_SurfaceOfRevolution: return OCC_SURFACE_REVOLUTION;
            case GeomAbs_SurfaceOfExtrusion: return OCC_SURFACE_EXTRUSION;
            case GeomAbs_OffsetSurface: return OCC_SURFACE_OFFSET;
            case GeomAbs_OtherSurface: return OCC_SURFACE_OTHER;
        }
        return OCC_SURFACE_UNKNOWN;
    }

    template<class T>
    occ_status_t put_handle(T **out, T *value) {
        if (out == nullptr) {
            delete value;
            return OCC_NULL_ARGUMENT;
        }
        *out = value;
        return OCC_OK;
    }

    occ_status_t put_shape(occ_shape_t **out, const TopoDS_Shape &shape) {
        if (shape.IsNull()) {
            return OCC_EMPTY_RESULT;
        }
        return put_handle(out, new occ_shape_t(shape));
    }

    occ_status_t put_curve3d(occ_curve3d_t **out, const occ::handle<Geom_Curve> &curve) {
        if (curve.IsNull()) {
            return OCC_EMPTY_RESULT;
        }
        return put_handle(out, new occ_curve3d_t(curve));
    }

    occ_status_t put_curve2d(occ_curve2d_t **out, const occ::handle<Geom2d_Curve> &curve) {
        if (curve.IsNull()) {
            return OCC_EMPTY_RESULT;
        }
        return put_handle(out, new occ_curve2d_t(curve));
    }

    occ_status_t put_surface(occ_surface_t **out, const occ::handle<Geom_Surface> &surface) {
        if (surface.IsNull()) {
            return OCC_EMPTY_RESULT;
        }
        return put_handle(out, new occ_surface_t(surface));
    }

    occ_status_t check_context(occ_context_t *c) {
        return c == nullptr ? OCC_NULL_ARGUMENT : OCC_OK;
    }

    occ_status_t require_shape(const occ_shape_t *p, const occ_shape_t **out) {
        return get_handle(p, OCC_HANDLE_SHAPE, out);
    }

    occ_status_t require_curve3d(const occ_curve3d_t *p, const occ_curve3d_t **out) {
        return get_handle(p, OCC_HANDLE_CURVE3D, out);
    }

    occ_status_t require_curve2d(const occ_curve2d_t *p, const occ_curve2d_t **out) {
        return get_handle(p, OCC_HANDLE_CURVE2D, out);
    }

    occ_status_t require_surface(const occ_surface_t *p, const occ_surface_t **out) {
        return get_handle(p, OCC_HANDLE_SURFACE, out);
    }

    occ_status_t require_mesh(const occ_mesh_t *p, const occ_mesh_t **out) {
        return get_handle(p, OCC_HANDLE_MESH, out);
    }

    occ_status_t expect_shape_kind(const occ_shape_t *s, const TopAbs_ShapeEnum t) {
        if (s->shape.IsNull()) {
            return OCC_INVALID_SHAPE;
        }
        return s->shape.ShapeType() == t ? OCC_OK : OCC_BAD_HANDLE_TYPE;
    }

    NCollection_List<TopoDS_Shape> shape_list(const occ_shape_t *const*shapes, const size_t count, occ_status_t &status) {
        NCollection_List<TopoDS_Shape> list;
        if (count != 0 && shapes == nullptr) {
            status = OCC_NULL_ARGUMENT;
            return list;
        }
        for (size_t i = 0; i < count; ++i) {
            const occ_shape_t *s = nullptr;
            status = require_shape(shapes[i], &s);
            if (status != OCC_OK) {
                return list;
            }
            if (s->shape.IsNull()) {
                status = OCC_INVALID_SHAPE;
                return list;
            }
            list.Append(s->shape);
        }
        status = OCC_OK;
        return list;
    }

    std::vector<TopoDS_Shape> collect_subshapes(const TopoDS_Shape &shape, TopAbs_ShapeEnum type,
                                                occ_traversal_t traversal) {
        std::vector<TopoDS_Shape> out;
        if (traversal == OCC_TRAVERSE_DIRECT) {
            for (TopoDS_Iterator it(shape); it.More(); it.Next()) {
                const TopoDS_Shape &current = it.Value();
                if (!current.IsNull() && current.ShapeType() == type) {
                    out.push_back(current);
                }
            }
            return out;
        }
        if (traversal == OCC_TRAVERSE_RECURSIVE) {
            for (TopExp_Explorer ex(shape, type); ex.More(); ex.Next()) {
                out.push_back(ex.Current());
            }
            return out;
        }
        NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> map;
        TopExp::MapShapes(shape, type, map);
        for (int i = 1; i <= map.Extent(); ++i) {
            out.push_back(map.FindKey(i));
        }
        return out;
    }

    void mass_properties(const GProp_GProps &props, occ_mass_properties_t *out) {
        out->measure = props.Mass();
        out->center_of_mass = vec3(props.CentreOfMass());
        const gp_Mat m = props.MatrixOfInertia();
        for (int r = 1; r <= 3; ++r) {
            for (int c = 1; c <= 3; ++c) {
                out->inertia[(r - 1) * 3 + (c - 1)] = m.Value(r, c);
            }
        }
    }

    void apply_options(BRepAlgoAPI_BooleanOperation &op, const occ_boolean_options_t *options) {
        const occ_boolean_options_t defaults = occ_boolean_options_default();
        const occ_boolean_options_t &o = options != nullptr ? *options : defaults;
        op.SetFuzzyValue(o.fuzzy_value);
        op.SetRunParallel(o.run_parallel != 0);
        op.SetNonDestructive(o.non_destructive != 0);
        op.SetGlue(glue(o.glue));
        op.SetCheckInverted(o.check_inverted != 0);
    }

    occ_status_t finish_boolean(occ_context_t *c, BRepAlgoAPI_BooleanOperation &op, occ_shape_t **out) {
        op.Build();
        if (op.HasErrors()) {
            return fail(c, OCC_ERROR, "boolean operation failed");
        }
        return put_shape(out, op.Shape());
    }

    bool looks_binary_brep(const char *data, size_t size) {
        const size_t n = std::min<size_t>(size, 4096);
        for (size_t i = 0; i < n; ++i) {
            const auto c = static_cast<unsigned char>(data[i]);
            if (c == 0 || (c < 9) || (c > 13 && c < 32)) {
                return true;
            }
        }
        return false;
    }

    occ_status_t read_brep_buffer(const void *data, size_t size, occ_shape_t **out) {
        if (data == nullptr || out == nullptr) {
            return OCC_NULL_ARGUMENT;
        }
        *out = nullptr;
        if (size == 0) {
            return OCC_INVALID_ARGUMENT;
        }
        const auto bytes = static_cast<const char *>(data);
        const std::string payload(bytes, bytes + size);
        std::istringstream stream(payload, std::ios::in | std::ios::binary);
        TopoDS_Shape shape;
        if (looks_binary_brep(bytes, size)) {
            BinTools::Read(shape, stream);
        } else {
            const BRep_Builder builder;
            BRepTools::Read(shape, stream, builder);
        }
        return shape.IsNull() ? OCC_PARSE_ERROR : put_shape(out, shape);
    }

    occ_status_t write_brep_buffer(const TopoDS_Shape &shape, occ_brep_format_t format, void **out_data,
                                   size_t *out_size) {
        if (out_data == nullptr || out_size == nullptr) {
            return OCC_NULL_ARGUMENT;
        }
        *out_data = nullptr;
        *out_size = 0;
        std::ostringstream stream(std::ios::out | std::ios::binary);
        stream.precision(15);
        if (format == OCC_BREP_ASCII) {
            BRepTools::Write(shape, stream, true, false, TopTools_FormatVersion_CURRENT);
        } else if (format == OCC_BREP_BINARY) {
            BinTools::Write(shape, stream, true, false, BinTools_FormatVersion_CURRENT);
        } else {
            return OCC_INVALID_ARGUMENT;
        }
        const std::string bytes = stream.str();
        if (!stream.good() || bytes.empty()) {
            return OCC_IO_ERROR;
        }
        void *memory = std::malloc(bytes.size());
        if (memory == nullptr) {
            return OCC_OUT_OF_MEMORY;
        }
        std::memcpy(memory, bytes.data(), bytes.size());
        *out_data = memory;
        *out_size = bytes.size();
        return OCC_OK;
    }

    occ_status_t read_file_bytes(const char *path, std::string &out) {
        if (path == nullptr) {
            return OCC_NULL_ARGUMENT;
        }
        std::ifstream stream(path, std::ios::in | std::ios::binary);
        if (!stream) {
            return OCC_IO_ERROR;
        }
        std::ostringstream buffer(std::ios::out | std::ios::binary);
        buffer << stream.rdbuf();
        if (!stream.good() && !stream.eof()) {
            return OCC_IO_ERROR;
        }
        out = buffer.str();
        return OCC_OK;
    }

    occ_mesh_packed_options_t default_mesh_packed_options() {
        occ_mesh_packed_options_t o{};
        o.vertex_format = OCC_MESH_VERTEX_FLOAT32_PNT;
        o.index_format = OCC_MESH_INDEX_UINT32;
        return o;
    }

    bool checked_mul_size(size_t a, size_t b, size_t &out) {
        if (a != 0 && b > std::numeric_limits<size_t>::max() / a) {
            return false;
        }
        out = a * b;
        return true;
    }

    size_t mesh_vertex_stride(occ_mesh_vertex_format_t format) {
        switch (format) {
            case OCC_MESH_VERTEX_FLOAT32_P: return sizeof(occ_mesh_vertex_float32_p_t);
            case OCC_MESH_VERTEX_FLOAT32_PN: return sizeof(occ_mesh_vertex_float32_pn_t);
            case OCC_MESH_VERTEX_FLOAT32_PNT: return sizeof(occ_mesh_vertex_float32_pnt_t);
        }
        return 0;
    }

    size_t mesh_index_stride(occ_mesh_index_format_t format) {
        switch (format) {
            case OCC_MESH_INDEX_UINT16: return sizeof(uint16_t);
            case OCC_MESH_INDEX_UINT32: return sizeof(uint32_t);
        }
        return 0;
    }

    occ_status_t make_mesh_packed_layout(occ_mesh_view_t view, const occ_mesh_packed_options_t *options_or_null,
                                         occ_mesh_packed_layout_t *out) {
        if (out == nullptr) {
            return OCC_NULL_ARGUMENT;
        }
        *out = {};
        const occ_mesh_packed_options_t defaults = default_mesh_packed_options();
        const occ_mesh_packed_options_t &options = options_or_null != nullptr ? *options_or_null : defaults;
        const size_t vertex_stride = mesh_vertex_stride(options.vertex_format);
        const size_t index_stride = mesh_index_stride(options.index_format);
        if (vertex_stride == 0 || index_stride == 0) {
            return OCC_INVALID_ARGUMENT;
        }
        if ((view.vertex_count != 0 && view.positions == nullptr) || (
                view.triangle_count != 0 && view.triangles == nullptr)) {
            return OCC_NULL_ARGUMENT;
        }
        if ((options.vertex_format == OCC_MESH_VERTEX_FLOAT32_PN || options.vertex_format ==
             OCC_MESH_VERTEX_FLOAT32_PNT) && view.vertex_count != 0 && view.normals == nullptr) {
            return OCC_NULL_ARGUMENT;
        }
        if (options.vertex_format == OCC_MESH_VERTEX_FLOAT32_PNT && view.vertex_count != 0 && view.uvs == nullptr) {
            return OCC_NULL_ARGUMENT;
        }
        size_t index_count = 0;
        size_t vertex_bytes = 0;
        size_t index_bytes = 0;
        if (!checked_mul_size(view.triangle_count, size_t(3), index_count) ||
            !checked_mul_size(view.vertex_count, vertex_stride, vertex_bytes) ||
            !checked_mul_size(index_count, index_stride, index_bytes)) {
            return OCC_INVALID_ARGUMENT;
        }
        for (size_t i = 0; i < view.triangle_count; ++i) {
            const occ_triangle_t t = view.triangles[i];
            if (t.i0 >= view.vertex_count || t.i1 >= view.vertex_count || t.i2 >= view.vertex_count) {
                return OCC_INVALID_ARGUMENT;
            }
            if (options.index_format == OCC_MESH_INDEX_UINT16 && (
                    t.i0 > UINT16_MAX || t.i1 > UINT16_MAX || t.i2 > UINT16_MAX)) {
                return OCC_INVALID_ARGUMENT;
            }
        }
        out->vertex_format = options.vertex_format;
        out->index_format = options.index_format;
        out->vertex_stride = vertex_stride;
        out->vertex_count = view.vertex_count;
        out->vertex_bytes = vertex_bytes;
        out->index_stride = index_stride;
        out->index_count = index_count;
        out->index_bytes = index_bytes;
        return OCC_OK;
    }

    void destroy_handle(void *handle) {
        occ_header_t *h = header_of(handle);
        h->magic = OCC_DEAD;
        switch (h->kind) {
            case OCC_HANDLE_CURVE2D: delete static_cast<occ_curve2d_t *>(handle);
                return;
            case OCC_HANDLE_CURVE3D: delete static_cast<occ_curve3d_t *>(handle);
                return;
            case OCC_HANDLE_SURFACE: delete static_cast<occ_surface_t *>(handle);
                return;
            case OCC_HANDLE_SHAPE: delete static_cast<occ_shape_t *>(handle);
                return;
            case OCC_HANDLE_MESH: delete static_cast<occ_mesh_t *>(handle);
                return;
            case OCC_HANDLE_UNKNOWN: return;
        }
    }
}

OCC_C_API occ_version_t OCC_C_CALL occ_version(void) {
    occ_version_t v{};
    v.abi_major = OCC_C_ABI_VERSION_MAJOR;
    v.abi_minor = OCC_C_ABI_VERSION_MINOR;
    v.abi_patch = OCC_C_ABI_VERSION_PATCH;
#ifdef OCC_VERSION_MAJOR
    v.occt_major = OCC_VERSION_MAJOR;
    v.occt_minor = OCC_VERSION_MINOR;
    v.occt_patch = OCC_VERSION_MAINTENANCE;
#endif
    return v;
}

OCC_C_API const char * OCC_C_CALL occ_status_string(occ_status_t status) {
    switch (status) {
        case OCC_OK: return "ok";
        case OCC_ERROR: return "error";
        case OCC_NULL_ARGUMENT: return "null argument";
        case OCC_INVALID_ARGUMENT: return "invalid argument";
        case OCC_OUT_OF_MEMORY: return "out of memory";
        case OCC_BAD_HANDLE: return "bad handle";
        case OCC_BAD_HANDLE_TYPE: return "bad handle type";
        case OCC_INDEX_OUT_OF_RANGE: return "index out of range";
        case OCC_EMPTY_RESULT: return "empty result";
        case OCC_INVALID_SHAPE: return "invalid shape";
        case OCC_NON_MANIFOLD: return "non manifold";
        case OCC_IO_ERROR: return "io error";
        case OCC_PARSE_ERROR: return "parse error";
        case OCC_UNSUPPORTED: return "unsupported";
        case OCC_OCCT_EXCEPTION: return "occt exception";
    }
    return "unknown status";
}

OCC_C_API void OCC_C_CALL occ_release(void *handle) {
    if (handle == nullptr) {
        return;
    }
    occ_header_t *h = header_of(handle);
    if (h->magic != OCC_MAGIC) {
        return;
    }
    if (h->refs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        destroy_handle(handle);
    }
}

OCC_C_API void OCC_C_CALL occ_retain(void *handle) {
    if (handle == nullptr) {
        return;
    }
    occ_header_t *h = header_of(handle);
    if (h->magic == OCC_MAGIC) {
        h->refs.fetch_add(1, std::memory_order_relaxed);
    }
}

OCC_C_API occ_handle_kind_t OCC_C_CALL occ_handle_kind(const void *handle) {
    if (handle == nullptr) {
        return OCC_HANDLE_UNKNOWN;
    }
    const occ_header_t *h = header_of(handle);
    return h->magic == OCC_MAGIC ? h->kind : OCC_HANDLE_UNKNOWN;
}

OCC_C_API void OCC_C_CALL occ_free(void *memory) {
    std::free(memory);
}

OCC_C_API occ_status_t OCC_C_CALL occ_context_create(occ_context_t **out_context) {
    return guard(nullptr, [&]() {
        if (out_context == nullptr) {
            return OCC_NULL_ARGUMENT;
        }
        *out_context = new occ_context_t;
        return OCC_OK;
    });
}

OCC_C_API void OCC_C_CALL occ_context_destroy(occ_context_t *context) {
    delete context;
}

OCC_C_API occ_status_t OCC_C_CALL occ_context_set_log_callback(occ_context_t *context, occ_log_callback_t callback,
                                                               void *user_data) {
    return guard(context, [&]() {
        const occ_status_t s = check_context(context);
        if (s != OCC_OK) {
            return s;
        }
        context->log = callback;
        context->log_user = user_data;
        return OCC_OK;
    });
}

OCC_C_API occ_status_t OCC_C_CALL occ_context_last_error(const occ_context_t *context, const char **out_message_utf8) {
    if (context == nullptr || out_message_utf8 == nullptr) {
        return OCC_NULL_ARGUMENT;
    }
    *out_message_utf8 = context->last_error.c_str();
    return OCC_OK;
}

OCC_C_API occ_status_t OCC_C_CALL occ_context_last_diagnostic_count(const occ_context_t *context, size_t *out_count) {
    if (context == nullptr || out_count == nullptr) {
        return OCC_NULL_ARGUMENT;
    }
    *out_count = context->diagnostics.size();
    return OCC_OK;
}

OCC_C_API occ_status_t OCC_C_CALL occ_context_last_diagnostic_at(const occ_context_t *context, size_t index,
                                                                 occ_log_level_t *out_level,
                                                                 const char **out_message_utf8) {
    if (context == nullptr || out_level == nullptr || out_message_utf8 == nullptr) {
        return OCC_NULL_ARGUMENT;
    }
    if (index >= context->diagnostics.size()) {
        return OCC_INDEX_OUT_OF_RANGE;
    }
    *out_level = context->diagnostics[index].level;
    *out_message_utf8 = context->diagnostics[index].message.c_str();
    return OCC_OK;
}

OCC_C_API occ_status_t OCC_C_CALL occ_curve3d_line(occ_context_t *context, occ_vec3_t p, occ_vec3_t d,
                                                   occ_curve3d_t **out_curve) {
    return guard(context, [&]() {
        if (check_context(context) != OCC_OK || out_curve == nullptr) {
            return OCC_NULL_ARGUMENT;
        }
        *out_curve = nullptr;
        const occ::handle<Geom_Curve> c = new Geom_Line(point(p), direction(d));
        return put_curve3d(out_curve, c);
    });
}

OCC_C_API occ_status_t OCC_C_CALL occ_curve3d_circle(occ_context_t *context, occ_axis2_t frame, double radius,
                                                     occ_curve3d_t **out_curve) {
    return guard(context, [&]() {
        if (check_context(context) != OCC_OK || out_curve == nullptr) {
            return OCC_NULL_ARGUMENT;
        }
        *out_curve = nullptr;
        const occ::handle<Geom_Curve> c = new Geom_Circle(axis2(frame), radius);
        return put_curve3d(out_curve, c);
    });
}

OCC_C_API occ_status_t OCC_C_CALL occ_curve3d_type(const occ_curve3d_t *curve, occ_curve_type_t *out_type) {
    return guard(nullptr, [&]() {
        const occ_curve3d_t *c = nullptr;
        const occ_status_t s = require_curve3d(curve, &c);
        if (s != OCC_OK || out_type == nullptr) {
            return s != OCC_OK ? s : OCC_NULL_ARGUMENT;
        }
        *out_type = curve_type(GeomAdaptor_Curve(c->curve).GetType());
        return OCC_OK;
    });
}

OCC_C_API occ_status_t OCC_C_CALL occ_curve3d_bounds(const occ_curve3d_t *curve, double *out_first, double *out_last) {
    return guard(nullptr, [&]() {
        const occ_curve3d_t *c = nullptr;
        const occ_status_t s = require_curve3d(curve, &c);
        if (s != OCC_OK || out_first == nullptr || out_last == nullptr) {
            return s != OCC_OK ? s : OCC_NULL_ARGUMENT;
        }
        *out_first = c->curve->FirstParameter();
        *out_last = c->curve->LastParameter();
        return OCC_OK;
    });
}

OCC_C_API occ_status_t OCC_C_CALL
occ_curve3d_eval(const occ_curve3d_t *curve, double parameter, occ_vec3_t *out_point) {
    return guard(nullptr, [&]() {
        const occ_curve3d_t *c = nullptr;
        const occ_status_t s = require_curve3d(curve, &c);
        if (s != OCC_OK || out_point == nullptr) {
            return s != OCC_OK ? s : OCC_NULL_ARGUMENT;
        }
        *out_point = vec3(c->curve->EvalD0(parameter));
        return OCC_OK;
    });
}

OCC_C_API occ_status_t OCC_C_CALL occ_curve3d_eval_d1(const occ_curve3d_t *curve, double parameter,
                                                      occ_vec3_t *out_point, occ_vec3_t *out_derivative) {
    return guard(nullptr, [&]() {
        const occ_curve3d_t *c = nullptr;
        const occ_status_t s = require_curve3d(curve, &c);
        if (s != OCC_OK || out_point == nullptr || out_derivative == nullptr) {
            return s != OCC_OK ? s : OCC_NULL_ARGUMENT;
        }
        const Geom_Curve::ResD1 r = c->curve->EvalD1(parameter);
        *out_point = vec3(r.Point);
        *out_derivative = vec3(r.D1);
        return OCC_OK;
    });
}

OCC_C_API occ_status_t OCC_C_CALL occ_surface_plane(occ_context_t *context, occ_axis2_t frame,
                                                    occ_surface_t **out_surface) {
    return guard(context, [&]() {
        if (context == nullptr || out_surface == nullptr) {
            return OCC_NULL_ARGUMENT;
        }
        *out_surface = nullptr;
        const occ::handle<Geom_Surface> s = new Geom_Plane(axis3(frame));
        return put_surface(out_surface, s);
    });
}

OCC_C_API occ_status_t OCC_C_CALL occ_surface_cylinder(occ_context_t *context, occ_axis2_t frame, double radius,
                                                       occ_surface_t **out_surface) {
    return guard(context, [&]() {
        if (context == nullptr || out_surface == nullptr) {
            return OCC_NULL_ARGUMENT;
        }
        *out_surface = nullptr;
        const occ::handle<Geom_Surface> s = new Geom_CylindricalSurface(axis3(frame), radius);
        return put_surface(out_surface, s);
    });
}

OCC_C_API occ_status_t OCC_C_CALL occ_surface_sphere(occ_context_t *context, occ_axis2_t frame, double radius,
                                                     occ_surface_t **out_surface) {
    return guard(context, [&]() {
        if (context == nullptr || out_surface == nullptr) {
            return OCC_NULL_ARGUMENT;
        }
        *out_surface = nullptr;
        const occ::handle<Geom_Surface> s = new Geom_SphericalSurface(axis3(frame), radius);
        return put_surface(out_surface, s);
    });
}

OCC_C_API occ_status_t OCC_C_CALL occ_surface_type(const occ_surface_t *surface, occ_surface_type_t *out_type) {
    return guard(nullptr, [&]() {
        const occ_surface_t *srf = nullptr;
        const occ_status_t s = require_surface(surface, &srf);
        if (s != OCC_OK || out_type == nullptr) {
            return s != OCC_OK ? s : OCC_NULL_ARGUMENT;
        }
        *out_type = surface_type(GeomAdaptor_Surface(srf->surface).GetType());
        return OCC_OK;
    });
}

OCC_C_API occ_status_t OCC_C_CALL occ_surface_bounds(const occ_surface_t *surface, double *out_umin, double *out_umax,
                                                     double *out_vmin, double *out_vmax) {
    return guard(nullptr, [&]() {
        const occ_surface_t *srf = nullptr;
        const occ_status_t s = require_surface(surface, &srf);
        if (s != OCC_OK || out_umin == nullptr || out_umax == nullptr || out_vmin == nullptr || out_vmax == nullptr) {
            return s != OCC_OK ? s : OCC_NULL_ARGUMENT;
        }
        srf->surface->Bounds(*out_umin, *out_umax, *out_vmin, *out_vmax);
        return OCC_OK;
    });
}

OCC_C_API occ_status_t OCC_C_CALL occ_surface_eval(const occ_surface_t *surface, double u, double v,
                                                   occ_vec3_t *out_point) {
    return guard(nullptr, [&]() {
        const occ_surface_t *srf = nullptr;
        const occ_status_t s = require_surface(surface, &srf);
        if (s != OCC_OK || out_point == nullptr) {
            return s != OCC_OK ? s : OCC_NULL_ARGUMENT;
        }
        *out_point = vec3(srf->surface->EvalD0(u, v));
        return OCC_OK;
    });
}

OCC_C_API occ_status_t OCC_C_CALL occ_surface_eval_d1(const occ_surface_t *surface, double u, double v,
                                                      occ_vec3_t *out_point, occ_vec3_t *out_du, occ_vec3_t *out_dv) {
    return guard(nullptr, [&]() {
        const occ_surface_t *srf = nullptr;
        const occ_status_t s = require_surface(surface, &srf);
        if (s != OCC_OK || out_point == nullptr || out_du == nullptr || out_dv == nullptr) {
            return s != OCC_OK ? s : OCC_NULL_ARGUMENT;
        }
        const Geom_Surface::ResD1 r = srf->surface->EvalD1(u, v);
        *out_point = vec3(r.Point);
        *out_du = vec3(r.D1U);
        *out_dv = vec3(r.D1V);
        return OCC_OK;
    });
}

OCC_C_API occ_status_t OCC_C_CALL occ_project_point_curve3d(occ_context_t *context, occ_vec3_t p,
                                                            const occ_curve3d_t *curve, double *out_parameter,
                                                            occ_vec3_t *out_nearest, double *out_distance) {
    return guard(context, [&]() {
        const occ_curve3d_t *c = nullptr;
        const occ_status_t s = require_curve3d(curve, &c);
        if (context == nullptr || s != OCC_OK || out_parameter == nullptr || out_nearest == nullptr || out_distance ==
            nullptr) {
            return context == nullptr ? OCC_NULL_ARGUMENT : (s != OCC_OK ? s : OCC_NULL_ARGUMENT);
        }
        const GeomAPI_ProjectPointOnCurve projector(point(p), c->curve);
        if (projector.NbPoints() == 0) {
            return OCC_EMPTY_RESULT;
        }
        *out_parameter = projector.LowerDistanceParameter();
        *out_nearest = vec3(projector.NearestPoint());
        *out_distance = projector.LowerDistance();
        return OCC_OK;
    });
}

OCC_C_API occ_status_t OCC_C_CALL occ_project_point_surface(occ_context_t *context, occ_vec3_t p,
                                                            const occ_surface_t *surface, double *out_u, double *out_v,
                                                            occ_vec3_t *out_nearest, double *out_distance) {
    return guard(context, [&]() {
        const occ_surface_t *srf = nullptr;
        const occ_status_t s = require_surface(surface, &srf);
        if (context == nullptr || s != OCC_OK || out_u == nullptr || out_v == nullptr || out_nearest == nullptr ||
            out_distance == nullptr) {
            return context == nullptr ? OCC_NULL_ARGUMENT : (s != OCC_OK ? s : OCC_NULL_ARGUMENT);
        }
        const GeomAPI_ProjectPointOnSurf projector(point(p), srf->surface);
        if (!projector.IsDone() || projector.NbPoints() == 0) {
            return OCC_EMPTY_RESULT;
        }
        projector.LowerDistanceParameters(*out_u, *out_v);
        *out_nearest = vec3(projector.NearestPoint());
        *out_distance = projector.LowerDistance();
        return OCC_OK;
    });
}

OCC_C_API occ_status_t OCC_C_CALL occ_shape_type(const occ_shape_t *shape, occ_shape_type_t *out_type) {
    return guard(nullptr, [&]() {
        const occ_shape_t *s = nullptr;
        const occ_status_t status = require_shape(shape, &s);
        if (status != OCC_OK || out_type == nullptr) {
            return status != OCC_OK ? status : OCC_NULL_ARGUMENT;
        }
        if (s->shape.IsNull()) {
            *out_type = OCC_SHAPE_UNKNOWN;
            return OCC_INVALID_SHAPE;
        }
        *out_type = shape_type(s->shape.ShapeType());
        return OCC_OK;
    });
}

OCC_C_API occ_status_t OCC_C_CALL occ_shape_is_null(const occ_shape_t *shape, int32_t *out_is_null) {
    return guard(nullptr, [&]() {
        const occ_shape_t *s = nullptr;
        const occ_status_t status = require_shape(shape, &s);
        if (status != OCC_OK || out_is_null == nullptr) {
            return status != OCC_OK ? status : OCC_NULL_ARGUMENT;
        }
        *out_is_null = s->shape.IsNull() ? OCC_C_TRUE : OCC_C_FALSE;
        return OCC_OK;
    });
}

OCC_C_API occ_status_t OCC_C_CALL occ_shape_orientation(const occ_shape_t *shape, occ_orientation_t *out_orientation) {
    return guard(nullptr, [&]() {
        const occ_shape_t *s = nullptr;
        const occ_status_t status = require_shape(shape, &s);
        if (status != OCC_OK || out_orientation == nullptr) {
            return status != OCC_OK ? status : OCC_NULL_ARGUMENT;
        }
        *out_orientation = orientation(s->shape.Orientation());
        return OCC_OK;
    });
}

OCC_C_API occ_status_t OCC_C_CALL occ_shape_location(const occ_shape_t *shape, occ_mat4_t *out_location) {
    return guard(nullptr, [&]() {
        const occ_shape_t *s = nullptr;
        const occ_status_t status = require_shape(shape, &s);
        if (status != OCC_OK || out_location == nullptr) {
            return status != OCC_OK ? status : OCC_NULL_ARGUMENT;
        }
        *out_location = mat4(s->shape.Location().Transformation());
        return OCC_OK;
    });
}

OCC_C_API occ_status_t OCC_C_CALL occ_shape_set_location(occ_context_t *context, const occ_shape_t *shape,
                                                         occ_mat4_t location, occ_shape_t **out_shape) {
    return guard(context, [&]() {
        const occ_shape_t *s = nullptr;
        const occ_status_t status = require_shape(shape, &s);
        if (context == nullptr || status != OCC_OK || out_shape == nullptr) {
            return context == nullptr ? OCC_NULL_ARGUMENT : (status != OCC_OK ? status : OCC_NULL_ARGUMENT);
        }
        *out_shape = nullptr;
        const TopoDS_Shape located = s->shape.Located(TopLoc_Location(trsf(location)));
        return put_shape(out_shape, located);
    });
}

OCC_C_API occ_status_t OCC_C_CALL occ_shape_reversed(occ_context_t *context, const occ_shape_t *shape,
                                                     occ_shape_t **out_shape) {
    return guard(context, [&]() {
        const occ_shape_t *s = nullptr;
        const occ_status_t status = require_shape(shape, &s);
        if (context == nullptr || status != OCC_OK || out_shape == nullptr) {
            return context == nullptr ? OCC_NULL_ARGUMENT : (status != OCC_OK ? status : OCC_NULL_ARGUMENT);
        }
        *out_shape = nullptr;
        return put_shape(out_shape, s->shape.Reversed());
    });
}

OCC_C_API occ_status_t OCC_C_CALL occ_shape_transformed(occ_context_t *context, const occ_shape_t *shape,
                                                        occ_mat4_t transform, int32_t copy_geometry,
                                                        occ_shape_t **out_shape) {
    return guard(context, [&]() {
        const occ_shape_t *s = nullptr;
        const occ_status_t status = require_shape(shape, &s);
        if (context == nullptr || status != OCC_OK || out_shape == nullptr) {
            return context == nullptr ? OCC_NULL_ARGUMENT : (status != OCC_OK ? status : OCC_NULL_ARGUMENT);
        }
        *out_shape = nullptr;
        BRepBuilderAPI_GTransform t(s->shape, gtrsf(transform), copy_geometry != 0);
        return put_shape(out_shape, t.Shape());
    });
}

OCC_C_API occ_status_t OCC_C_CALL occ_shape_bbox(occ_context_t *context, const occ_shape_t *shape,
                                                 int32_t use_triangulation, occ_bbox_t *out_bbox) {
    return guard(context, [&]() {
        const occ_shape_t *s = nullptr;
        const occ_status_t status = require_shape(shape, &s);
        if (context == nullptr || status != OCC_OK || out_bbox == nullptr) {
            return context == nullptr ? OCC_NULL_ARGUMENT : (status != OCC_OK ? status : OCC_NULL_ARGUMENT);
        }
        Bnd_Box box;
        BRepBndLib::Add(s->shape, box, use_triangulation != 0);
        *out_bbox = {};
        if (box.IsVoid()) {
            out_bbox->is_empty = OCC_C_TRUE;
            return OCC_OK;
        }
        box.Get(out_bbox->xmin, out_bbox->ymin, out_bbox->zmin, out_bbox->xmax, out_bbox->ymax, out_bbox->zmax);
        out_bbox->is_empty = OCC_C_FALSE;
        return OCC_OK;
    });
}

OCC_C_API occ_status_t OCC_C_CALL occ_shape_tolerance(const occ_shape_t *shape, double *out_tolerance) {
    return guard(nullptr, [&]() {
        const occ_shape_t *s = nullptr;
        const occ_status_t status = require_shape(shape, &s);
        if (status != OCC_OK || out_tolerance == nullptr) {
            return status != OCC_OK ? status : OCC_NULL_ARGUMENT;
        }
        if (s->shape.IsNull()) {
            return OCC_INVALID_SHAPE;
        }
        switch (s->shape.ShapeType()) {
            case TopAbs_VERTEX: *out_tolerance = BRep_Tool::Tolerance(TopoDS::Vertex(s->shape));
                return OCC_OK;
            case TopAbs_EDGE: *out_tolerance = BRep_Tool::Tolerance(TopoDS::Edge(s->shape));
                return OCC_OK;
            case TopAbs_FACE: *out_tolerance = BRep_Tool::Tolerance(TopoDS::Face(s->shape));
                return OCC_OK;
            default:
                *out_tolerance = std::max({
                    BRep_Tool::MaxTolerance(s->shape, TopAbs_VERTEX), BRep_Tool::MaxTolerance(s->shape, TopAbs_EDGE),
                    BRep_Tool::MaxTolerance(s->shape, TopAbs_FACE)
                });
                return OCC_OK;
        }
    });
}

OCC_C_API occ_status_t OCC_C_CALL occ_shape_count_subshapes(occ_context_t *context, const occ_shape_t *shape,
                                                            occ_shape_type_t type, occ_traversal_t traversal,
                                                            size_t *out_count) {
    return guard(context, [&]() {
        const occ_shape_t *s = nullptr;
        const occ_status_t status = require_shape(shape, &s);
        if (context == nullptr || status != OCC_OK || out_count == nullptr) {
            return context == nullptr ? OCC_NULL_ARGUMENT : (status != OCC_OK ? status : OCC_NULL_ARGUMENT);
        }
        *out_count = collect_subshapes(s->shape, shape_type(type), traversal).size();
        return OCC_OK;
    });
}

OCC_C_API occ_status_t OCC_C_CALL occ_shape_subshape_at(occ_context_t *context, const occ_shape_t *shape,
                                                        occ_shape_type_t type, occ_traversal_t traversal, size_t index,
                                                        occ_shape_t **out_subshape) {
    return guard(context, [&]() {
        const occ_shape_t *s = nullptr;
        const occ_status_t status = require_shape(shape, &s);
        if (context == nullptr || status != OCC_OK || out_subshape == nullptr) {
            return context == nullptr ? OCC_NULL_ARGUMENT : (status != OCC_OK ? status : OCC_NULL_ARGUMENT);
        }
        *out_subshape = nullptr;
        const std::vector<TopoDS_Shape> found = collect_subshapes(s->shape, shape_type(type), traversal);
        if (index >= found.size()) {
            return OCC_INDEX_OUT_OF_RANGE;
        }
        return put_shape(out_subshape, found[index]);
    });
}

OCC_C_API occ_status_t OCC_C_CALL occ_shape_visit_subshapes(occ_context_t *context, const occ_shape_t *shape,
                                                            occ_shape_type_t type, occ_traversal_t traversal,
                                                            occ_shape_visit_fn visitor, void *user_data) {
    return guard(context, [&]() {
        const occ_shape_t *s = nullptr;
        const occ_status_t status = require_shape(shape, &s);
        if (context == nullptr || status != OCC_OK || visitor == nullptr) {
            return context == nullptr ? OCC_NULL_ARGUMENT : (status != OCC_OK ? status : OCC_NULL_ARGUMENT);
        }
        const std::vector<TopoDS_Shape> found = collect_subshapes(s->shape, shape_type(type), traversal);
        for (const TopoDS_Shape &current: found) {
            const auto wrapped = new occ_shape_t(current);
            const occ_status_t visit = visitor(wrapped, shape_type(current.ShapeType()), user_data);
            occ_release(wrapped);
            if (visit != OCC_OK) {
                return visit;
            }
        }
        return OCC_OK;
    });
}

OCC_C_API occ_status_t OCC_C_CALL occ_vertex_point(const occ_shape_t *vertex, occ_vec3_t *out_point) {
    return guard(nullptr, [&]() {
        const occ_shape_t *v = nullptr;
        occ_status_t s = require_shape(vertex, &v);
        if (s != OCC_OK || out_point == nullptr) {
            return s != OCC_OK ? s : OCC_NULL_ARGUMENT;
        }
        s = expect_shape_kind(v, TopAbs_VERTEX);
        if (s != OCC_OK) {
            return s;
        }
        *out_point = vec3(BRep_Tool::Pnt(TopoDS::Vertex(v->shape)));
        return OCC_OK;
    });
}

OCC_C_API occ_status_t OCC_C_CALL occ_edge_curve3d(occ_context_t *context, const occ_shape_t *edge,
                                                   occ_curve3d_t **out_curve, double *out_first, double *out_last) {
    return guard(context, [&]() {
        const occ_shape_t *e = nullptr;
        occ_status_t s = require_shape(edge, &e);
        if (context == nullptr || s != OCC_OK || out_curve == nullptr || out_first == nullptr || out_last == nullptr) {
            return context == nullptr ? OCC_NULL_ARGUMENT : (s != OCC_OK ? s : OCC_NULL_ARGUMENT);
        }
        *out_curve = nullptr;
        s = expect_shape_kind(e, TopAbs_EDGE);
        if (s != OCC_OK) {
            return s;
        }
        const occ::handle<Geom_Curve> c = BRep_Tool::Curve(TopoDS::Edge(e->shape), *out_first, *out_last);
        return put_curve3d(out_curve, c);
    });
}

OCC_C_API occ_status_t OCC_C_CALL occ_edge_curve2d_on_face(occ_context_t *context, const occ_shape_t *edge,
                                                           const occ_shape_t *face, occ_curve2d_t **out_curve,
                                                           double *out_first, double *out_last) {
    return guard(context, [&]() {
        const occ_shape_t *e = nullptr;
        const occ_shape_t *f = nullptr;
        occ_status_t s = require_shape(edge, &e);
        if (s == OCC_OK) {
            s = require_shape(face, &f);
        }
        if (context == nullptr || s != OCC_OK || out_curve == nullptr || out_first == nullptr || out_last == nullptr) {
            return context == nullptr ? OCC_NULL_ARGUMENT : (s != OCC_OK ? s : OCC_NULL_ARGUMENT);
        }
        *out_curve = nullptr;
        s = expect_shape_kind(e, TopAbs_EDGE);
        if (s == OCC_OK) {
            s = expect_shape_kind(f, TopAbs_FACE);
        }
        if (s != OCC_OK) {
            return s;
        }
        const occ::handle<Geom2d_Curve> c = BRep_Tool::CurveOnSurface(TopoDS::Edge(e->shape), TopoDS::Face(f->shape),
                                                                      *out_first, *out_last);
        return put_curve2d(out_curve, c);
    });
}

OCC_C_API occ_status_t OCC_C_CALL occ_curve2d_type(const occ_curve2d_t *curve, occ_curve_type_t *out_type) {
    return guard(nullptr, [&]() {
        const occ_curve2d_t *c = nullptr;
        const occ_status_t s = require_curve2d(curve, &c);
        if (s != OCC_OK || out_type == nullptr) {
            return s != OCC_OK ? s : OCC_NULL_ARGUMENT;
        }
        *out_type = curve_type(Geom2dAdaptor_Curve(c->curve).GetType());
        return OCC_OK;
    });
}

OCC_C_API occ_status_t OCC_C_CALL
occ_curve2d_eval(const occ_curve2d_t *curve, double parameter, occ_vec2_t *out_point) {
    return guard(nullptr, [&]() {
        const occ_curve2d_t *c = nullptr;
        const occ_status_t s = require_curve2d(curve, &c);
        if (s != OCC_OK || out_point == nullptr) {
            return s != OCC_OK ? s : OCC_NULL_ARGUMENT;
        }
        *out_point = vec2(c->curve->EvalD0(parameter));
        return OCC_OK;
    });
}

OCC_C_API occ_status_t OCC_C_CALL occ_curve2d_eval_d1(const occ_curve2d_t *curve, double parameter,
                                                      occ_vec2_t *out_point, occ_vec2_t *out_derivative) {
    return guard(nullptr, [&]() {
        const occ_curve2d_t *c = nullptr;
        const occ_status_t s = require_curve2d(curve, &c);
        if (s != OCC_OK || out_point == nullptr || out_derivative == nullptr) {
            return s != OCC_OK ? s : OCC_NULL_ARGUMENT;
        }
        const Geom2d_Curve::ResD1 r = c->curve->EvalD1(parameter);
        *out_point = vec2(r.Point);
        *out_derivative = vec2(r.D1);
        return OCC_OK;
    });
}

OCC_C_API occ_status_t OCC_C_CALL occ_face_surface(occ_context_t *context, const occ_shape_t *face,
                                                   occ_surface_t **out_surface) {
    return guard(context, [&]() {
        const occ_shape_t *f = nullptr;
        occ_status_t s = require_shape(face, &f);
        if (context == nullptr || s != OCC_OK || out_surface == nullptr) {
            return context == nullptr ? OCC_NULL_ARGUMENT : (s != OCC_OK ? s : OCC_NULL_ARGUMENT);
        }
        *out_surface = nullptr;
        s = expect_shape_kind(f, TopAbs_FACE);
        if (s != OCC_OK) {
            return s;
        }
        return put_surface(out_surface, BRep_Tool::Surface(TopoDS::Face(f->shape)));
    });
}

OCC_C_API occ_status_t OCC_C_CALL occ_face_uv_bounds(occ_context_t *context, const occ_shape_t *face, double *out_umin,
                                                     double *out_umax, double *out_vmin, double *out_vmax) {
    return guard(context, [&]() {
        const occ_shape_t *f = nullptr;
        occ_status_t s = require_shape(face, &f);
        if (context == nullptr || s != OCC_OK || out_umin == nullptr || out_umax == nullptr || out_vmin == nullptr ||
            out_vmax == nullptr) {
            return context == nullptr ? OCC_NULL_ARGUMENT : (s != OCC_OK ? s : OCC_NULL_ARGUMENT);
        }
        s = expect_shape_kind(f, TopAbs_FACE);
        if (s != OCC_OK) {
            return s;
        }
        BRepTools::UVBounds(TopoDS::Face(f->shape), *out_umin, *out_umax, *out_vmin, *out_vmax);
        return OCC_OK;
    });
}

OCC_C_API occ_status_t OCC_C_CALL occ_make_vertex(occ_context_t *context, occ_vec3_t p, occ_shape_t **out_vertex) {
    return guard(context, [&]() {
        if (context == nullptr || out_vertex == nullptr) {
            return OCC_NULL_ARGUMENT;
        }
        *out_vertex = nullptr;
        BRepBuilderAPI_MakeVertex maker(point(p));
        return put_shape(out_vertex, maker.Shape());
    });
}

OCC_C_API occ_status_t OCC_C_CALL occ_make_edge_points(occ_context_t *context, occ_vec3_t start, occ_vec3_t end,
                                                       occ_shape_t **out_edge) {
    return guard(context, [&]() {
        if (context == nullptr || out_edge == nullptr) {
            return OCC_NULL_ARGUMENT;
        }
        *out_edge = nullptr;
        BRepBuilderAPI_MakeEdge maker(point(start), point(end));
        return put_shape(out_edge, maker.Shape());
    });
}

OCC_C_API occ_status_t OCC_C_CALL occ_make_edge_curve3d(occ_context_t *context, const occ_curve3d_t *curve,
                                                        double first, double last, occ_shape_t **out_edge) {
    return guard(context, [&]() {
        const occ_curve3d_t *c = nullptr;
        const occ_status_t s = require_curve3d(curve, &c);
        if (context == nullptr || s != OCC_OK || out_edge == nullptr) {
            return context == nullptr ? OCC_NULL_ARGUMENT : (s != OCC_OK ? s : OCC_NULL_ARGUMENT);
        }
        *out_edge = nullptr;
        BRepBuilderAPI_MakeEdge maker(c->curve, first, last);
        return put_shape(out_edge, maker.Shape());
    });
}

OCC_C_API occ_status_t OCC_C_CALL occ_make_wire(occ_context_t *context, const occ_shape_t *const*edges,
                                                size_t edge_count, occ_shape_t **out_wire) {
    return guard(context, [&]() {
        if (context == nullptr || out_wire == nullptr || edge_count == 0) {
            return context == nullptr || out_wire == nullptr ? OCC_NULL_ARGUMENT : OCC_INVALID_ARGUMENT;
        }
        *out_wire = nullptr;
        BRepBuilderAPI_MakeWire maker;
        for (size_t i = 0; i < edge_count; ++i) {
            const occ_shape_t *e = nullptr;
            occ_status_t s = require_shape(edges != nullptr ? edges[i] : nullptr, &e);
            if (s != OCC_OK) {
                return s;
            }
            s = expect_shape_kind(e, TopAbs_EDGE);
            if (s != OCC_OK) {
                return s;
            }
            maker.Add(TopoDS::Edge(e->shape));
        }
        return put_shape(out_wire, maker.Shape());
    });
}

OCC_C_API occ_status_t OCC_C_CALL occ_make_face_wire(occ_context_t *context, const occ_shape_t *wire,
                                                     int32_t force_planar, occ_shape_t **out_face) {
    return guard(context, [&]() {
        const occ_shape_t *w = nullptr;
        occ_status_t s = require_shape(wire, &w);
        if (context == nullptr || s != OCC_OK || out_face == nullptr) {
            return context == nullptr ? OCC_NULL_ARGUMENT : (s != OCC_OK ? s : OCC_NULL_ARGUMENT);
        }
        *out_face = nullptr;
        s = expect_shape_kind(w, TopAbs_WIRE);
        if (s != OCC_OK) {
            return s;
        }
        BRepBuilderAPI_MakeFace maker(TopoDS::Wire(w->shape), force_planar != 0);
        return put_shape(out_face, maker.Shape());
    });
}

OCC_C_API occ_status_t OCC_C_CALL occ_make_face_surface_bounds(occ_context_t *context, const occ_surface_t *surface,
                                                               double umin, double umax, double vmin, double vmax,
                                                               double tolerance, occ_shape_t **out_face) {
    return guard(context, [&]() {
        const occ_surface_t *srf = nullptr;
        const occ_status_t s = require_surface(surface, &srf);
        if (context == nullptr || s != OCC_OK || out_face == nullptr) {
            return context == nullptr ? OCC_NULL_ARGUMENT : (s != OCC_OK ? s : OCC_NULL_ARGUMENT);
        }
        *out_face = nullptr;
        BRepBuilderAPI_MakeFace maker(srf->surface, umin, umax, vmin, vmax, tolerance);
        return put_shape(out_face, maker.Shape());
    });
}

OCC_C_API occ_status_t OCC_C_CALL occ_make_shell(occ_context_t *context, const occ_shape_t *const*faces,
                                                 size_t face_count, occ_shape_t **out_shell) {
    return guard(context, [&]() {
        if (context == nullptr || out_shell == nullptr || face_count == 0 || faces == nullptr) {
            return context == nullptr || out_shell == nullptr || faces == nullptr
                       ? OCC_NULL_ARGUMENT
                       : OCC_INVALID_ARGUMENT;
        }
        *out_shell = nullptr;
        const BRep_Builder builder;
        TopoDS_Shell shell;
        builder.MakeShell(shell);
        for (size_t i = 0; i < face_count; ++i) {
            const occ_shape_t *f = nullptr;
            occ_status_t s = require_shape(faces[i], &f);
            if (s != OCC_OK) {
                return s;
            }
            s = expect_shape_kind(f, TopAbs_FACE);
            if (s != OCC_OK) {
                return s;
            }
            builder.Add(shell, f->shape);
        }
        return put_shape(out_shell, shell);
    });
}

OCC_C_API occ_status_t OCC_C_CALL occ_make_solid(occ_context_t *context, const occ_shape_t *shell,
                                                 occ_shape_t **out_solid) {
    return guard(context, [&]() {
        const occ_shape_t *sh = nullptr;
        occ_status_t s = require_shape(shell, &sh);
        if (context == nullptr || s != OCC_OK || out_solid == nullptr) {
            return context == nullptr ? OCC_NULL_ARGUMENT : (s != OCC_OK ? s : OCC_NULL_ARGUMENT);
        }
        *out_solid = nullptr;
        s = expect_shape_kind(sh, TopAbs_SHELL);
        if (s != OCC_OK) {
            return s;
        }
        BRepBuilderAPI_MakeSolid maker(TopoDS::Shell(sh->shape));
        return put_shape(out_solid, maker.Shape());
    });
}

OCC_C_API occ_status_t OCC_C_CALL occ_make_compound(occ_context_t *context, const occ_shape_t *const*shapes,
                                                    size_t shape_count, occ_shape_t **out_compound) {
    return guard(context, [&]() {
        if (context == nullptr || out_compound == nullptr || shapes == nullptr) {
            return OCC_NULL_ARGUMENT;
        }
        *out_compound = nullptr;
        const BRep_Builder builder;
        TopoDS_Compound compound;
        builder.MakeCompound(compound);
        for (size_t i = 0; i < shape_count; ++i) {
            const occ_shape_t *s = nullptr;
            const occ_status_t status = require_shape(shapes[i], &s);
            if (status != OCC_OK) {
                return status;
            }
            builder.Add(compound, s->shape);
        }
        return put_shape(out_compound, compound);
    });
}

OCC_C_API occ_status_t OCC_C_CALL occ_make_box(occ_context_t *context, double dx, double dy, double dz,
                                               occ_shape_t **out_shape) {
    return guard(context, [&]() {
        if (context == nullptr || out_shape == nullptr) {
            return OCC_NULL_ARGUMENT;
        }
        *out_shape = nullptr;
        BRepPrimAPI_MakeBox maker(dx, dy, dz);
        return put_shape(out_shape, maker.Shape());
    });
}

OCC_C_API occ_status_t OCC_C_CALL occ_make_box_between(occ_context_t *context, occ_vec3_t min_corner,
                                                       occ_vec3_t max_corner, occ_shape_t **out_shape) {
    return guard(context, [&]() {
        if (context == nullptr || out_shape == nullptr) {
            return OCC_NULL_ARGUMENT;
        }
        *out_shape = nullptr;
        BRepPrimAPI_MakeBox maker(point(min_corner), point(max_corner));
        return put_shape(out_shape, maker.Shape());
    });
}

OCC_C_API occ_status_t OCC_C_CALL occ_make_sphere(occ_context_t *context, occ_vec3_t center, double radius,
                                                  occ_shape_t **out_shape) {
    return guard(context, [&]() {
        if (context == nullptr || out_shape == nullptr) {
            return OCC_NULL_ARGUMENT;
        }
        *out_shape = nullptr;
        BRepPrimAPI_MakeSphere maker(point(center), radius);
        return put_shape(out_shape, maker.Shape());
    });
}

OCC_C_API occ_status_t OCC_C_CALL occ_make_cylinder(occ_context_t *context, occ_axis2_t axis, double radius,
                                                    double height, occ_shape_t **out_shape) {
    return guard(context, [&]() {
        if (context == nullptr || out_shape == nullptr) {
            return OCC_NULL_ARGUMENT;
        }
        *out_shape = nullptr;
        BRepPrimAPI_MakeCylinder maker(axis2(axis), radius, height);
        return put_shape(out_shape, maker.Shape());
    });
}

OCC_C_API occ_status_t OCC_C_CALL occ_make_cone(occ_context_t *context, occ_axis2_t axis, double radius1,
                                                double radius2, double height, occ_shape_t **out_shape) {
    return guard(context, [&]() {
        if (context == nullptr || out_shape == nullptr) {
            return OCC_NULL_ARGUMENT;
        }
        *out_shape = nullptr;
        BRepPrimAPI_MakeCone maker(axis2(axis), radius1, radius2, height);
        return put_shape(out_shape, maker.Shape());
    });
}

OCC_C_API occ_status_t OCC_C_CALL occ_make_torus(occ_context_t *context, occ_axis2_t axis, double major_radius,
                                                 double minor_radius, occ_shape_t **out_shape) {
    return guard(context, [&]() {
        if (context == nullptr || out_shape == nullptr) {
            return OCC_NULL_ARGUMENT;
        }
        *out_shape = nullptr;
        BRepPrimAPI_MakeTorus maker(axis2(axis), major_radius, minor_radius);
        return put_shape(out_shape, maker.Shape());
    });
}

OCC_C_API occ_status_t OCC_C_CALL occ_make_prism(occ_context_t *context, const occ_shape_t *profile, occ_vec3_t v,
                                                 int32_t copy_profile, occ_shape_t **out_shape) {
    return guard(context, [&]() {
        const occ_shape_t *p = nullptr;
        const occ_status_t s = require_shape(profile, &p);
        if (context == nullptr || s != OCC_OK || out_shape == nullptr) {
            return context == nullptr ? OCC_NULL_ARGUMENT : (s != OCC_OK ? s : OCC_NULL_ARGUMENT);
        }
        *out_shape = nullptr;
        BRepPrimAPI_MakePrism maker(p->shape, vector(v), copy_profile != 0);
        return put_shape(out_shape, maker.Shape());
    });
}

OCC_C_API occ_status_t OCC_C_CALL occ_make_revolution(occ_context_t *context, const occ_shape_t *profile,
                                                      occ_axis1_t ax, double angle_radians, int32_t copy_profile,
                                                      occ_shape_t **out_shape) {
    return guard(context, [&]() {
        const occ_shape_t *p = nullptr;
        const occ_status_t s = require_shape(profile, &p);
        if (context == nullptr || s != OCC_OK || out_shape == nullptr) {
            return context == nullptr ? OCC_NULL_ARGUMENT : (s != OCC_OK ? s : OCC_NULL_ARGUMENT);
        }
        *out_shape = nullptr;
        BRepPrimAPI_MakeRevol maker(p->shape, axis1(ax), angle_radians, copy_profile != 0);
        return put_shape(out_shape, maker.Shape());
    });
}

OCC_C_API occ_status_t OCC_C_CALL occ_shape_check(occ_context_t *context, const occ_shape_t *shape,
                                                  occ_shape_validity_t *out_validity) {
    return guard(context, [&]() {
        const occ_shape_t *s = nullptr;
        const occ_status_t status = require_shape(shape, &s);
        if (context == nullptr || status != OCC_OK || out_validity == nullptr) {
            return context == nullptr ? OCC_NULL_ARGUMENT : (status != OCC_OK ? status : OCC_NULL_ARGUMENT);
        }
        const BRepCheck_Analyzer analyzer(s->shape, true);
        *out_validity = analyzer.IsValid() ? OCC_SHAPE_VALID : OCC_SHAPE_INVALID;
        return OCC_OK;
    });
}

OCC_C_API occ_status_t OCC_C_CALL occ_shape_fix_basic(occ_context_t *context, const occ_shape_t *shape,
                                                      occ_shape_t **out_fixed_shape) {
    return guard(context, [&]() {
        const occ_shape_t *s = nullptr;
        const occ_status_t status = require_shape(shape, &s);
        if (context == nullptr || status != OCC_OK || out_fixed_shape == nullptr) {
            return context == nullptr ? OCC_NULL_ARGUMENT : (status != OCC_OK ? status : OCC_NULL_ARGUMENT);
        }
        *out_fixed_shape = nullptr;
        ShapeFix_Shape fixer(s->shape);
        fixer.Perform();
        return put_shape(out_fixed_shape, fixer.Shape());
    });
}

OCC_C_API occ_status_t OCC_C_CALL occ_shape_linear_properties(occ_context_t *context, const occ_shape_t *shape,
                                                              occ_mass_properties_t *out_properties) {
    return guard(context, [&]() {
        const occ_shape_t *s = nullptr;
        const occ_status_t status = require_shape(shape, &s);
        if (context == nullptr || status != OCC_OK || out_properties == nullptr) {
            return context == nullptr ? OCC_NULL_ARGUMENT : (status != OCC_OK ? status : OCC_NULL_ARGUMENT);
        }
        GProp_GProps props;
        BRepGProp::LinearProperties(s->shape, props);
        mass_properties(props, out_properties);
        return OCC_OK;
    });
}

OCC_C_API occ_status_t OCC_C_CALL occ_shape_surface_properties(occ_context_t *context, const occ_shape_t *shape,
                                                               occ_mass_properties_t *out_properties) {
    return guard(context, [&]() {
        const occ_shape_t *s = nullptr;
        const occ_status_t status = require_shape(shape, &s);
        if (context == nullptr || status != OCC_OK || out_properties == nullptr) {
            return context == nullptr ? OCC_NULL_ARGUMENT : (status != OCC_OK ? status : OCC_NULL_ARGUMENT);
        }
        GProp_GProps props;
        BRepGProp::SurfaceProperties(s->shape, props);
        mass_properties(props, out_properties);
        return OCC_OK;
    });
}

OCC_C_API occ_status_t OCC_C_CALL occ_shape_volume_properties(occ_context_t *context, const occ_shape_t *shape,
                                                              occ_mass_properties_t *out_properties) {
    return guard(context, [&]() {
        const occ_shape_t *s = nullptr;
        const occ_status_t status = require_shape(shape, &s);
        if (context == nullptr || status != OCC_OK || out_properties == nullptr) {
            return context == nullptr ? OCC_NULL_ARGUMENT : (status != OCC_OK ? status : OCC_NULL_ARGUMENT);
        }
        GProp_GProps props;
        BRepGProp::VolumeProperties(s->shape, props);
        mass_properties(props, out_properties);
        return OCC_OK;
    });
}

OCC_C_API occ_status_t OCC_C_CALL occ_shape_distance(occ_context_t *context, const occ_shape_t *a, const occ_shape_t *b,
                                                     double *out_distance, occ_vec3_t *out_nearest_on_a,
                                                     occ_vec3_t *out_nearest_on_b) {
    return guard(context, [&]() {
        const occ_shape_t *sa = nullptr;
        const occ_shape_t *sb = nullptr;
        occ_status_t status = require_shape(a, &sa);
        if (status == OCC_OK) {
            status = require_shape(b, &sb);
        }
        if (context == nullptr || status != OCC_OK || out_distance == nullptr || out_nearest_on_a == nullptr ||
            out_nearest_on_b == nullptr) {
            return context == nullptr ? OCC_NULL_ARGUMENT : (status != OCC_OK ? status : OCC_NULL_ARGUMENT);
        }
        const BRepExtrema_DistShapeShape dist(sa->shape, sb->shape);
        if (!dist.IsDone() || dist.NbSolution() == 0) {
            return OCC_EMPTY_RESULT;
        }
        *out_distance = dist.Value();
        *out_nearest_on_a = vec3(dist.PointOnShape1(1));
        *out_nearest_on_b = vec3(dist.PointOnShape2(1));
        return OCC_OK;
    });
}

OCC_C_API occ_boolean_options_t OCC_C_CALL occ_boolean_options_default(void) {
    occ_boolean_options_t o{};
    o.fuzzy_value = Precision::Confusion();
    o.run_parallel = OCC_C_FALSE;
    o.non_destructive = OCC_C_FALSE;
    o.glue = OCC_BOOLEAN_GLUE_OFF;
    o.check_inverted = OCC_C_TRUE;
    return o;
}

OCC_C_API occ_status_t OCC_C_CALL occ_boolean_apply(occ_context_t *context, occ_boolean_operation_t operation,
                                                    const occ_shape_t *a, const occ_shape_t *b,
                                                    const occ_boolean_options_t *options_or_null,
                                                    occ_shape_t **out_shape) {
    return guard(context, [&]() {
        const occ_shape_t *sa = nullptr;
        const occ_shape_t *sb = nullptr;
        occ_status_t status = require_shape(a, &sa);
        if (status == OCC_OK) {
            status = require_shape(b, &sb);
        }
        if (context == nullptr || status != OCC_OK || out_shape == nullptr) {
            return context == nullptr ? OCC_NULL_ARGUMENT : (status != OCC_OK ? status : OCC_NULL_ARGUMENT);
        }
        *out_shape = nullptr;
        NCollection_List<TopoDS_Shape> arguments;
        NCollection_List<TopoDS_Shape> tools;
        arguments.Append(sa->shape);
        tools.Append(sb->shape);
        BRepAlgoAPI_BooleanOperation op;
        op.SetOperation(boolean_operation(operation));
        op.SetArguments(arguments);
        op.SetTools(tools);
        apply_options(op, options_or_null);
        return finish_boolean(context, op, out_shape);
    });
}

OCC_C_API occ_status_t OCC_C_CALL occ_boolean_fuse_many(occ_context_t *context, const occ_shape_t *const*shapes,
                                                        size_t shape_count,
                                                        const occ_boolean_options_t *options_or_null,
                                                        occ_shape_t **out_shape) {
    return guard(context, [&]() {
        if (context == nullptr || out_shape == nullptr || shape_count < 2) {
            return context == nullptr || out_shape == nullptr ? OCC_NULL_ARGUMENT : OCC_INVALID_ARGUMENT;
        }
        *out_shape = nullptr;
        occ_status_t status = OCC_OK;
        NCollection_List<TopoDS_Shape> arguments;
        NCollection_List<TopoDS_Shape> tools;
        if (shapes == nullptr) {
            return OCC_NULL_ARGUMENT;
        }
        const occ_shape_t *first = nullptr;
        status = require_shape(shapes[0], &first);
        if (status != OCC_OK) {
            return status;
        }
        arguments.Append(first->shape);
        for (size_t i = 1; i < shape_count; ++i) {
            const occ_shape_t *item = nullptr;
            status = require_shape(shapes[i], &item);
            if (status != OCC_OK) {
                return status;
            }
            tools.Append(item->shape);
        }
        BRepAlgoAPI_BooleanOperation op;
        op.SetOperation(BOPAlgo_FUSE);
        op.SetArguments(arguments);
        op.SetTools(tools);
        apply_options(op, options_or_null);
        return finish_boolean(context, op, out_shape);
    });
}

OCC_C_API occ_status_t OCC_C_CALL occ_boolean_cut_many(occ_context_t *context, const occ_shape_t *base,
                                                       const occ_shape_t *const*tools_in, size_t tool_count,
                                                       const occ_boolean_options_t *options_or_null,
                                                       occ_shape_t **out_shape) {
    return guard(context, [&]() {
        const occ_shape_t *b = nullptr;
        occ_status_t status = require_shape(base, &b);
        if (context == nullptr || status != OCC_OK || out_shape == nullptr || tool_count == 0 || tools_in == nullptr) {
            return context == nullptr || out_shape == nullptr || tools_in == nullptr
                       ? OCC_NULL_ARGUMENT
                       : (status != OCC_OK ? status : OCC_INVALID_ARGUMENT);
        }
        *out_shape = nullptr;
        NCollection_List<TopoDS_Shape> arguments;
        NCollection_List<TopoDS_Shape> tools;
        arguments.Append(b->shape);
        for (size_t i = 0; i < tool_count; ++i) {
            const occ_shape_t *t = nullptr;
            status = require_shape(tools_in[i], &t);
            if (status != OCC_OK) {
                return status;
            }
            tools.Append(t->shape);
        }
        BRepAlgoAPI_BooleanOperation op;
        op.SetOperation(BOPAlgo_CUT);
        op.SetArguments(arguments);
        op.SetTools(tools);
        apply_options(op, options_or_null);
        return finish_boolean(context, op, out_shape);
    });
}

OCC_C_API occ_status_t OCC_C_CALL occ_shape_read_brep_file(occ_context_t *context, const char *path_utf8,
                                                           occ_shape_t **out_shape) {
    return guard(context, [&]() {
        if (context == nullptr || out_shape == nullptr || path_utf8 == nullptr) {
            return OCC_NULL_ARGUMENT;
        }
        *out_shape = nullptr;
        std::string bytes;
        const occ_status_t s = read_file_bytes(path_utf8, bytes);
        if (s != OCC_OK) {
            return s;
        }
        return read_brep_buffer(bytes.data(), bytes.size(), out_shape);
    });
}

OCC_C_API occ_status_t OCC_C_CALL occ_shape_write_brep_file(occ_context_t *context, const occ_shape_t *shape,
                                                            const char *path_utf8, occ_brep_format_t format) {
    return guard(context, [&]() {
        const occ_shape_t *s = nullptr;
        const occ_status_t status = require_shape(shape, &s);
        if (context == nullptr || status != OCC_OK || path_utf8 == nullptr) {
            return context == nullptr || path_utf8 == nullptr ? OCC_NULL_ARGUMENT : status;
        }
        bool ok = false;
        if (format == OCC_BREP_ASCII) {
            ok = BRepTools::Write(s->shape, path_utf8, true, false, TopTools_FormatVersion_CURRENT);
        } else if (format == OCC_BREP_BINARY) {
            ok = BinTools::Write(s->shape, path_utf8, true, false, BinTools_FormatVersion_CURRENT);
        } else {
            return OCC_INVALID_ARGUMENT;
        }
        return ok ? OCC_OK : OCC_IO_ERROR;
    });
}

OCC_C_API occ_status_t OCC_C_CALL occ_shape_read_brep_memory(occ_context_t *context, const void *data, size_t size,
                                                             occ_shape_t **out_shape) {
    return guard(context, [&]() {
        if (context == nullptr) {
            return OCC_NULL_ARGUMENT;
        }
        return read_brep_buffer(data, size, out_shape);
    });
}

OCC_C_API occ_status_t OCC_C_CALL occ_shape_write_brep_memory(occ_context_t *context, const occ_shape_t *shape,
                                                              occ_brep_format_t format, void **out_data,
                                                              size_t *out_size) {
    return guard(context, [&]() {
        const occ_shape_t *s = nullptr;
        const occ_status_t status = require_shape(shape, &s);
        if (context == nullptr || status != OCC_OK) {
            return context == nullptr ? OCC_NULL_ARGUMENT : status;
        }
        return write_brep_buffer(s->shape, format, out_data, out_size);
    });
}

OCC_C_API occ_mesh_options_t OCC_C_CALL occ_mesh_options_default(void) {
    const IMeshTools_Parameters p;
    occ_mesh_options_t o{};
    o.linear_deflection = p.Deflection;
    o.angular_deflection = p.Angle;
    o.relative_deflection = p.Relative ? OCC_C_TRUE : OCC_C_FALSE;
    o.run_parallel = p.InParallel ? OCC_C_TRUE : OCC_C_FALSE;
    return o;
}

OCC_C_API occ_status_t OCC_C_CALL occ_shape_triangulate(occ_context_t *context, const occ_shape_t *shape,
                                                        const occ_mesh_options_t *options_or_null,
                                                        occ_shape_t **out_shape_with_triangulation) {
    return guard(context, [&]() {
        const occ_shape_t *s = nullptr;
        const occ_status_t status = require_shape(shape, &s);
        if (context == nullptr || status != OCC_OK || out_shape_with_triangulation == nullptr) {
            return context == nullptr ? OCC_NULL_ARGUMENT : (status != OCC_OK ? status : OCC_NULL_ARGUMENT);
        }
        *out_shape_with_triangulation = nullptr;
        const occ_mesh_options_t defaults = occ_mesh_options_default();
        const occ_mesh_options_t &o = options_or_null != nullptr ? *options_or_null : defaults;
        const TopoDS_Shape copy = s->shape;
        const BRepMesh_IncrementalMesh mesh(copy, o.linear_deflection, o.relative_deflection != 0, o.angular_deflection,
                                            o.run_parallel != 0);
        if (!mesh.IsDone()) {
            return OCC_EMPTY_RESULT;
        }
        return put_shape(out_shape_with_triangulation, copy);
    });
}

OCC_C_API occ_status_t OCC_C_CALL occ_face_triangulation(occ_context_t *context, const occ_shape_t *face,
                                                         int32_t apply_face_location, occ_mesh_t **out_mesh) {
    return guard(context, [&]() {
        const occ_shape_t *f = nullptr;
        occ_status_t status = require_shape(face, &f);
        if (context == nullptr || status != OCC_OK || out_mesh == nullptr) {
            return context == nullptr ? OCC_NULL_ARGUMENT : (status != OCC_OK ? status : OCC_NULL_ARGUMENT);
        }
        *out_mesh = nullptr;
        status = expect_shape_kind(f, TopAbs_FACE);
        if (status != OCC_OK) {
            return status;
        }
        TopLoc_Location location;
        const occ::handle<Poly_Triangulation> triangulation =
                BRep_Tool::Triangulation(TopoDS::Face(f->shape), location);
        if (triangulation.IsNull() || triangulation->NbNodes() == 0 || triangulation->NbTriangles() == 0) {
            return OCC_EMPTY_RESULT;
        }
        const auto mesh = new occ_mesh_t;
        mesh->positions.reserve(static_cast<size_t>(triangulation->NbNodes()));
        if (triangulation->HasNormals()) {
            mesh->normals.reserve(static_cast<size_t>(triangulation->NbNodes()));
        }
        if (triangulation->HasUVNodes()) {
            mesh->uvs.reserve(static_cast<size_t>(triangulation->NbNodes()));
        }
        const gp_Trsf transform = location.Transformation();
        for (int i = 1; i <= triangulation->NbNodes(); ++i) {
            gp_Pnt p = triangulation->Node(i);
            if (apply_face_location != 0) {
                p.Transform(transform);
            }
            mesh->positions.push_back(vec3(p));
            if (triangulation->HasNormals()) {
                gp_Dir n = triangulation->Normal(i);
                if (apply_face_location != 0) {
                    gp_Vec nv(n);
                    nv.Transform(transform);
                    n = gp_Dir(nv);
                }
                mesh->normals.push_back(vec3(n));
            }
            if (triangulation->HasUVNodes()) {
                mesh->uvs.push_back(vec2(triangulation->UVNode(i)));
            }
        }
        mesh->triangles.reserve(static_cast<size_t>(triangulation->NbTriangles()));
        const bool reversed = f->shape.Orientation() == TopAbs_REVERSED;
        for (int i = 1; i <= triangulation->NbTriangles(); ++i) {
            int a = 0;
            int b = 0;
            int c = 0;
            triangulation->Triangle(i).Get(a, b, c);
            occ_triangle_t tri{};
            tri.i0 = static_cast<uint32_t>(a - 1);
            tri.i1 = static_cast<uint32_t>((reversed ? c : b) - 1);
            tri.i2 = static_cast<uint32_t>((reversed ? b : c) - 1);
            mesh->triangles.push_back(tri);
        }
        *out_mesh = mesh;
        return OCC_OK;
    });
}

OCC_C_API occ_status_t OCC_C_CALL occ_mesh_view(const occ_mesh_t *mesh, occ_mesh_view_t *out_view) {
    return guard(nullptr, [&]() {
        const occ_mesh_t *m = nullptr;
        const occ_status_t status = require_mesh(mesh, &m);
        if (status != OCC_OK || out_view == nullptr) {
            return status != OCC_OK ? status : OCC_NULL_ARGUMENT;
        }
        out_view->positions = m->positions.empty() ? nullptr : m->positions.data();
        out_view->normals = m->normals.empty() ? nullptr : m->normals.data();
        out_view->uvs = m->uvs.empty() ? nullptr : m->uvs.data();
        out_view->vertex_count = m->positions.size();
        out_view->triangles = m->triangles.empty() ? nullptr : m->triangles.data();
        out_view->triangle_count = m->triangles.size();
        return OCC_OK;
    });
}

OCC_C_API occ_mesh_packed_options_t OCC_C_CALL occ_mesh_packed_options_default(void) {
    return default_mesh_packed_options();
}

OCC_C_API occ_status_t OCC_C_CALL occ_mesh_packed_layout(occ_mesh_view_t view,
                                                         const occ_mesh_packed_options_t *options_or_null,
                                                         occ_mesh_packed_layout_t *out_layout) {
    return guard(nullptr, [&]() {
        return make_mesh_packed_layout(view, options_or_null, out_layout);
    });
}

OCC_C_API occ_status_t OCC_C_CALL occ_mesh_write_packed(occ_mesh_view_t view,
                                                        const occ_mesh_packed_options_t *options_or_null,
                                                        void *vertex_dst, size_t vertex_dst_size, void *index_dst,
                                                        size_t index_dst_size, occ_mesh_packed_layout_t *out_layout) {
    return guard(nullptr, [&]() {
        occ_mesh_packed_layout_t layout{};
        const occ_status_t status = make_mesh_packed_layout(view, options_or_null, &layout);
        if (out_layout != nullptr) {
            *out_layout = layout;
        }
        if (status != OCC_OK) {
            return status;
        }
        if ((layout.vertex_bytes != 0 && vertex_dst == nullptr) || (layout.index_bytes != 0 && index_dst == nullptr)) {
            return OCC_NULL_ARGUMENT;
        }
        if (vertex_dst_size < layout.vertex_bytes || index_dst_size < layout.index_bytes) {
            return OCC_INVALID_ARGUMENT;
        }
        if (layout.vertex_format == OCC_MESH_VERTEX_FLOAT32_P) {
            const auto dst = static_cast<occ_mesh_vertex_float32_p_t *>(vertex_dst);
            for (size_t i = 0; i < view.vertex_count; ++i) {
                const occ_vec3_t p = view.positions[i];
                dst[i] = {static_cast<float>(p.x), static_cast<float>(p.y), static_cast<float>(p.z)};
            }
        } else if (layout.vertex_format == OCC_MESH_VERTEX_FLOAT32_PN) {
            const auto dst = static_cast<occ_mesh_vertex_float32_pn_t *>(vertex_dst);
            for (size_t i = 0; i < view.vertex_count; ++i) {
                const occ_vec3_t p = view.positions[i];
                const occ_vec3_t n = view.normals[i];
                dst[i] = {
                    static_cast<float>(p.x), static_cast<float>(p.y), static_cast<float>(p.z),
                    static_cast<float>(n.x), static_cast<float>(n.y), static_cast<float>(n.z)
                };
            }
        } else {
            const auto dst = static_cast<occ_mesh_vertex_float32_pnt_t *>(vertex_dst);
            for (size_t i = 0; i < view.vertex_count; ++i) {
                const occ_vec3_t p = view.positions[i];
                const occ_vec3_t n = view.normals[i];
                const occ_vec2_t t = view.uvs[i];
                dst[i] = {
                    static_cast<float>(p.x), static_cast<float>(p.y), static_cast<float>(p.z),
                    static_cast<float>(n.x), static_cast<float>(n.y), static_cast<float>(n.z),
                    static_cast<float>(t.x), static_cast<float>(t.y)
                };
            }
        }
        if (layout.index_format == OCC_MESH_INDEX_UINT16) {
            const auto dst = static_cast<uint16_t *>(index_dst);
            for (size_t i = 0, j = 0; i < view.triangle_count; ++i) {
                const occ_triangle_t t = view.triangles[i];
                dst[j++] = static_cast<uint16_t>(t.i0);
                dst[j++] = static_cast<uint16_t>(t.i1);
                dst[j++] = static_cast<uint16_t>(t.i2);
            }
        } else {
            const auto dst = static_cast<uint32_t *>(index_dst);
            for (size_t i = 0, j = 0; i < view.triangle_count; ++i) {
                const occ_triangle_t t = view.triangles[i];
                dst[j++] = t.i0;
                dst[j++] = t.i1;
                dst[j++] = t.i2;
            }
        }
        return OCC_OK;
    });
}
