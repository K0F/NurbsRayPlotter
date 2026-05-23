/*
Coded by Kof @
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
#include <stddef.h>

#define NUM_SAMPLES 36
#define CURVE_RESOLUTION 256.0f
#define TARGET_FPS 50

#define FONT_SIZE 12
#define RENDER true

#ifndef PI
    #define PI 3.14159265358979323846f
#endif

int frameCount = 0;

int main() {
  InitGeometryLib();
  SetConfigFlags(FLAG_MSAA_4X_HINT); // Enable 4x Multi-sample Anti-aliasing
                                     //
  InitWindow(768, 576, "OpenNURBS Plotter");

  Vec2 samples[NUM_SAMPLES];

    int baseFontSize = FONT_SIZE;
    int charCount = 127;
    // this fixes pixelfont
    int spacing = 1;

  Font pixelFont = LoadFontEx("resources/Monaco_Linux.ttf", baseFontSize, NULL, 0);

    // 2. Force Point Filtering to ensure perfect pixel boundaries
  SetTextureFilter(pixelFont.texture, TEXTURE_FILTER_POINT);


  // FPS
  SetTargetFPS(TARGET_FPS);


  // main loop
  while (!WindowShouldClose()) {
    BeginDrawing();
    ClearBackground(BLACK);

    Vector2 pos1 = {40.0,30.0};
    DrawTextEx(pixelFont, TextFormat("frameCount: %05d", frameCount) , pos1, (float)baseFontSize, spacing, WHITE);


    for (int i = 0; i < NUM_SAMPLES; i++) {
      float x = i * 1.0f;
      float y = sinf(x + (frameCount / 250.0f * PI) + (i/NUM_SAMPLES*PI) ) * 50.0f;
      samples[i] = (Vec2){ x * 18.0f + 65.0f, 240.0f - y };
    }

    // Create the NURBS curve via the bridge
    NurbsCurveHandle curve = CreateFunctionCurve(samples, NUM_SAMPLES);


    // Draw the curve by sampling it at high resolution
    Vec2 prev = EvaluateCurve(curve, 0.0f);
    for (int i = 1; i <= CURVE_RESOLUTION; i++) {
      float t = (float)i / CURVE_RESOLUTION;
      Vec2 current = EvaluateCurve(curve, t);
      DrawLineEx((Vector2) {prev.x, prev.y}, (Vector2){current.x, current.y}, 1.0f, ORANGE);
      prev = current;
    }

    // Draw original points for reference
    for (int i = 0; i < NUM_SAMPLES; i++) DrawCircle(samples[i].x, samples[i].y, 2.0f, WHITE);

    EndDrawing();

    if(RENDER){
      const char *fileName = TextFormat("render/frame_%05d.png", frameCount);
      TakeScreenshot(fileName);
    }
    DestroyCurve(curve);
    frameCount++;

  }// main loop

  CloseWindow();
  return 0;
}
