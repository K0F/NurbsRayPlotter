/*
Coded by Kof & Gemča @ 
Wed May 13 02:51:02 AM CEST 2026

   ,dPYb,                  ,dPYb,
   IP'`Yb                  IP'`Yb
   I8  8I                  I8  8I
   I8  8bgg,               I8  8'
   I8 dP" "8    ,ggggg,    I8 dP
   I8d8bggP"   dP"  "Y8ggg I8dP
   I8P' "Yb,  i8'    ,8I   I8P
  ,d8    `Yb,,d8,   ,d8'  ,d8b,_
  88P      Y8P"Y8888P"    PI8"8888
                           I8 `8,
                           I8  `8,
                           I8   8I
                           I8   8I
                           I8, ,8'
                            "Y8P'
*/


#include "raylib.h"
#include "geometry_bridge.h"
#include <math.h>

#define NUM_SAMPLES 11

int main() {
  InitGeometryLib();
  SetConfigFlags(FLAG_MSAA_4X_HINT); // Enable 4x Multi-sample Anti-aliasing
  InitWindow(500, 400, "C Raylib + openNURBS Bridge");

  Vec2 samples[NUM_SAMPLES];

  for (int i = 0; i < NUM_SAMPLES; i++) {
    float x = i * 1.0f;
    float y = sinf(x) * 100.0f;
    samples[i] = (Vec2){ x * 40.0f + 50.0f, 225.0f - y };
  }

  // Create the NURBS curve via the bridge
  NurbsCurveHandle curve = CreateFunctionCurve(samples, NUM_SAMPLES);

  // FPS
  SetTargetFPS(60);

  while (!WindowShouldClose()) {
    BeginDrawing();
    ClearBackground(RAYWHITE);

    // Draw the curve by sampling it at high resolution
    Vec2 prev = EvaluateCurve(curve, 0.0f);
    for (int i = 1; i <= 100; i++) {
      float t = (float)i / 100.0f;
      Vec2 current = EvaluateCurve(curve, t);
      DrawLineEx((Vector2){prev.x, prev.y}, (Vector2){current.x, current.y}, 2.0f, MAROON);
      prev = current;
    }

    // Draw original points for reference
    for (int i = 0; i < NUM_SAMPLES; i++) DrawCircle(samples[i].x, samples[i].y, 3, DARKBLUE);

    EndDrawing();
  }

  DestroyCurve(curve);
  CloseWindow();
  return 0;
}
