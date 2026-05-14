#ifndef GEOMETRY_BRIDGE_H
#define GEOMETRY_BRIDGE_H

/*
 * This header is C-compatible so it can be included in main.c
 */

#ifdef __cplusplus
extern "C" {
#endif

  // We use a void pointer to hide the C++ openNURBS class from the C compiler
  typedef void* NurbsCurveHandle;

  // A simple vector for our points
  typedef struct {
    float x;
    float y;
  } Vec2;

  // Function declarations
  void InitGeometryLib();
  NurbsCurveHandle CreateFunctionCurve(Vec2* points, int count);
  Vec2 EvaluateCurve(NurbsCurveHandle handle, float t);
  void DestroyCurve(NurbsCurveHandle handle);

#ifdef __cplusplus
}
#endif

#endif // GEOMETRY_BRIDGE_H
