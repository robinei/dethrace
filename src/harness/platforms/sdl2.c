#include <SDL.h>
#include <string.h>

#include "harness.h"
#include "harness/config.h"
#include "harness/hooks.h"
#include "harness/trace.h"
#include "sdl2_scancode_map.h"
#include "sdl2_syms.h"

SDL_COMPILE_TIME_ASSERT(sdl2_platform_requires_SDL2, SDL_MAJOR_VERSION == 2);

static SDL_Window* window;
// the map's own window (MapWindow): no OpenGL, the image drawn into its surface; the image it shows (the
// harness's), so it can be drawn again when the window changes; closed by the user: not opened again
static SDL_Window* map_window;
static SDL_Renderer* map_renderer;
static SDL_Texture* map_texture;
static int map_texture_width, map_texture_height;
static const br_uint_32* map_window_pixels;
static int map_window_width, map_window_height;
static int map_window_closed;
static SDL_Renderer* renderer;
static SDL_Texture* screen_texture;
static br_uint_32 converted_palette[256];
static br_pixelmap* last_screen_src;

static SDL_GLContext* gl_context;

static int render_width, render_height;

static Uint32 last_frame_time;

static void (*gKeyHandler_func)(void);
static SDL_GameController* game_controller;

// 32 bytes, 1 bit per key. Matches dos executable behavior
static br_uint_32 key_state[8];

static struct {
    int x, y;
    float scale_x, scale_y;
} viewport;

// Callbacks back into original game code
extern void QuitGame(void);
extern br_pixelmap* gBack_screen;

#ifdef DETHRACE_SDL_DYNAMIC
#ifdef _WIN32
static const char* const possible_locations[] = {
    "SDL2.dll",
};
#elif defined(__APPLE__)
#define SHARED_OBJECT_NAME "libSDL2"
#define SDL2_LIBNAME "libSDL2.dylib"
#define SDL2_FRAMEWORK "SDL2.framework/Versions/A/SDL2"
static const char* const possible_locations[] = {
    "@loader_path/" SDL2_LIBNAME,                     /* MyApp.app/Contents/MacOS/libSDL2_dylib */
    "@loader_path/../Frameworks/" SDL2_FRAMEWORK,     /* MyApp.app/Contents/Frameworks/SDL2_framework */
    "@executable_path/" SDL2_LIBNAME,                 /* MyApp.app/Contents/MacOS/libSDL2_dylib */
    "@executable_path/../Frameworks/" SDL2_FRAMEWORK, /* MyApp.app/Contents/Frameworks/SDL2_framework */
    "/Library/Frameworks/" SDL2_FRAMEWORK,            /* /Library/Frameworks/SDL2_framework */
    SDL2_LIBNAME                                      /* oh well, anywhere the system can see the .dylib (/usr/local/lib or whatever) */
};
#else
#include "elfdlopennote.h"
#ifdef ELF_NOTE_DLOPEN
ELF_NOTE_DLOPEN(
    "SDL2",
    "Platform-specific operations such as creating windows and handling events",
    ELF_NOTE_DLOPEN_PRIORITY_SUGGESTED,
    "libSDL2-2.0.so.0",
    "libSDL2-2.0.so");
#endif
static const char* const possible_locations[] = {
    "libSDL2-2.0.so.0",
    "libSDL2-2.0.so",
};
#endif

static void* sdl2_so;
#endif

#define SDL_NAME "SDL2"
#define OBJECT_NAME sdl2_so
#define SYMBOL_PREFIX SDL2_
#define FOREACH_SDLX_SYM FOREACH_SDL2_SYM

#include "sdl_dyn_common.h"

static void calculate_viewport(int window_width, int window_height) {
    int vp_width, vp_height;
    float target_aspect_ratio;
    float aspect_ratio;

    aspect_ratio = (float)window_width / window_height;
    target_aspect_ratio = (float)gBack_screen->width / gBack_screen->height;

    vp_width = window_width;
    vp_height = window_height;
    if (aspect_ratio != target_aspect_ratio) {
        if (aspect_ratio > target_aspect_ratio) {
            vp_width = window_height * target_aspect_ratio + .5f;
        } else {
            vp_height = window_width / target_aspect_ratio + .5f;
        }
    }
    viewport.x = (window_width - vp_width) / 2;
    viewport.y = (window_height - vp_height) / 2;
    viewport.scale_x = (float)vp_width / gBack_screen->width;
    viewport.scale_y = (float)vp_height / gBack_screen->height;
}

static int SDL2_Harness_SetWindowPos(void* hWnd, int x, int y, int nWidth, int nHeight) {
    // SDL_SetWindowPosition(hWnd, x, y);
    if (nWidth == 320 && nHeight == 200) {
        nWidth = 640;
        nHeight = 400;
    }
    SDL2_SetWindowSize(hWnd, nWidth, nHeight);
    return 0;
}

static void SDL2_Harness_DestroyWindow(void) {
    // SDL2_GL_DeleteContext(context);
    if (window != NULL) {
        SDL2_DestroyWindow(window);
    }
    SDL2_Quit();
    window = NULL;
}

// Checks whether the `flag_check` is the only modifier applied.
// e.g. is_only_modifier(event.key.keysym.mod, KMOD_ALT) returns true when only the ALT key was pressed
static int is_only_key_modifier(int modifier_flags, int flag_check) {
    return (modifier_flags & flag_check) && (modifier_flags & (KMOD_CTRL | KMOD_SHIFT | KMOD_ALT | KMOD_GUI)) == (modifier_flags & flag_check);
}

static void open_first_game_controller(void) {
    int i;

    for (i = 0; i < SDL2_NumJoysticks(); i++) {
        if (SDL2_IsGameController(i)) {
            game_controller = SDL2_GameControllerOpen(i);
            if (game_controller != NULL) {
                LOG_INFO2("Using game controller \"%s\"", SDL2_GameControllerName(game_controller));
                return;
            }
        }
    }
}

static void map_window_draw(void) {
    if (map_window == NULL || map_renderer == NULL || map_window_pixels == NULL) {
        return;
    }
    if (map_texture == NULL || map_texture_width != map_window_width || map_texture_height != map_window_height) {
        if (map_texture != NULL) {
            SDL2_DestroyTexture(map_texture);
        }
        map_texture = SDL2_CreateTexture(map_renderer, SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_STREAMING, map_window_width, map_window_height);
        if (map_texture == NULL) {
            return;
        }
        map_texture_width = map_window_width;
        map_texture_height = map_window_height;
        // scaled to fit, centred, black around
        SDL2_RenderSetLogicalSize(map_renderer, map_window_width, map_window_height);
    }
    SDL2_UpdateTexture(map_texture, NULL, map_window_pixels, map_window_width * 4);
    SDL2_RenderClear(map_renderer);
    SDL2_RenderCopy(map_renderer, map_texture, NULL, NULL);
    SDL2_RenderPresent(map_renderer);
    // (the renderer draws with a GL context of its own: the game's current again)
    if (gl_context != NULL) {
        SDL2_GL_MakeCurrent(window, gl_context);
    }
}

// opens the map's window (black until there is something to show), once
static void map_window_open(void) {
    if (map_window != NULL || map_window_closed) {
        return;
    }
    // (kept above the game where the windowing system lets programs ask for it: not on Wayland, where it is the
    // compositor's rule to make)
    map_window = SDL2_CreateWindow("Dethrace map", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640, 400, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALWAYS_ON_TOP);
    if (map_window == NULL) {
        LOG_WARN2("Failed to create the map's window: %s", SDL2_GetError());
        map_window_closed = 1;
        return;
    }
    // (drawn with a renderer: SDL has no window surfaces on Wayland; not waiting for vsync)
    map_renderer = SDL2_CreateRenderer(map_window, -1, 0);
    if (map_renderer == NULL) {
        LOG_WARN2("No renderer for the map's window: %s", SDL2_GetError());
    } else {
        SDL2_RenderClear(map_renderer);
        SDL2_RenderPresent(map_renderer);
    }
    // (the game keeps the focus, and its GL context)
    SDL2_RaiseWindow(window);
    if (gl_context != NULL) {
        SDL2_GL_MakeCurrent(window, gl_context);
    }
}

static void map_window_close(void) {
    if (map_texture != NULL) {
        SDL2_DestroyTexture(map_texture);
        map_texture = NULL;
    }
    if (map_renderer != NULL) {
        SDL2_DestroyRenderer(map_renderer);
        map_renderer = NULL;
    }
    if (map_window != NULL) {
        SDL2_DestroyWindow(map_window);
        map_window = NULL;
    }
    if (gl_context != NULL) {
        SDL2_GL_MakeCurrent(window, gl_context);
    }
}

static void SDL2_Harness_MapWindow(const br_uint_32* pixels, int width, int height) {
    if (pixels == NULL) {
        map_window_close();
        return;
    }
    map_window_pixels = pixels;
    map_window_width = width;
    map_window_height = height;
    map_window_open();
    map_window_draw();
}

static int SDL2_Harness_DisplayCount(void) {
    return SDL2_GetNumVideoDisplays();
}

static void SDL2_Harness_ProcessWindowMessages(void) {
    SDL_Event event;

    while (SDL2_PollEvent(&event)) {
        switch (event.type) {
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            if (event.key.windowID != SDL2_GetWindowID(window)) {
                continue;
            }
            if (event.key.keysym.sym == SDLK_RETURN) {
                if (event.key.type == SDL_KEYDOWN) {
                    if ((event.key.keysym.mod & (KMOD_CTRL | KMOD_SHIFT | KMOD_ALT | KMOD_GUI))) {
                        // Ignore keydown of RETURN when used together with some modifier
                        return;
                    }
                } else if (event.key.type == SDL_KEYUP) {
                    if (is_only_key_modifier(event.key.keysym.mod, KMOD_ALT)) {
                        SDL2_SetWindowFullscreen(window, (SDL2_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP) ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
                    }
                }
            }

            // Map incoming SDL scancode to PC scan code as used by game code
            int dethrace_scancode = sdl_scancode_map[event.key.keysym.scancode];
            if (dethrace_scancode == 0) {
                // once per key (eg. the Super key held for a screenshot shortcut repeats)
                static unsigned char warned[SDL_NUM_SCANCODES];
                if (event.key.keysym.scancode < SDL_NUM_SCANCODES && !warned[event.key.keysym.scancode]) {
                    warned[event.key.keysym.scancode] = 1;
                    LOG_WARN3("unexpected scan code %s (%d)", SDL2_GetScancodeName(event.key.keysym.scancode), event.key.keysym.scancode);
                }
                return;
            }

            if (event.type == SDL_KEYDOWN) {
                key_state[dethrace_scancode >> 5] |= (1 << (dethrace_scancode & 0x1F));
            } else {
                key_state[dethrace_scancode >> 5] &= ~(1 << (dethrace_scancode & 0x1F));
            }
            gKeyHandler_func();
            break;

        case SDL_WINDOWEVENT:
            if (map_window != NULL && event.window.windowID == SDL2_GetWindowID(map_window)) {
                // the map's window: drawn again as it changes; closed, it stays closed
                if (event.window.event == SDL_WINDOWEVENT_CLOSE) {
                    map_window_close();
                    map_window_closed = 1;
                } else if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED || event.window.event == SDL_WINDOWEVENT_EXPOSED) {
                    map_window_draw();
                }
                break;
            }
            if (event.window.windowID != SDL2_GetWindowID(window)) {
                break;
            }
            if (event.window.event == SDL_WINDOWEVENT_CLOSE && map_window != NULL) {
                // (with the map's window open, closing the game's doesn't end SDL's last window)
                QuitGame();
            } else if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                calculate_viewport(event.window.data1, event.window.data2);
            } else if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                Harness_Hook_FocusLost();
            } else if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
                // a loss outside a race (eg. in the menus) should not pause the next race
                Harness_ConsumeFocusLost();
            }
            break;

        case SDL_CONTROLLERDEVICEADDED:
            if (game_controller == NULL) {
                open_first_game_controller();
            }
            break;
        case SDL_CONTROLLERDEVICEREMOVED:
            if (game_controller != NULL && event.cdevice.which == SDL2_JoystickInstanceID(SDL2_GameControllerGetJoystick(game_controller))) {
                SDL2_GameControllerClose(game_controller);
                game_controller = NULL;
                open_first_game_controller();
            }
            break;
        case SDL_QUIT:
            QuitGame();
        }
    }
}

static float controller_axis(SDL_GameControllerAxis axis) {
    Sint16 value = SDL2_GameControllerGetAxis(game_controller, axis);
    return value < 0 ? value / 32768.0f : value / 32767.0f;
}

static void SDL2_Harness_GetGamepadState(tHarness_gamepad_state* state) {
    int i;

    memset(state, 0, sizeof(*state));
    if (game_controller == NULL) {
        return;
    }
    state->connected = 1;
    for (i = SDL_CONTROLLER_BUTTON_A; i <= SDL_CONTROLLER_BUTTON_DPAD_RIGHT; i++) {
        if (SDL2_GameControllerGetButton(game_controller, (SDL_GameControllerButton)i)) {
            state->buttons |= 1u << i;
        }
    }
    state->left_x = controller_axis(SDL_CONTROLLER_AXIS_LEFTX);
    state->left_y = controller_axis(SDL_CONTROLLER_AXIS_LEFTY);
    state->right_x = controller_axis(SDL_CONTROLLER_AXIS_RIGHTX);
    state->right_y = controller_axis(SDL_CONTROLLER_AXIS_RIGHTY);
    state->left_trigger = controller_axis(SDL_CONTROLLER_AXIS_TRIGGERLEFT);
    state->right_trigger = controller_axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
}

static void SDL2_Harness_SetGamepadRumble(float low_frequency, float high_frequency, unsigned duration_ms) {
    if (game_controller == NULL) {
        return;
    }
    if (low_frequency < 0.f) low_frequency = 0.f;
    if (low_frequency > 1.f) low_frequency = 1.f;
    if (high_frequency < 0.f) high_frequency = 0.f;
    if (high_frequency > 1.f) high_frequency = 1.f;
    SDL2_GameControllerRumble(game_controller, (Uint16)(low_frequency * 65535.f), (Uint16)(high_frequency * 65535.f), duration_ms);
}

static void SDL2_Harness_SetKeyHandler(void (*handler_func)(void)) {
    gKeyHandler_func = handler_func;
}

static void SDL2_Harness_GetKeyboardState(br_uint_32* buffer) {
    memcpy(buffer, key_state, sizeof(key_state));
}

static int SDL2_Harness_GetMouseButtons(int* pButton1, int* pButton2) {
    if (SDL2_GetMouseFocus() != window) {
        *pButton1 = 0;
        *pButton2 = 0;
        return 0;
    }
    int state = SDL2_GetMouseState(NULL, NULL);
    *pButton1 = state & SDL_BUTTON_LMASK;
    *pButton2 = state & SDL_BUTTON_RMASK;
    return 0;
}

static int SDL2_Harness_GetMousePosition(int* pX, int* pY) {
    int window_width, window_height;
    float lX, lY;

    if (SDL2_GetMouseFocus() != window) {
        return 0;
    }
    SDL2_GetWindowSize(window, &window_width, &window_height);

    SDL2_GetMouseState(pX, pY);
    if (renderer != NULL) {
        // software renderer
        SDL2_RenderWindowToLogical(renderer, *pX, *pY, &lX, &lY);
    } else {
        // hardware renderer
        // handle case where window is stretched larger than the pixel size
        lX = *pX * (640.0f / window_width);
        lY = *pY * (480.0f / window_height);
    }
    *pX = (int)lX;
    *pY = (int)lY;
    return 0;
}

static void limit_fps(void) {
    Uint32 now = SDL2_GetTicks();
    if (last_frame_time != 0) {
        unsigned int frame_time = now - last_frame_time;
        last_frame_time = now;
        if (frame_time < 100) {
            int sleep_time = (1000 / harness_game_config.fps) - frame_time;
            if (sleep_time > 5) {
                gHarness_platform.Sleep(sleep_time);
            }
        }
    }
    last_frame_time = SDL2_GetTicks();
}

static int SDL2_Harness_ShowErrorMessage(char* title, char* message) {
    fprintf(stderr, "%s", message);
    SDL2_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, message, window);
    return 0;
}

// Supersampling: shade every sample of the multisampled framebuffer instead of once per pixel
// (GL 4.0 / ARB_sample_shading). Textures and colour-keyed edges are then anti-aliased too.
static void enable_sample_shading(void) {
    typedef void (*get_integerv_t)(unsigned, int*);
    typedef const unsigned char* (*get_stringi_t)(unsigned, unsigned);
    typedef void (*min_sample_shading_t)(float);
    typedef void (*enable_t)(unsigned);
    get_integerv_t get_integerv = (get_integerv_t)SDL2_GL_GetProcAddress("glGetIntegerv");
    get_stringi_t get_stringi = (get_stringi_t)SDL2_GL_GetProcAddress("glGetStringi");
    enable_t enable = (enable_t)SDL2_GL_GetProcAddress("glEnable");
    min_sample_shading_t min_sample_shading = NULL;
    int major = 0, sample_buffers = 0, extensions = 0, i;

    get_integerv(0x80A8 /* GL_SAMPLE_BUFFERS */, &sample_buffers);
    if (!sample_buffers) {
        LOG_WARN("Supersampling needs MSAA to be enabled");
        return;
    }
    get_integerv(0x821B /* GL_MAJOR_VERSION */, &major);
    if (major >= 4) {
        min_sample_shading = (min_sample_shading_t)SDL2_GL_GetProcAddress("glMinSampleShading");
    } else if (get_stringi != NULL) {
        get_integerv(0x821D /* GL_NUM_EXTENSIONS */, &extensions);
        for (i = 0; i < extensions; i++) {
            if (strcmp((const char*)get_stringi(0x1F03 /* GL_EXTENSIONS */, i), "GL_ARB_sample_shading") == 0) {
                min_sample_shading = (min_sample_shading_t)SDL2_GL_GetProcAddress("glMinSampleShadingARB");
                break;
            }
        }
    }
    if (min_sample_shading == NULL) {
        LOG_WARN("Supersampling is not supported by this OpenGL driver");
        return;
    }
    enable(0x8C36 /* GL_SAMPLE_SHADING */);
    min_sample_shading(1.0f);
}

static void SDL2_Harness_CreateWindow(const char* title, int width, int height, tHarness_window_type window_type) {
    int window_width, window_height;
    Uint32 extra_window_flags;

    render_width = width;
    render_height = height;

    window_width = width;
    window_height = height;

    // special case lores and make a bigger window
    if (width == 320 && height == 200) {
        window_width = 640;
        window_height = 480;
    }

    if (SDL2_Init(SDL_INIT_VIDEO) != 0) {
        LOG_PANIC2("SDL_INIT_VIDEO error: %s", SDL2_GetError());
    }
    // Controllers already connected are reported through SDL_CONTROLLERDEVICEADDED events
    if (SDL2_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) {
        LOG_WARN2("SDL_INIT_GAMECONTROLLER error: %s", SDL2_GetError());
    }

    extra_window_flags = SDL_WINDOW_RESIZABLE;
    if (harness_game_config.start_full_screen) {
        extra_window_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    }

    if (window_type == eWindow_type_opengl) {

        // Framebuffer attributes must be set before the window is created (the visual is picked then).
        // SDL defaults to a 16-bit depth buffer, which z-fights badly with extended draw distances.
        SDL2_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
        if (harness_game_config.msaa_samples > 0) {
            SDL2_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
            SDL2_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, harness_game_config.msaa_samples);
        }
        window = SDL2_CreateWindow(title,
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            window_width, window_height,
            extra_window_flags | SDL_WINDOW_OPENGL);
        if (window == NULL && harness_game_config.msaa_samples > 0) {
            LOG_WARN2("Failed to create window with %dx MSAA, retrying without", harness_game_config.msaa_samples);
            SDL2_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
            SDL2_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
            window = SDL2_CreateWindow(title,
                SDL_WINDOWPOS_CENTERED,
                SDL_WINDOWPOS_CENTERED,
                window_width, window_height,
                extra_window_flags | SDL_WINDOW_OPENGL);
        }

        if (window == NULL) {
            LOG_PANIC2("Failed to create window: %s", SDL2_GetError());
        }

        SDL2_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL2_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL2_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
        gl_context = SDL2_GL_CreateContext(window);

        if (gl_context == NULL) {
            LOG_WARN2("Failed to create OpenGL core profile: %s. Trying OpenGLES...", SDL2_GetError());
            SDL2_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
            SDL2_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
            SDL2_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
            gl_context = SDL2_GL_CreateContext(window);
        }
        if (gl_context == NULL) {
            LOG_PANIC2("Failed to create OpenGL context: %s", SDL2_GetError());
        }
        // VSync = 1 (default), 0 (off), or adaptive: vsync, but a late frame is shown immediately (may tear)
        // instead of waiting for the next refresh. Falls back to normal vsync if the driver lacks it
        if (harness_game_config.vsync < 0) {
            if (SDL2_GL_SetSwapInterval(-1) != 0) {
                LOG_WARN("Adaptive vsync not supported, using normal vsync");
                SDL2_GL_SetSwapInterval(1);
            }
        } else {
            SDL2_GL_SetSwapInterval(harness_game_config.vsync ? 1 : 0);
        }
        if (harness_game_config.supersampling) {
            enable_sample_shading();
        }
        // the map's own window, from the start (MapWindow)
        if (Harness_MapWindowEnabled()) {
            map_window_open();
        }

    } else {
        window = SDL2_CreateWindow(title,
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            window_width, window_height,
            extra_window_flags);
        if (window == NULL) {
            LOG_PANIC2("Failed to create window: %s", SDL2_GetError());
        }

        renderer = SDL2_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC);
        if (renderer == NULL) {
            LOG_PANIC2("Failed to create renderer: %s", SDL2_GetError());
        }
        SDL2_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        SDL2_RenderSetLogicalSize(renderer, render_width, render_height);

        screen_texture = SDL2_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, width, height);
        if (screen_texture == NULL) {
            SDL_RendererInfo info;
            SDL2_GetRendererInfo(renderer, &info);
            for (Uint32 i = 0; i < info.num_texture_formats; i++) {
                LOG_INFO2("%s\n", SDL2_GetPixelFormatName(info.texture_formats[i]));
            }
            LOG_PANIC2("Failed to create screen_texture: %s", SDL2_GetError());
        }
    }

    SDL2_ShowCursor(SDL_DISABLE);

    viewport.x = 0;
    viewport.y = 0;
    viewport.scale_x = 1;
    viewport.scale_y = 1;
}

static void SDL2_Harness_Swap(br_pixelmap* back_buffer) {
    int i;
    int dest_pitch;
    uint8_t* src_pixels;
    br_uint_32* dest_pixels;

    SDL2_Harness_ProcessWindowMessages();
    Harness_MapWindowUpdate();

    if (gl_context != NULL) {
        SDL2_GL_SwapWindow(window);
    } else {
        src_pixels = back_buffer->pixels;

        SDL2_LockTexture(screen_texture, NULL, (void**)&dest_pixels, &dest_pitch);
        for (i = 0; i < back_buffer->height * back_buffer->width; i++) {
            *dest_pixels = converted_palette[*src_pixels];
            dest_pixels++;
            src_pixels++;
        }
        SDL2_UnlockTexture(screen_texture);
        SDL2_RenderClear(renderer);
        SDL2_RenderCopy(renderer, screen_texture, NULL, NULL);
        SDL2_RenderPresent(renderer);
        last_screen_src = back_buffer;
    }

    if (harness_game_config.fps != 0) {
        limit_fps();
    }
}

static void SDL2_Harness_PaletteChanged(br_colour entries[256]) {
    int i;
    for (i = 0; i < 256; i++) {
        converted_palette[i] = (0xffu << 24 | BR_RED(entries[i]) << 16 | BR_GRN(entries[i]) << 8 | BR_BLU(entries[i]));
    }
    if (last_screen_src != NULL) {
        SDL2_Harness_Swap(last_screen_src);
    }
}

static void SDL2_Harness_GetViewport(int* x, int* y, float* width_multipler, float* height_multiplier) {
    *x = viewport.x;
    *y = viewport.y;
    *width_multipler = viewport.scale_x;
    *height_multiplier = viewport.scale_y;
}

static int SDL2_Harness_Platform_Init(tHarness_platform* platform) {
    if (SDL2_LoadSymbols() != 0) {
        return 1;
    }
    platform->ProcessWindowMessages = SDL2_Harness_ProcessWindowMessages;
    platform->Sleep = SDL2_Delay;
    platform->GetTicks = SDL2_GetTicks;
    platform->ShowCursor = SDL2_ShowCursor;
    platform->SetWindowPos = SDL2_Harness_SetWindowPos;
    platform->DestroyWindow = SDL2_Harness_DestroyWindow;
    platform->SetKeyHandler = SDL2_Harness_SetKeyHandler;
    platform->GetKeyboardState = SDL2_Harness_GetKeyboardState;
    platform->GetMousePosition = SDL2_Harness_GetMousePosition;
    platform->GetMouseButtons = SDL2_Harness_GetMouseButtons;
    platform->ShowErrorMessage = SDL2_Harness_ShowErrorMessage;

    platform->CreateWindow_ = SDL2_Harness_CreateWindow;
    platform->Swap = SDL2_Harness_Swap;
    platform->PaletteChanged = SDL2_Harness_PaletteChanged;
    platform->GL_GetProcAddress = SDL2_GL_GetProcAddress;
    platform->GetViewport = SDL2_Harness_GetViewport;
    platform->GetGamepadState = SDL2_Harness_GetGamepadState;
    platform->SetGamepadRumble = SDL2_Harness_SetGamepadRumble;
    platform->DisplayCount = SDL2_Harness_DisplayCount;
    platform->MapWindow = SDL2_Harness_MapWindow;
    return 0;
};

const tPlatform_bootstrap SDL2_bootstrap = {
    "sdl2",
    "SDL2 video backend (libsdl.org)",
    ePlatform_cap_software | ePlatform_cap_opengl,
    SDL2_Harness_Platform_Init,
};
