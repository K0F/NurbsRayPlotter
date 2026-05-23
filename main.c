/*
Coded by Kof @
Wed May 13 02:51:02 AM CEST 2026

   ,dPYb,                  ,dPYb,
   IP'`Yb                  IP'`Yb
   I8  8I                  I8  8I
   I8  8bgg,               I8  8'
   I8 dP" "8    ,ggggg,    I8 dP
   I8d8bggP"    dP"  "Y8ggg I8dP
   I8P' "Yb,  i8'  ,8I    I8P
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
#include <stdlib.h>

#define NUM_SAMPLES 36
#define CURVE_RESOLUTION 256.0f
#define TARGET_FPS 50

#define FONT_SIZE 12
#define RENDER false

#define SAMPLE_RATE 48000
#define AUDIO_BUFFER_SIZE 2048

#ifndef PI
    #define PI 3.14159265358979323846f
#endif

int frameCount = 0;

int main() {
  InitGeometryLib();

  // 1. Audio Stream Setup
  InitAudioDevice();
  SetAudioStreamBufferSizeDefault(AUDIO_BUFFER_SIZE);
  AudioStream audioStream = LoadAudioStream(SAMPLE_RATE, 16, 1);

  short *audioWriteBuffer = (short *)malloc(sizeof(short) * AUDIO_BUFFER_SIZE);
  float synthPhase = 0.0f;
  float basePlaybackFrequency = 110.0f; // Fundamental frequency (A2 pitch)

  PlayAudioStream(audioStream);

  SetConfigFlags(FLAG_MSAA_4X_HINT); 
  InitWindow(768, 576, "OpenNURBS Curve Synth Interpreter");

  Vec2 samples[NUM_SAMPLES];
  int baseFontSize = FONT_SIZE;
  int spacing = 1;

  Font pixelFont = LoadFontEx("resources/Monaco_Linux.ttf", baseFontSize, NULL, 0);
  SetTextureFilter(pixelFont.texture, TEXTURE_FILTER_POINT);

  SetTargetFPS(TARGET_FPS);

  // main loop
  while (!WindowShouldClose()) {

    // Compute current frame's control points
    for (int i = 0; i < NUM_SAMPLES; i++) {
      float x = i * 1.0f;
      float y = sinf(x+(sinf(x/100.0f+frameCount/500.0f*PI) + 1.0f / 2.0f) * ((float)frameCount / (float)NUM_SAMPLES * PI + x)) * 50.0f;
      samples[i] = (Vec2){ x * 18.0f + 65.0f, 240.0f - y };
    }

    // Generate the geometric NURBS curve handle from points
    NurbsCurveHandle curve = CreateFunctionCurve(samples, NUM_SAMPLES);

    // 2. INTERPRET GEOMETRIC CURVE AS AUDIO WAVEFORM
    if (IsAudioStreamProcessed(audioStream)) {
        for (int i = 0; i < AUDIO_BUFFER_SIZE; i++) {
            // Increment the audio hardware playback pointer
            synthPhase += (2.0f * PI * basePlaybackFrequency) / SAMPLE_RATE;
            if (synthPhase > 2.0f * PI) synthPhase -= 2.0f * PI;

            // Map the periodic audio phase directly to the curve domain [0.0, 1.0]
            float t = synthPhase / (2.0f * PI);

            // Read the custom geometry position vector evaluated at parameter 't'
            Vec2 curvePoint = EvaluateCurve(curve, t);

            // Normalize the screen Y coordinate back into audio space
            // 240.0f is your visual horizontal origin. Your wave height amplitude is 50.0f pixels.
            float rawAudioSample = (240.0f - curvePoint.y) / 50.0f;

            // Clamp threshold safely to protect hardware against math anomalies
            if (rawAudioSample > 1.0f)  rawAudioSample = 1.0f;
            if (rawAudioSample < -1.0f) rawAudioSample = -1.0f;

            // Convert to 16-bit PCM integer data stream (multiplied by 0.30 volume mix)
            audioWriteBuffer[i] = (short)(rawAudioSample * 32767.0f * 0.30f);
        }
        UpdateAudioStream(audioStream, audioWriteBuffer, AUDIO_BUFFER_SIZE);
    }

    // 3. DRAW LOOP
    BeginDrawing();
    ClearBackground(BLACK);

    Vector2 pos1 = {40.0,30.0};
    DrawTextEx(pixelFont, TextFormat("frameCount: %05d | Playing NURBS Geometry", frameCount) , pos1, (float)baseFontSize, spacing, WHITE);

    // Draw high-resolution curve representation on screen
    Vec2 prev = EvaluateCurve(curve, 0.0f);
    for (int i = 1; i <= CURVE_RESOLUTION; i++) {
      float t = (float)i / CURVE_RESOLUTION;
      Vec2 current = EvaluateCurve(curve, t);
      DrawLineEx((Vector2) {prev.x, prev.y}, (Vector2){current.x, current.y}, 1.5f, ORANGE);
      prev = current;
    }

    // Reference context indicators
    for (int i = 0; i < NUM_SAMPLES; i++) DrawCircle(samples[i].x, samples[i].y, 2.0f, WHITE);

    EndDrawing();

    if(RENDER){
      const char *fileName = TextFormat("render/frame_%05d.png", frameCount);
      TakeScreenshot(fileName);
    }

    // Clean up handle at the explicit boundary end of frame processing
    DestroyCurve(curve);
    frameCount++;

  } // main loop

  // Free system memory buffers safely
  UnloadAudioStream(audioStream);
  free(audioWriteBuffer);
  CloseAudioDevice();

  CloseWindow();
  return 0;
}
