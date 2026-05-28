/**
Coded by Kof @ 
Tue May 26 04:30:21 PM CEST 2026
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
#include "raymath.h"
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>          // <-- added for memcpy

#define STB_PERLIN_IMPLEMENTATION
#include "stb_perlin.h"

#define WIDTH 932 
#define HEIGHT 576

#define NUM_SAMPLES 36
#define CURVE_RESOLUTION 2048.0f
#define TARGET_FPS 60
#define FONT_SIZE 12

#define SAMPLE_RATE 48000
#define AUDIO_BUFFER_SIZE 2048
#define WAVETABLE_SIZE 2048

#ifndef PI
#define PI 3.14159265358979323846f
#endif

int frameCount = 0;

/* ---------- ADSR ---------- */
typedef enum {
    ENV_IDLE,
    ENV_ATTACK,
    ENV_DECAY,
    ENV_SUSTAIN,
    ENV_RELEASE
} EnvStage;

typedef struct {
    float attackTime;   // seconds
    float decayTime;    // seconds
    float sustainLevel; // 0.0‑1.0
    float releaseTime; // seconds
} AdsrParameters;

/* ---------- CHANNEL ---------- */
typedef struct {
    Vec2 samples[NUM_SAMPLES];
    float wavetableBack[WAVETABLE_SIZE];
    float wavetableFront[WAVETABLE_SIZE];
    float baseFrequency;   // playback pitch (changes with key presses)
    float nativeFrequency; // fixed structural pitch
    float currentVolume; 
    float readPointer;
    float pan;              // -1.0 (left) … 1.0 (right)
    Color color;
    bool active;            // edit mode toggle (keys 1‑4)
    int currentMidiNote;

    /* ADSR state */
    EnvStage envStage;
    float envVolume;
    float envTimeCounter;
    AdsrParameters adsr;
} MatrixChannel;

/* ---------- UTILITIES ---------- */
static float MidiToFreq(int note) {               // (currently unused, keep for future)
    return 440.0f * powf(2.0f, (float)(note - 69) / 12.0f);
}

/* ---------- INPUT HANDLING ---------- */
static void ProcessMidiInputs(MatrixChannel *channels) {
    /* channel toggles */
    if (IsKeyPressed(KEY_ONE))   channels[0].active = !channels[0].active;
    if (IsKeyPressed(KEY_TWO))   channels[1].active = !channels[1].active;
    if (IsKeyPressed(KEY_THREE)) channels[2].active = !channels[2].active;
    if (IsKeyPressed(KEY_FOUR))  channels[3].active = !channels[3].active;

    /* piano layout – 13 chromatic keys C4‑C5 */
    const int pitchKeys[13] = {
        KEY_A, KEY_W, KEY_S, KEY_E, KEY_D, KEY_F,
        KEY_T, KEY_G, KEY_Y, KEY_H, KEY_U, KEY_J, KEY_K
    };
    const int scaleNotes[13] = {
        60, 61, 62, 63, 64, 65,
        66, 67, 68, 69, 70, 71, 72   // C4 … C5
    };

    for (int k = 0; k < 13; ++k) {
        if (IsKeyPressed(pitchKeys[k])) {
            int note = scaleNotes[k];
            float ratio = powf(2.0f, (float)(note - 60) / 12.0f);

            for (int i = 0; i < 4; ++i) {
                if (channels[i].active) {
                    channels[i].currentMidiNote = note;
                    channels[i].baseFrequency   = channels[i].nativeFrequency * ratio;
                    channels[i].envStage        = ENV_ATTACK;
                    channels[i].envTimeCounter  = 0.0f;
                }
            }
        }

        if (IsKeyReleased(pitchKeys[k])) {
            int note = scaleNotes[k];
            for (int i = 0; i < 4; ++i) {
                if (channels[i].currentMidiNote == note && channels[i].envStage != ENV_RELEASE) {
                    channels[i].envStage       = ENV_RELEASE;
                    channels[i].envTimeCounter = 0.0f;
                }
            }
        }
    }
}

/* ---------- ADSR PROCESSING ---------- */
static void UpdateChannelEnvelope(MatrixChannel *ch, float sampleRate) {
    float dt = 1.0f / sampleRate;
    ch->envTimeCounter += dt;

    switch (ch->envStage) {
        case ENV_IDLE:
            ch->envVolume = 0.0f;
            break;

        case ENV_ATTACK:
            if (ch->adsr.attackTime > 0.0f) {
                ch->envVolume = ch->envTimeCounter / ch->adsr.attackTime;
                if (ch->envVolume >= 1.0f) {
                    ch->envVolume = 1.0f;
                    ch->envStage = ENV_DECAY;
                    ch->envTimeCounter = 0.0f;
                }
            } else {
                ch->envVolume = 1.0f;
                ch->envStage = ENV_DECAY;
                ch->envTimeCounter = 0.0f;
            }
            break;

        case ENV_DECAY:
            if (ch->adsr.decayTime > 0.0f) {
                float prog = ch->envTimeCounter / ch->adsr.decayTime;
                ch->envVolume = 1.0f - prog * (1.0f - ch->adsr.sustainLevel);
                if (prog >= 1.0f) {
                    ch->envVolume = ch->adsr.sustainLevel;
                    ch->envStage = ENV_SUSTAIN;
                    ch->envTimeCounter = 0.0f;
                }
            } else {
                ch->envVolume = ch->adsr.sustainLevel;
                ch->envStage = ENV_SUSTAIN;
                ch->envTimeCounter = 0.0f;
            }
            break;

        case ENV_SUSTAIN:
            ch->envVolume = ch->adsr.sustainLevel;
            break;

        case ENV_RELEASE:
            if (ch->adsr.releaseTime > 0.0f) {
                float prog = ch->envTimeCounter / ch->adsr.releaseTime;
                ch->envVolume = ch->adsr.sustainLevel * (1.0f - prog);
                if (prog >= 1.0f) {
                    ch->envVolume = 0.0f;
                    ch->envStage = ENV_IDLE;
                    ch->envTimeCounter = 0.0f;
                }
            } else {
                ch->envVolume = 0.0f;
                ch->envStage = ENV_IDLE;
                ch->envTimeCounter = 0.0f;
            }
            break;
    }
}

/* ---------- MAIN ---------- */
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

    /* ----- initialise per‑channel data ----- */
    for (int ch = 0; ch < 4; ++ch) {
        channels[ch].adsr.attackTime   = 0.15f;
        channels[ch].adsr.decayTime    = 0.30f;
        channels[ch].adsr.sustainLevel = 0.70f;
        channels[ch].adsr.releaseTime = 0.40f;
        channels[ch].envStage          = ENV_IDLE;
        channels[ch].envVolume         = 0.0f;
        channels[ch].envTimeCounter    = 0.0f;

        /* native frequencies: 55 Hz, 110 Hz, 220 Hz, 440 Hz */
        channels[ch].nativeFrequency = 55.0f * powf(2.0f, (float)ch);
        channels[ch].baseFrequency   = 0.0f;          // silent until a key is pressed
        channels[ch].pan             = 0.0f;          // centred
        channels[ch].active          = true;          // editing enabled by default
        channels[ch].color = (Color){
            (unsigned char)(64 + 48 * ch),
            (unsigned char)(128 - 32 * ch),
            (unsigned char)(255 - 64 * ch),
            255
        };

        float centerY = (zoneHeight * ch) + (zoneHeight / 2.0f);
        channels[ch].readPointer = 0.0f;

        /* initialise sample points (simple sine shape) */
        for (int i = 0; i < NUM_SAMPLES; ++i) {
            float phase = ((float)i / (NUM_SAMPLES - 1)) * 2.0f * PI;
            float y = sinf(phase) * (zoneHeight * 0.3f);
            channels[ch].samples[i].x = i * (WIDTH - 100.0f) / NUM_SAMPLES + 50.0f;
            channels[ch].samples[i].y = centerY - y;
        }

        /* zero wavetable buffers */
        for (int i = 0; i < WAVETABLE_SIZE; ++i) {
            channels[ch].wavetableBack[i]  = 0.0f;
            channels[ch].wavetableFront[i] = 0.0f;
        }
    }

    Font pixelFont = LoadFontEx("resources/Monaco_Linux.ttf", FONT_SIZE, NULL, 0);
    SetTextureFilter(pixelFont.texture, TEXTURE_FILTER_POINT);
    SetTargetFPS(TARGET_FPS);

    float brushRadius = 57.6f;

    while (!WindowShouldClose()) {
        ProcessMidiInputs(channels);
        Vector2 mousePos = GetMousePosition();

        /* ----- curve editing & wavetable baking ----- */
        for (int ch = 0; ch < 4; ++ch) {
            float centerY = (zoneHeight * ch) + (zoneHeight / 2.0f);

            /* mouse brush */
            if (channels[ch].active && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                for (int i = 0; i < NUM_SAMPLES; ++i) {
                    float distX = fabsf(mousePos.x - channels[ch].samples[i].x);
                    if (distX < brushRadius) {
                        float influence = 1.0f - (distX / brushRadius);
                        float targetY   = Clamp(mousePos.y,
                                                zoneHeight * ch + 10.0f,
                                                zoneHeight * (ch + 1) - 10.0f);
                        channels[ch].samples[i].y += (targetY - channels[ch].samples[i].y) * influence * 0.3f;
                    }
                }
            }

            /* perlin distortion */
            for (int i = 0; i < NUM_SAMPLES; ++i) {
                float noise = stb_perlin_noise3(i * 0.2f + ch * 10.0f,
                                                frameCount * 0.02f,
                                                0, 0, 0, 0);
                channels[ch].samples[i].y += noise * 0.33f;
            }

            /* bake wavetable from the current curve */
            NurbsCurveHandle curve = CreateFunctionCurve(channels[ch].samples, NUM_SAMPLES);
            for (int i = 0; i < WAVETABLE_SIZE; ++i) {
                float t = (float)i / WAVETABLE_SIZE;
                Vec2 pt = EvaluateCurve(curve, t);
                float raw = (centerY - pt.y) / (zoneHeight * 0.4f);

                /* fade edges to avoid clicks */
                float fade = 1.0f;
                const float edge = 0.05f;
                if (t < edge)       fade = sinf((t / edge) * (PI / 2.0f));
                else if (t > 1.0f - edge) fade = sinf(((1.0f - t) / edge) * (PI / 2.0f));

                raw = Clamp(raw * fade, -1.0f, 1.0f);
                channels[ch].wavetableBack[i] = raw;
            }
            DestroyCurve(curve);
        }

        /* ----- audio synthesis ----- */
        if (IsAudioStreamProcessed(audioStream)) {
            int writeIdx = 0;
            for (int i = 0; i < AUDIO_BUFFER_SIZE; ++i) {
                float mixL = 0.0f, mixR = 0.0f;

                for (int ch = 0; ch < 4; ++ch) {
                    UpdateChannelEnvelope(&channels[ch], (float)SAMPLE_RATE);

                    if (channels[ch].baseFrequency > 0.0f) {
                        float step = (WAVETABLE_SIZE * channels[ch].baseFrequency) / SAMPLE_RATE;
                        channels[ch].readPointer += step;
                    }

                    if (channels[ch].readPointer >= WAVETABLE_SIZE) {
                        channels[ch].readPointer -= WAVETABLE_SIZE;
                        memcpy(channels[ch].wavetableFront,
                               channels[ch].wavetableBack,
                               sizeof(float) * WAVETABLE_SIZE);
                    }

                    int i1 = (int)channels[ch].readPointer;
                    int i2 = (i1 + 1) % WAVETABLE_SIZE;
                    float frac = channels[ch].readPointer - i1;

                    float voice = channels[ch].wavetableFront[i1] * (1.0f - frac) +
                                  channels[ch].wavetableFront[i2] * frac;
                    voice *= channels[ch].envVolume;

                    float panL = fmaxf(0.0f, 1.0f - channels[ch].pan);
                    float panR = fmaxf(0.0f, 1.0f + channels[ch].pan);
                    mixL += voice * panL;
                    mixR += voice * panR;
                }

                /* normalise to 16‑bit range */
                mixL = (mixL / 4.0f) * 32767.0f * 0.40f;
                mixR = (mixR / 4.0f) * 32767.0f * 0.40f;

                if (writeIdx + 2 > AUDIO_BUFFER_SIZE * 2) break;   // safety guard
                audioWriteBuffer[writeIdx++] = (short)mixL;
                audioWriteBuffer[writeIdx++] = (short)mixR;
            }
            UpdateAudioStream(audioStream, audioWriteBuffer, AUDIO_BUFFER_SIZE);
        }

        /* ----- rendering ----- */
        BeginDrawing();
        ClearBackground(GetColor(0x0A0A0AFF));

        for (int ch = 0; ch < 4; ++ch) {
            if (ch > 0) DrawLine(0, zoneHeight * ch, WIDTH, zoneHeight * ch, GetColor(0x222222FF));

            NurbsCurveHandle render = CreateFunctionCurve(channels[ch].samples, NUM_SAMPLES);
            Vec2 prev = EvaluateCurve(render, 0.0f);
            for (int i = 1; i <= (int)CURVE_RESOLUTION; ++i) {
                float t = (float)i / CURVE_RESOLUTION;
                Vec2 cur = EvaluateCurve(render, t);
                Color col = channels[ch].active ? channels[ch].color : GetColor(0x222222FF);
                DrawLineEx((Vector2){prev.x, prev.y},
                           (Vector2){cur.x, cur.y},
                           channels[ch].active ? 2.0f : 1.0f,
                           col);
                prev = cur;
            }
            DestroyCurve(render);

            for (int i = 0; i < NUM_SAMPLES; ++i) {
                DrawCircle(channels[ch].samples[i].x,
                           channels[ch].samples[i].y,
                           1.5f,
                           channels[ch].active ? WHITE : GetColor(0x333333FF));
            }

            const char *stageNames[] = { "IDLE", "ATTACK", "DECAY", "SUSTAIN", "RELEASE" };
            DrawTextEx(pixelFont,
                       TextFormat("[%d] CH: %3.1fHz %s - Env: %s (Gain: %.2f)",
                                  ch + 1,
                                  channels[ch].baseFrequency,
                                  channels[ch].active ? "[EDIT ENABLED]" : "[MUTED]",
                                  stageNames[channels[ch].envStage],
                                  channels[ch].envVolume),
                       (Vector2){30, zoneHeight * ch + 15},
                       FONT_SIZE,
                       1,
                       channels[ch].active ? channels[ch].color : GetColor(0x444444FF));
        }

        DrawCircleLines(mousePos.x, mousePos.y, brushRadius, GetColor(0x44444455));
        DrawTextEx(pixelFont,
                   TextFormat("FRAME: %06d", frameCount),
                   (Vector2){WIDTH - 120, 15},
                   FONT_SIZE,
                   1,
                   GetColor(0x555555FF));

        EndDrawing();
        ++frameCount;
    }

    UnloadAudioStream(audioStream);
    free(audioWriteBuffer);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
