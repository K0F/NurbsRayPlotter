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
#define AUDIO_BUFFER_SIZE 512
#define WAVETABLE_SIZE 2048 

#define NUM_VOICES 3

#ifndef PI
    #define PI 3.14159265358979323846f
#endif

int frameCount = 0;

float wavetableBack[WAVETABLE_SIZE] = {0}; 
float wavetableFront[WAVETABLE_SIZE] = {0}; 

// A minor chord (Root, Minor 3rd, Perfect 5th)
float playbackFrequencies[NUM_VOICES] = {110.0f, 130.81f, 164.81f}; 
float readPointers[NUM_VOICES] = {0.0f, 0.0f, 0.0f};

int main() {
  InitGeometryLib();
  
  InitAudioDevice();
  SetAudioStreamBufferSizeDefault(AUDIO_BUFFER_SIZE);
  AudioStream audioStream = LoadAudioStream(SAMPLE_RATE, 16, 1); 
  
  short *audioWriteBuffer = (short *)malloc(sizeof(short) * AUDIO_BUFFER_SIZE);
  PlayAudioStream(audioStream);

  SetConfigFlags(FLAG_MSAA_4X_HINT); 
  InitWindow(768, 576, "OpenNURBS Drawable Synth");

  Vec2 samples[NUM_SAMPLES];
  
  // 1. INITIALIZE DEFAULT SHAPE ONCE
  // Instead of recalculating every frame, we just set it up at launch.
  for (int i = 0; i < NUM_SAMPLES; i++) {
      float x = i * 1.0f;
      float normPhase = ((float)i / (NUM_SAMPLES - 1)) * 2.0f * PI;
      float y = sinf(normPhase) * 50.0f;
      samples[i] = (Vec2){ x * 18.0f + 65.0f, 240.0f - y };
  }

  Font pixelFont = LoadFontEx("resources/Monaco_Linux.ttf", FONT_SIZE, NULL, 0);
  SetTextureFilter(pixelFont.texture, TEXTURE_FILTER_POINT);
  SetTargetFPS(TARGET_FPS);

  float brushRadius = 50.0f; // How wide the mouse affects the curve

  while (!WindowShouldClose()) {
    
    // 2. MOUSE INTERACTION (The Magnetic Brush)
    Vector2 mousePos = GetMousePosition();
    
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        for (int i = 0; i < NUM_SAMPLES; i++) {
            // Find distance from mouse to this control point on the X axis
            float distX = fabsf(mousePos.x - samples[i].x);
            
            // If the point is inside our brush radius, pull it towards the mouse Y
            if (distX < brushRadius) {
                // Closer to center = stronger pull (1.0). Edge of radius = weak pull (0.0).
                float influence = 1.0f - (distX / brushRadius);
                
                // Keep the points from going completely off-screen
                float targetY = mousePos.y;
                if (targetY < 40.0f) targetY = 40.0f;
                if (targetY > 440.0f) targetY = 440.0f;

                // Ease the point towards the mouse smoothly
                samples[i].y += (targetY - samples[i].y) * influence * 0.3f;
            }
        }
    }

    // Generate curve from the modified points
    NurbsCurveHandle curve = CreateFunctionCurve(samples, NUM_SAMPLES);

    // 3. BAKE TO BACK BUFFER (With Anti-Pop Taper)
    for (int i = 0; i < WAVETABLE_SIZE; i++) {
        float t = (float)i / WAVETABLE_SIZE;
        Vec2 curvePoint = EvaluateCurve(curve, t);
        
        // We divide by 150.0f to give the user plenty of vertical drawing space
        float rawAudioSample = (240.0f - curvePoint.y) / 150.0f;
        
        // RE-APPLIED TAPER: Forces the edges of the drawing to exactly 0.0
        // so you never get clicks when drawing mismatched edges.
        float fadeAmount = 1.0f;
        float fadeEdgeRatio = 0.05f; // Taper the outer 5% of the curve
        if (t < fadeEdgeRatio) {
            fadeAmount = sinf((t / fadeEdgeRatio) * (PI / 2.0f)); 
        } else if (t > 1.0f - fadeEdgeRatio) {
            fadeAmount = sinf(((1.0f - t) / fadeEdgeRatio) * (PI / 2.0f)); 
        }

        rawAudioSample *= fadeAmount;
        
        if (rawAudioSample > 1.0f)  rawAudioSample = 1.0f;
        if (rawAudioSample < -1.0f) rawAudioSample = -1.0f;
        
        wavetableBack[i] = rawAudioSample;
    }

    // 4. POLYPHONIC AUDIO SYNTHESIS
    if (IsAudioStreamProcessed(audioStream)) {
        for (int i = 0; i < AUDIO_BUFFER_SIZE; i++) {
            float mixedSample = 0.0f; 

            for (int v = 0; v < NUM_VOICES; v++) {
                float pointerStep = (WAVETABLE_SIZE * playbackFrequencies[v]) / SAMPLE_RATE;
                readPointers[v] += pointerStep;

                if (readPointers[v] >= WAVETABLE_SIZE) {
                    readPointers[v] -= WAVETABLE_SIZE;
                    
                    if (v == 0) { // Only swap buffer when root note finishes cycle
                        for (int j = 0; j < WAVETABLE_SIZE; j++) wavetableFront[j] = wavetableBack[j];
                    }
                }

                int index1 = (int)readPointers[v];
                int index2 = (index1 + 1) % WAVETABLE_SIZE; 
                float fraction = readPointers[v] - index1;

                float voiceSample = (wavetableFront[index1] * (1.0f - fraction)) + 
                                    (wavetableFront[index2] * fraction);

                mixedSample += voiceSample; 
            }

            mixedSample = mixedSample / NUM_VOICES;
            audioWriteBuffer[i] = (short)(mixedSample * 32767.0f * 0.30f);
        }
        UpdateAudioStream(audioStream, audioWriteBuffer, AUDIO_BUFFER_SIZE);
    }

    // 5. DRAW LOOP
    BeginDrawing();
    ClearBackground(BLACK);

    DrawTextEx(pixelFont, "CLICK AND DRAG TO DRAW WAVEFORM", (Vector2){40, 30}, FONT_SIZE, 1, WHITE);

    // Draw brush indicator
    DrawCircleLines(mousePos.x, mousePos.y, brushRadius, DARKGRAY);

    Vec2 prev = EvaluateCurve(curve, 0.0f);
    for (int i = 1; i <= CURVE_RESOLUTION; i++) {
      float t = (float)i / CURVE_RESOLUTION;
      Vec2 current = EvaluateCurve(curve, t);
      DrawLineEx((Vector2){prev.x, prev.y}, (Vector2){current.x, current.y}, 1.5f, SKYBLUE);
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