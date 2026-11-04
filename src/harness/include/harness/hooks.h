#ifndef HARNESS_HOOKS_H
#define HARNESS_HOOKS_H

#include "brender.h"
#include <stdio.h>

typedef enum tHarness_window_type {
    eWindow_type_software = 0,
    eWindow_type_opengl = 1,
} tHarness_window_type;

// Gamepad buttons, in the same order as SDL's game controller buttons (Xbox layout)
enum {
    eGamepad_button_a = 1 << 0,
    eGamepad_button_b = 1 << 1,
    eGamepad_button_x = 1 << 2,
    eGamepad_button_y = 1 << 3,
    eGamepad_button_back = 1 << 4,
    eGamepad_button_guide = 1 << 5,
    eGamepad_button_start = 1 << 6,
    eGamepad_button_left_stick = 1 << 7,
    eGamepad_button_right_stick = 1 << 8,
    eGamepad_button_left_shoulder = 1 << 9,
    eGamepad_button_right_shoulder = 1 << 10,
    eGamepad_button_dpad_up = 1 << 11,
    eGamepad_button_dpad_down = 1 << 12,
    eGamepad_button_dpad_left = 1 << 13,
    eGamepad_button_dpad_right = 1 << 14,
};

typedef struct tHarness_gamepad_state {
    int connected;
    br_uint_32 buttons; // eGamepad_button_* bits
    // sticks: -1..1, positive is right/down
    float left_x, left_y;
    float right_x, right_y;
    // triggers: 0..1
    float left_trigger, right_trigger;
} tHarness_gamepad_state;

// Platform implementation functions
typedef struct tHarness_platform {
    // Render a fullscreen quad using the specified pixel data
    void (*Renderer_Present)(br_pixelmap* src);
    // Set the 256 color palette to use (BGRA format)
    void (*Renderer_SetPalette)(br_colour* palette);
    // Get mouse button state
    int (*GetMouseButtons)(int* button_1, int* button_2);
    // Get mouse position
    int (*GetMousePosition)(int* pX, int* pY);
    // Close specified window
    void (*DestroyWindow)(void);
    // Process window messages, return any WM_QUIT message
    void (*ProcessWindowMessages)(void);
    // Set position of a window
    int (*SetWindowPos)(void* hWnd, int x, int y, int nWidth, int nHeight);
    // Show/hide the cursor
    int (*ShowCursor)(int show);
    // invoked when key is set up or down
    void (*SetKeyHandler)(void (*handler_func)(void));
    // Get keyboard state. Argument expected to point to 32 byte buffer - 1 bit per key
    void (*GetKeyboardState)(br_uint_32* buffer);

    // Sleep
    void (*Sleep)(br_uint_32 dwMilliseconds);
    // Get ticks
    br_uint_32 (*GetTicks)(void);
    // Show error message
    int (*ShowErrorMessage)(char* title, char* message);

    // Create a window. Uses an underscore to avoid name collisions with windows.h `CreateWindow` macro
    void (*CreateWindow_)(const char* title, int nWidth, int nHeight, tHarness_window_type window_type);
    void (*Swap)(br_pixelmap* back_buffer);
    void (*PaletteChanged)(br_colour entries[256]);
    // If this platform supports OpenGL
    void* (*GL_GetProcAddress)(const char* name);
    void (*GetViewport)(int* x, int* y, float* width_multiplier, float* height_multiplier);
    // Get the state of the first connected gamepad. Optional (may be NULL)
    void (*GetGamepadState)(tHarness_gamepad_state* state);
    // 0..1 strength of the low (heavy) and high (light) frequency motors
    void (*SetGamepadRumble)(float low_frequency, float high_frequency, unsigned duration_ms);

} tHarness_platform;

enum {
    ePlatform_cap_software = 0x1,
    ePlatform_cap_opengl = 0x2,
    ePlatform_cap_video_mask = ePlatform_cap_software | ePlatform_cap_opengl,
};

typedef struct tPlatform_bootstrap {
    const char* name;
    const char* description;
    int capabilities;
    int (*init)(tHarness_platform* platform);
} tPlatform_bootstrap;

extern tHarness_platform gHarness_platform;

extern int Harness_Init(int* argc, char* argv[]);

extern void Harness_Quit(void);

// Hooks are called from original game code.

// Filesystem hooks
FILE* Harness_Hook_fopen(const char* pathname, const char* mode);

// Localization
int Harness_Hook_isalnum(int c);

// Scale things based on frame time, allowing us to run physics every frame
int Harness_Hook_ScaleProbabilityWithDt(int min_inclusive, int max_inclusive, float dt_seconds);

// Widescreen (OpenGL mode). Returns the factor to widen the camera aspect ratio by (1.0 if not extended).
float Harness_Hook_WidescreenBeginView(br_pixelmap* back_screen, br_pixelmap* render_screen, int allow);
void Harness_Hook_WidescreenEndView(void);

// The window lost keyboard focus (eg. alt-tab). Harness_ConsumeFocusLost returns 1 once per loss when
// PauseOnFocusLoss is enabled
void Harness_Hook_FocusLost(void);
int Harness_ConsumeFocusLost(void);
void Harness_Hook_WidescreenFillBackground(br_pixelmap* pixelmap, br_uint_16 colour_565);
// glrend `adjust_render_viewport` callback
void Harness_GL_AdjustRenderViewport(br_pixelmap* target, int* x, int* y, int* width, int* height);

// Widescreen HUD: while the 3d view of the current frame is extended, HUD elements towards the sides of the
// 640x480 interface are moved out to the window edges.
// Horizontal shift (interface pixels) for an element spanning `left` to `right`: 0 when not moving it
float Harness_Hook_HudShift(int left, int right);
// The overlay (2d layer) pixels in this rectangle belong to an element moved by `shift`
void Harness_Hook_HudRegion(int left, int top, int right, int bottom, float shift);
// Draw the 2d layer into the moved layer (on), which the overlay regions' shifts apply to, or back into the
// main one. Around the drawing of the HUD elements that move (with Harness_Hook_HudRegion)
void Harness_Hook_HudLayer(int on);
// Around rendering the 2d camera (dimmed boxes, pratcam): widens its viewport to the window, so moved
// elements are not clipped. Returns the factor to widen the 2d camera by (1.0 if not widened) and sets
// `x_offset` to the distance to move the camera by (interface pixels; it maps from the left edge)
float Harness_Hook_Hud2dBegin(float* x_offset);
void Harness_Hook_Hud2dEnd(void);
// Fade geometry out towards the yon distance (DistanceFade), while on. For views without a depth effect,
// with the sky drawn first so the geometry fades into it
void Harness_Hook_DistanceFade(int on);

// Post-processing of the 3d view (OpenGL mode, depth of field). Begin returns whether it is in effect;
// End is then called with the view camera's depth range
int Harness_Hook_PostProcessBegin(br_pixelmap* screen);
// While post-processing: the world drawn so far is what the ambient occlusion is made from (not the
// pedestrian sprites, smoke and sparks drawn after it)
void Harness_Hook_PostProcessDepthSnapshot(void);
void Harness_Hook_SoftParticles(int on);
// speed: 0..1 for the speed blur; fires: camera space position and strength of up to 8 fires (heat shimmer)
void Harness_Hook_PostProcessEnd(br_pixelmap* screen, float hither, float yon, float speed, int night, const float* up, const float* to_world, const float* fires, int fire_count);
// The surfaces with this texture mirror their surroundings (ice) in the OpenGL renderer; NULL forgets them all
void Harness_Hook_ReflectiveTexture(const char* identifier);
void Harness_Hook_CarTexture(const char* identifier);
float Harness_GL_TextureReflect(const char* identifier);

// Night (OpenGL mode): the 3d scenes are darkened and lit by lights (headlights, fires)
// brightness 1 = normal; lights are BR_GL_SCENE_LIGHT_FLOATS floats each, see glrend's scene_lighting callback
void Harness_Hook_SceneLighting(float brightness, const float* lights, int count);
// Darken the 3d scenes begun from now on by this factor (1 = normal), on top of the scene lighting
void Harness_Hook_SceneShade(float shade);
// A car's texture: tail lights glow brighter while braking
void Harness_Hook_CarLightGlow(const char* identifier, int braking);
// Creates a directory (if it does not exist)
void Harness_Hook_MakeDirectory(const char* path);

// Decides whether this frame is at night (Night = 1, or auto and the view has the darkness depth cue)
int Harness_Hook_Night(int pDarkness);

// glrend callbacks
int Harness_GL_SceneLighting(float* brightness, const float** lights);
// The sun and up directions in the camera's space, for the sun shading and surface variation of the unlit
// surfaces (see glrend's scene_surface callback)
// haze_colour: NULL for no haze (on tracks with fog or darkness)
void Harness_Hook_SceneSurface(const float* sun, const float* up, int night, const float* haze_colour, float haze_distance);
int Harness_GL_SceneSurface(float* params);
// While drawing the sky (which moves with the camera): no surface shading, and no depth written, so the
// effects reading depth (ambient occlusion, depth of field) take it for infinitely far
void Harness_Hook_DrawingSky(int sky);
int Harness_GL_DepthWrite(void);
float Harness_GL_TextureGlow(const char* identifier, const unsigned char** indices, const short** rects, int* rect_count, int* id);
float Harness_GL_GlowScale(int id);
float Harness_GL_DistanceFade(void);
int Harness_GL_OverlayRegions(const float** regions);
void Harness_GL_SwapBuffers(br_pixelmap* pm);
int Harness_Hook_ScaleEmissionCountWithDt(float count, float dt_seconds);

#endif
