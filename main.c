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

#define SAMPLE_RATE 44100
#define AUDIO_BUFFER_SIZE 1024
#define WAVETABLE_SIZE 2048

#ifndef PI
    #define PI 3.14159265358979323846f
#endif

int frameCount = 0;

// Double-Buffering: We draw to the back, and the audio thread plays the front.
float wavetableBack[WAVETABLE_SIZE] = {0};
float wavetableFront[WAVETABLE_SIZE] = {0};

int main() {
  InitGeometryLib();

  InitAudioDevice();
  SetAudioStreamBufferSizeDefault(AUDIO_BUFFER_SIZE);
  AudioStream audioStream = LoadAudioStream(SAMPLE_RATE, 16, 1);

  short *audioWriteBuffer = (short *)malloc(sizeof(short) * AUDIO_BUFFER_SIZE);

  float readPointer = 0.0f;
  float playbackFrequency = 110.0f; // A2 pitch

  PlayAudioStream(audioStream);

  SetConfigFlags(FLAG_MSAA_4X_HINT);
  InitWindow(814, 576, "OpenNURBS Pristine Wavetable Synth");

  Vec2 samples[NUM_SAMPLES];
  Font pixelFont = LoadFontEx("resources/Monaco_Linux.ttf", FONT_SIZE, NULL, 0);
  SetTextureFilter(pixelFont.texture, TEXTURE_FILTER_POINT);
  SetTargetFPS(TARGET_FPS);

  while (!WindowShouldClose()) {

    // 1. COMPUTE CURVE (Morphing shape, NOT sliding phase)
    // We use a slow LFO to morph the wave's shape smoothly
    float shapeMorph = sinf(frameCount * 0.03f);

    for (int i = 0; i < NUM_SAMPLES; i++) {
      float x = i * 1.0f;
      float normPhase = ((float)i / (NUM_SAMPLES - 1)) * 2.0f * PI;

      // Notice there is no "timeOffset" sliding left or right.
      // The start (0) and end (2*PI) of these sines will ALWAYS be exactly 0.0
      float y = sinf(normPhase * 1.0f + x/2.0f) * 31.0f                                // Fundamental
              + sinf(normPhase * 2.0f + x/3.0f) * (16.1f * shapeMorph)          // 2nd Harmonic morphs
              + sinf(normPhase * 3.0f + x/4.0f) * (16.2f * (1.0f - shapeMorph)) // 3rd Harmonic inverses
              + sinf( (frameCount + x) / 10.0f * PI) * 20.0f;

      samples[i] = (Vec2){ x * 20.0f + 65.0f, 240.0f - y };
    }

    NurbsCurveHandle curve = CreateFunctionCurve(samples, NUM_SAMPLES);

    // 2. BAKE CURVE TO *BACK* BUFFER
    for (int i = 0; i < WAVETABLE_SIZE; i++) {
        float t = (float)i / WAVETABLE_SIZE;
        Vec2 curvePoint = EvaluateCurve(curve, t);

        float rawAudioSample = (240.0f - curvePoint.y) / 50.0f;

        if (rawAudioSample > 1.0f)  rawAudioSample = 1.0f;
        if (rawAudioSample < -1.0f) rawAudioSample = -1.0f;

        wavetableBack[i] = rawAudioSample;
    }

    // 3. AUDIO SYNTHESIS (Zero-Crossing Safe)
    if (IsAudioStreamProcessed(audioStream)) {
        float pointerStep = (WAVETABLE_SIZE * playbackFrequency) / SAMPLE_RATE;

        for (int i = 0; i < AUDIO_BUFFER_SIZE; i++) {

            readPointer += pointerStep;

            // THE MAGIC FIX: Only swap the back buffer to the front buffer
            // at the exact moment the wave starts a new cycle!
            if (readPointer >= WAVETABLE_SIZE) {
                readPointer -= WAVETABLE_SIZE;

                // Copy the newly drawn frame to the live audio buffer
                for (int j = 0; j < WAVETABLE_SIZE; j++) {
                    wavetableFront[j] = wavetableBack[j];
                }
            }

            int index1 = (int)readPointer;
            int index2 = (index1 + 1) % WAVETABLE_SIZE;
            float fraction = readPointer - index1;

            // Play from the FRONT buffer safely
            float sample = (wavetableFront[index1] * (1.0f - fraction)) +
                           (wavetableFront[index2] * fraction);

            audioWriteBuffer[i] = (short)(sample * 32767.0f * 0.30f);
        }
        UpdateAudioStream(audioStream, audioWriteBuffer, AUDIO_BUFFER_SIZE);
    }

    // 4. DRAW LOOP
    BeginDrawing();
    ClearBackground(BLACK);

    DrawTextEx(pixelFont, TextFormat("frameCount: %05d | Zero-Cross Active", frameCount), (Vector2){40, 30}, FONT_SIZE, 1, WHITE);

    Vec2 prev = EvaluateCurve(curve, 0.0f);
    for (int i = 1; i <= CURVE_RESOLUTION; i++) {
      float t = (float)i / CURVE_RESOLUTION;
      Vec2 current = EvaluateCurve(curve, t);
      DrawLineEx((Vector2){prev.x, prev.y}, (Vector2){current.x, current.y}, 1.5f, ORANGE);
      prev = current;
    }

    for (int i = 0; i < NUM_SAMPLES; i++) DrawCircle(samples[i].x, samples[i].y, 2.0f, WHITE);

    EndDrawing();
    DestroyCurve(curve);
    frameCount++;
  }

  UnloadAudioStream(audioStream);
  free(audioWriteBuffer);
  CloseAudioDevice();
  CloseWindow();
  return 0;
}
