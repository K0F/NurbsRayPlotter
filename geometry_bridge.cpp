#include "geometry_bridge.h"
#include "opennurbs.h"

extern "C" {

  void InitGeometryLib() {
    // Automatic in modern openNURBS
  }

  NurbsCurveHandle CreateFunctionCurve(Vec2* points, int count) {
    if (count < 2) return NULL;

    // 1. Fill the point array
    ON_3dPointArray samplePoints(count);
    for (int i = 0; i < count; i++) {
      samplePoints.Append(ON_3dPoint(points[i].x, points[i].y, 0.0));
    }

    // 2. Initialize a new NURBS curve
    ON_NurbsCurve* curve = ON_NurbsCurve::New();

    // 3. The 5-argument signature your compiler is asking for:
    // (dimension, degree, point_count, points_pointer, knot_step)
    int dimension = 3;
    int degree = (count > 3) ? 3 : count - 1;

    bool success = curve->CreateClampedUniformNurbs(
        dimension, 
        degree, 
        count, 
        samplePoints.Array(), 
        1.0
        );

    if (success) {
      return (NurbsCurveHandle)curve;
    }

    delete curve;
    return NULL;
  }

  Vec2 EvaluateCurve(NurbsCurveHandle handle, float t) {
    if (!handle) return (Vec2){0.0f, 0.0f};

    ON_NurbsCurve* curve = (ON_NurbsCurve*)handle;

    double t0, t1;
    if (!curve->GetDomain(&t0, &t1)) {
      t0 = 0.0; t1 = 1.0;
    }

    // Map 0.0-1.0 to the actual curve domain
    double realT = t0 + (double)t * (t1 - t0);

    ON_3dPoint p;
    curve->Evaluate(realT, 0, 3, &p.x);

    return (Vec2){ (float)p.x, (float)p.y };
  }

  void DestroyCurve(NurbsCurveHandle handle) {
    if (handle) {
      delete (ON_NurbsCurve*)handle;
    }
  }

} // extern "C"
