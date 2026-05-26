#include "raylib.h"
#include "geometry_bridge.h"
#include "raymath.h"
#include <math.h>
#include <stddef.h>
#include <stdlib.h>

#define STB_PERLIN_IMPLEMENTATION
#include "stb_perlin.h"

#define WIDTH 932 
#define HEIGHT 576

#define NUM_SAMPLES 36
#define CURVE_RESOLUTION 256.0f
#define TARGET_FPS 60
#define FONT_SIZE 12

#define SAMPLE_RATE 48000
#define AUDIO_BUFFER_SIZE 2048
#define WAVETABLE_SIZE 2048

#ifndef PI
    #define PI 3.14159265358979323846f
#endif

int frameCount = 0;

typedef struct {
    Vec2 samples[NUM_SAMPLES];
    float wavetableBack[WAVETABLE_SIZE];
    float wavetableFront[WAVETABLE_SIZE];
    float baseFrequency;
    float currentVolume; 
    float readPointer;
    float pan;           
    Color color;
    bool active;
} MatrixChannel;

int main(void) {
    InitGeometryLib();
    InitAudioDevice();
    
    SetAudioStreamBufferSizeDefault(AUDIO_BUFFER_SIZE);
    AudioStream audioStream = LoadAudioStream(SAMPLE_RATE, 16, 2); 

    short *audioWriteBuffer = (short *)malloc(sizeof(short) * AUDIO_BUFFER_SIZE * 2);
    PlayAudioStream(audioStream);

    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(WIDTH, HEIGHT, "Multi-Curve Interactive WaveMatrix (Pure C)");

    float zoneHeight = (float)HEIGHT / 4.0f;

    MatrixChannel channels[4];
    channels[0].baseFrequency = 55.0f;  channels[0].pan = -0.6f; channels[0].color = GetColor(0xFF5733FF); channels[0].active = true;  channels[0].currentVolume = 1.0f;
    channels[1].baseFrequency = 110.0f; channels[1].pan = -0.2f; channels[1].color = GetColor(0x2ECC71FF); channels[1].active = false; channels[1].currentVolume = 0.0f;
    channels[2].baseFrequency = 220.0f; channels[2].pan =  0.2f; channels[2].color = GetColor(0x3498DBFF); channels[2].active = false; channels[2].currentVolume = 0.0f;
    channels[3].baseFrequency = 440.0f; channels[3].pan =  0.6f; channels[3].color = GetColor(0x9B59B6FF); channels[3].active = false; channels[3].currentVolume = 0.0f;

    for (int ch = 0; ch < 4; ch++) {
        float centerY = (zoneHeight * ch) + (zoneHeight / 2.0f);
        channels[ch].readPointer = 0.0f;
        
        for (int i = 0; i < NUM_SAMPLES; i++) {
            float normPhase = ((float)i / (NUM_SAMPLES - 1)) * 2.0f * PI;
            float y = sinf(normPhase) * (zoneHeight * 0.3f);
            channels[ch].samples[i].x = i * (WIDTH - 100.0f) / NUM_SAMPLES + 50.0f;
            channels[ch].samples[i].y = centerY - y;
        }
        for (int j = 0; j < WAVETABLE_SIZE; j++) {
            channels[ch].wavetableBack[j] = 0.0f;
            channels[ch].wavetableFront[j] = 0.0f;
        }
    }

    Font pixelFont = LoadFontEx("resources/Monaco_Linux.ttf", FONT_SIZE, NULL, 0);
    SetTextureFilter(pixelFont.texture, TEXTURE_FILTER_POINT);
    SetTargetFPS(TARGET_FPS);

    float brushRadius = 57.6f;

    while (!WindowShouldClose()) {

        // --- INPUT PROCESSING ---
        if (IsKeyPressed(KEY_ONE))   channels[0].active = !channels[0].active;
        if (IsKeyPressed(KEY_TWO))   channels[1].active = !channels[1].active;
        if (IsKeyPressed(KEY_THREE)) channels[2].active = !channels[2].active;
        if (IsKeyPressed(KEY_FOUR))  channels[3].active = !channels[3].active;

        Vector2 mousePos = GetMousePosition();

        // --- MATH BRUSH & PERLIN MORPHING ---
        for (int ch = 0; ch < 4; ch++) {
            channels[ch].currentVolume = Lerp(channels[ch].currentVolume, channels[ch].active ? 1.0f : 0.0f, 0.15f);
            float centerY = (zoneHeight * ch) + (zoneHeight / 2.0f);

            // SKIP MOUSE INTERACTION IF CHANNEL IS INACTIVE
            if (!channels[ch].active) {
                // Optional: You can still let Perlin run while muted, or add a skip here too.
                goto bake_wave; 
            }

            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                for (int i = 0; i < NUM_SAMPLES; i++) {
                    float distX = fabsf(mousePos.x - channels[ch].samples[i].x);
                    if (distX < brushRadius) {
                        float influence = 1.0f - (distX / brushRadius);
                        float targetY = mousePos.y;
                        
                        if (targetY < (zoneHeight * ch) + 10.0f) targetY = (zoneHeight * ch) + 10.0f;
                        if (targetY > (zoneHeight * (ch + 1)) - 10.0f) targetY = (zoneHeight * (ch + 1)) - 10.0f;

                        channels[ch].samples[i].y += (targetY - channels[ch].samples[i].y) * influence * 0.3f;
                    }
                }
            }

            for (int i = 0; i < NUM_SAMPLES; i++) {
                float noise = stb_perlin_noise3(i * 0.2f + ch, frameCount * 0.02f, 0, 0, 0, 0);
                channels[ch].samples[i].y += noise * 0.3f;
            }

        bake_wave: ; // Label for skipping brush math on muted lines
            NurbsCurveHandle curve = CreateFunctionCurve(channels[ch].samples, NUM_SAMPLES);

            for (int i = 0; i < WAVETABLE_SIZE; i++) {
                float t = (float)i / WAVETABLE_SIZE;
                Vec2 curvePoint = EvaluateCurve(curve, t);
                float rawAudioSample = (centerY - curvePoint.y) / (zoneHeight * 0.4f);

                float fadeAmount = 1.0f;
                float fadeEdgeRatio = 0.05f;
                if (t < fadeEdgeRatio) {
                    fadeAmount = sinf((t / fadeEdgeRatio) * (PI / 2.0f));
                } else if (t > 1.0f - fadeEdgeRatio) {
                    fadeAmount = sinf(((1.0f - t) / fadeEdgeRatio) * (PI / 2.0f));
                }

                rawAudioSample *= fadeAmount;
                if (rawAudioSample > 1.0f)  rawAudioSample = 1.0f;
                if (rawAudioSample < -1.0f) rawAudioSample = -1.0f;

                channels[ch].wavetableBack[i] = rawAudioSample;
            }
            DestroyCurve(curve);
        }

        // --- STEREO SYNTHESIS ---
        if (IsAudioStreamProcessed(audioStream)) {
            int writeIndex = 0;

            for (int i = 0; i < AUDIO_BUFFER_SIZE; i++) {
                float mixedSampleL = 0.0f;
                float mixedSampleR = 0.0f;

                for (int ch = 0; ch < 4; ch++) {
                    float pointerStep = (WAVETABLE_SIZE * channels[ch].baseFrequency) / SAMPLE_RATE;
                    channels[ch].readPointer += pointerStep;

                    if (channels[ch].readPointer >= WAVETABLE_SIZE) {
                        channels[ch].readPointer -= WAVETABLE_SIZE;
                        for (int j = 0; j < WAVETABLE_SIZE; j++) {
                            channels[ch].wavetableFront[j] = channels[ch].wavetableBack[j];
                        }
                    }

                    int index1 = (int)channels[ch].readPointer;
                    int index2 = (index1 + 1) % WAVETABLE_SIZE;
                    float fraction = channels[ch].readPointer - index1;

                    float voiceSample = (channels[ch].wavetableFront[index1] * (1.0f - fraction)) +
                                        (channels[ch].wavetableFront[index2] * fraction);

                    voiceSample *= channels[ch].currentVolume;

                    float panL = fminf(1.0f, 1.0f - channels[ch].pan);
                    float panR = fminf(1.0f, 1.0f + channels[ch].pan);

                    mixedSampleL += voiceSample * panL;
                    mixedSampleR += voiceSample * panR;
                }

                mixedSampleL = (mixedSampleL / 4.0f) * 32767.0f * 0.40f;
                mixedSampleR = (mixedSampleR / 4.0f) * 32767.0f * 0.40f;

                audioWriteBuffer[writeIndex++] = (short)mixedSampleL; 
                audioWriteBuffer[writeIndex++] = (short)mixedSampleR; 
            }
            UpdateAudioStream(audioStream, audioWriteBuffer, AUDIO_BUFFER_SIZE);
        }

        // --- RENDER ---
        BeginDrawing();
        ClearBackground(GetColor(0x0A0A0AFF));

        for (int ch = 0; ch < 4; ch++) {
            if (ch > 0) DrawLine(0, zoneHeight * ch, WIDTH, zoneHeight * ch, GetColor(0x222222FF));
            
            NurbsCurveHandle renderCurve = CreateFunctionCurve(channels[ch].samples, NUM_SAMPLES);
            
            Vec2 prev = EvaluateCurve(renderCurve, 0.0f);
            for (int i = 1; i <= (int)CURVE_RESOLUTION; i++) {
                float t = (float)i / CURVE_RESOLUTION;
                Vec2 current = EvaluateCurve(renderCurve, t);
                Color drawColor = channels[ch].active ? channels[ch].color : GetColor(0x222222FF);
                
                DrawLineEx((Vector2){prev.x, prev.y}, (Vector2){current.x, current.y}, channels[ch].active ? 2.0f : 1.0f, drawColor);
                prev = current;
            }
            DestroyCurve(renderCurve);

            for (int i = 0; i < NUM_SAMPLES; i++) {
                DrawCircle(channels[ch].samples[i].x, channels[ch].samples[i].y, 1.5f, channels[ch].active ? WHITE : GetColor(0x333333FF));
            }

            DrawTextEx(pixelFont, TextFormat("[%d] CH: %3.0fHz - %s", ch + 1, channels[ch].baseFrequency, channels[ch].active ? "ONLINE" : "MUTED"), 
                       (Vector2){30, (zoneHeight * ch) + 15}, FONT_SIZE, 1, channels[ch].active ? channels[ch].color : GetColor(0x444444FF));
        }

        DrawCircleLines(mousePos.x, mousePos.y, brushRadius, GetColor(0x44444455));
        DrawTextEx(pixelFont, TextFormat("FRAME: %06d", frameCount), (Vector2){WIDTH - 120, 15}, FONT_SIZE, 1, GetColor(0x555555FF));

        EndDrawing();
        frameCount++;
    }

    UnloadAudioStream(audioStream);
    free(audioWriteBuffer);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}