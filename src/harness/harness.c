#include "harness.h"
#include "../DETHRACE/constants.h"
#include "ascii_tables.h"
#include "include/harness/config.h"
#include "include/harness/hooks.h"
#include "include/harness/os.h"
#include "glow_table.h"
#include "ini.h"
#include "version.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#endif

extern br_uint_32 gI_am_cheating;
extern int gSound_override;
extern int gSausage_override;
extern int gGraf_spec_index;

extern void Harness_Platform_Init(tHarness_platform* platform);

extern const tPlatform_bootstrap SDL1_bootstrap;
extern const tPlatform_bootstrap SDL2_bootstrap;
extern const tPlatform_bootstrap SDL3_bootstrap;

static const tPlatform_bootstrap* platform_bootstraps[] = {
#if defined(DETHRACE_PLATFORM_SDL3)
    &SDL3_bootstrap,
#define HAS_PLATFORM_BOOTSTRAP
#endif
#if defined(DETHRACE_PLATFORM_SDL2)
    &SDL2_bootstrap,
#define HAS_PLATFORM_BOOTSTRAP
#endif
#if defined(DETHRACE_PLATFORM_SDL1)
    &SDL1_bootstrap,
#define HAS_PLATFORM_BOOTSTRAP
#endif
#ifndef HAS_PLATFORM_BOOTSTRAP
    // This is the case for MSVC 4.20 builds
    NULL
#endif
};

#define safe_strcpy(DEST, SRC)                \
    do {                                      \
        strncpy((DEST), (SRC), sizeof(DEST)); \
        (DEST)[sizeof(DEST) - 1] = '\0';      \
    } while (0)

// SplatPack or Carmageddon. This is where we represent the code differences between the two. For example, the intro smack file.
tHarness_game_info harness_game_info;

// Configuration options
tHarness_game_config harness_game_config;

// Platform hooks
tHarness_platform gHarness_platform;

static int Harness_ProcessCommandLine(int* argc, char* argv[]);
static int Harness_ProcessIniFile(void);

static int Harness_InitPlatform(void) {
    int required_caps = 0;

    const tPlatform_bootstrap* selected_bootstrap = NULL;

    if (harness_game_config.opengl_3dfx_mode) {
        required_caps &= ~ePlatform_cap_video_mask;
        required_caps |= ePlatform_cap_opengl;
    }

    if (strlen(harness_game_config.platform_name) != 0) {
        size_t i;
        for (i = 0; i < BR_ASIZE(platform_bootstraps); i++) {
            if (strcasecmp(platform_bootstraps[i]->name, harness_game_config.platform_name) == 0) {
                if ((platform_bootstraps[i]->capabilities & required_caps) != required_caps) {
                    fprintf(stderr, "Platform \"%s\" does not support required capabilities. Try another video driver and/or add/remove --opengl\n", platform_bootstraps[i]->name);
                    return 1;
                }
                selected_bootstrap = platform_bootstraps[i];
                break;
            }
        }
        if (selected_bootstrap == NULL) {
            fprintf(stderr, "Could not find a platform named \"%s\"\n", harness_game_config.platform_name);
            return 1;
        }
        if (selected_bootstrap->init(&gHarness_platform) != 0) {
            fprintf(stderr, "%s initialization failed\n", selected_bootstrap->name);
            return 1;
        }
    } else {
        size_t i;
        for (i = 0; i < BR_ASIZE(platform_bootstraps); i++) {
            LOG_DEBUG2("Attempting video driver \"%s\"", platform_bootstraps[i]->name);
            if ((platform_bootstraps[i]->capabilities & required_caps) != required_caps) {
                fprintf(stderr, "Skipping platform \"%s\". Does not support required capabilities.\n", platform_bootstraps[i]->name);
                continue;
            }
            LOG_DEBUG2("Try platform \"%s\"...", platform_bootstraps[i]->name);
            if (platform_bootstraps[i]->init(&gHarness_platform) == 0) {
                selected_bootstrap = platform_bootstraps[i];
                break;
            }
        }
        if (selected_bootstrap == NULL) {
            fprintf(stderr, "Could not find a supported platform\n");
            return 1;
        }
    }
    LOG_INFO3("Platform: %s (%s)", selected_bootstrap->name, selected_bootstrap->description);

    return 0;
}

static void Harness_DetectGameMode(void) {
    FILE* f;
    int filesize;
    char* buffer;
    int nb;

    if (access("DATA/RACES/CASTLE.TXT", F_OK) != -1) {
        // All splatpack edition have the castle track
        if (access("DATA/RACES/CASTLE2.TXT", F_OK) != -1) {
            // Only the full splat release has the castle2 track
            harness_game_info.defines.INTRO_SMK_FILE = "MIX_INTR.SMK";
            harness_game_info.defines.GERMAN_LOADSCRN = "LOADSCRN.PIX";
            harness_game_info.mode = eGame_splatpack;
            printf("Game mode: Splat Pack\n");
        } else if (access("DATA/RACES/TINSEL.TXT", F_OK) != -1) {
            // Only the the splat x-mas demo has the tinsel track
            harness_game_info.defines.INTRO_SMK_FILE = "MIX_INTR.SMK";
            harness_game_info.defines.GERMAN_LOADSCRN = "";
            harness_game_info.mode = eGame_splatpack_xmas_demo;
            printf("Game mode: Splat Pack X-mas demo\n");
        } else {
            // Assume we're using the splatpack demo
            harness_game_info.defines.INTRO_SMK_FILE = "MIX_INTR.SMK";
            harness_game_info.defines.GERMAN_LOADSCRN = "";
            harness_game_info.mode = eGame_splatpack_demo;
            printf("Game mode: Splat Pack demo\n");
        }
    } else if (access("DATA/RACES/CITYB3.TXT", F_OK) != -1) {
        // All non-splatpack edition have the cityb3 track
        if (access("DATA/RACES/CITYA1.TXT", F_OK) == -1) {
            // The demo does not have the citya1 track
            harness_game_info.defines.INTRO_SMK_FILE = "";
            harness_game_info.defines.GERMAN_LOADSCRN = "COWLESS.PIX";
            harness_game_info.mode = eGame_carmageddon_demo;
            printf("Game mode: Carmageddon demo\n");
        } else {
            goto carmageddon;
        }
    } else {
    carmageddon:
        if (access("DATA/CUTSCENE/Mix_intr.smk", F_OK) == -1) {
            harness_game_info.defines.INTRO_SMK_FILE = "Mix_intr.smk";
        } else {
            harness_game_info.defines.INTRO_SMK_FILE = "MIX_INTR.SMK";
        }
        harness_game_info.defines.GERMAN_LOADSCRN = "LOADSCRN.PIX";
        harness_game_info.mode = eGame_carmageddon;
        printf("Game mode: Carmageddon\n");
    }

    harness_game_info.localization = eGameLocalization_none;
    if (access("DATA/TRNSLATE.TXT", F_OK) != -1) {
        f = fopen("DATA/TRNSLATE.TXT", "rb");
        fseek(f, 0, SEEK_END);
        filesize = ftell(f);
        fseek(f, 0, SEEK_SET);
        buffer = malloc(filesize + 1);
        nb = fread(buffer, 1, filesize, f);
        if (nb != filesize) {
            LOG_PANIC("Unable to read DATA/TRNSLATE.TXT");
        }
        buffer[filesize] = '\0';
        fclose(f);
        if (strstr(buffer, "NEUES SPIEL") != NULL) {
            harness_game_info.localization = eGameLocalization_german;
            LOG_INFO2("Language: \"%s\"", "German");
        } else if (strstr(buffer, "NOWA GRA") != NULL) {
            harness_game_info.localization = eGameLocalization_polish;
            LOG_INFO2("Language: \"%s\"", "Polish");
        } else if (strstr(buffer, "NOUVELLE PARTIE") != NULL) {
            harness_game_info.localization = eGameLocalization_french;
            LOG_INFO2("Language: \"%s\"", "French");
        } else {
            LOG_INFO("Language: unrecognized");
        }
        free(buffer);
    }

    switch (harness_game_info.mode) {
    case eGame_carmageddon:
        switch (harness_game_info.localization) {
        case eGameLocalization_german:
            harness_game_info.defines.requires_ascii_table = 1;
            harness_game_info.defines.ascii_table = carmageddon_german_ascii_tables.ascii;
            harness_game_info.defines.ascii_shift_table = carmageddon_german_ascii_tables.ascii_shift;
            break;
        default:
            harness_game_info.defines.ascii_table = carmageddon_ascii_tables.ascii;
            harness_game_info.defines.ascii_shift_table = carmageddon_ascii_tables.ascii_shift;
            break;
        }
        break;
    case eGame_carmageddon_demo:
        harness_game_info.defines.ascii_table = demo_ascii_tables.ascii;
        harness_game_info.defines.ascii_shift_table = demo_ascii_tables.ascii_shift;
        break;
    case eGame_splatpack_demo:
    case eGame_splatpack_xmas_demo:
        harness_game_info.defines.ascii_table = xmas_ascii_tables.ascii;
        harness_game_info.defines.ascii_shift_table = xmas_ascii_tables.ascii_shift;
        break;
    default:
        break;
    }

    // 3dfx code paths require at least smoke.pix which is used instead of writing smoke directly to framebuffer
    if (access("DATA/PIXELMAP/SMOKE.PIX", F_OK) != -1) {
        harness_game_info.data_dir_has_3dfx_assets = 1;
    }
}

void Harness_DetectAndSetWorkingDirectory(char* argv0) {
    char* path;
    char* env_var;
    char pref_path[MAX_PATH];

    env_var = getenv("DETHRACE_ROOT_DIR");

    if (strlen(harness_game_config.selected_dir) > 0) {
        path = harness_game_config.selected_dir;
    } else if (env_var != NULL) {
        LOG_INFO2("DETHRACE_ROOT_DIR is set to '%s'", env_var);
        path = env_var;
    } else {
        path = OS_GetWorkingDirectory(argv0);
        if (access("DATA/GENERAL.TXT", F_OK) == 0) {
            // good, found
        } else {
            OS_GetPrefPath(pref_path, "dethrace");
            path = pref_path;
        }
    }

    // if root_dir is null or empty, no need to chdir
    if (path != NULL && path[0] != '\0') {
        printf("Using game directory: %s\n", path);
        if (chdir(path) != 0) {
            LOG_PANIC2("Failed to chdir. Error is %s", strerror(errno));
        }
    }
}

int Harness_Init(int* argc, char* argv[]) {

    printf("Dethrace version: %s\n", DETHRACE_VERSION);

    memset(&harness_game_info, 0, sizeof(harness_game_info));

    // disable the original CD check code
    harness_game_config.enable_cd_check = 0;
    // original physics updates every 40ms, causing noticable camera juddering when cornering at slow speed
    // this setting runs physics every frame, smoothing movement out.
    // disabled by default for now until we are happy it doesn't cause other issues
    harness_game_config.physics_per_frame = 0;
    // limit to 60 fps by default
    harness_game_config.fps = 60;
    // do not freeze timer
    harness_game_config.freeze_timer = 0;
    // default demo time out is 240s
    harness_game_config.demo_timeout = 240000;
    // disable developer diagnostics by default
    harness_game_config.enable_diagnostics = 0;
    // no volume multiplier
    harness_game_config.volume_multiplier = 1.0f;
    // start window in windowed mode
    harness_game_config.start_full_screen = 1;
    // Disable gore check emulation
    harness_game_config.gore_check = 0;
    // Disable "Sound Options" menu
    harness_game_config.sound_options = 0;
    // Skip binding socket to allow local network testing
    harness_game_config.no_bind = 0;
    // Disable verbose logging
    harness_game_config.verbose = 0;

    // install signal handler
    harness_game_config.install_signalhandler = 1;

    harness_game_config.gl_nearest_filtering = 0;
    harness_game_config.gl_sharp_filtering = 0;
    harness_game_config.gl_widescreen = 1;
    harness_game_config.gl_widescreen_hud = 1;
    harness_game_config.gl_distance_fog = 1;
    harness_game_config.gl_distance_fade = 0.8f;
    harness_game_config.gl_depth_of_field = 0;
    harness_game_config.gl_dof_start = 0.4f;
    harness_game_config.gl_dof_end = 1.0f;
    harness_game_config.gl_dof_strength = 3.0f;
    harness_game_config.gl_glow = 0;
    harness_game_config.gl_glow_strength = 0.7f;
    harness_game_config.gl_night = 2;
    harness_game_config.gl_night_brightness = 0.3f;
    harness_game_config.gl_headlight_strength = 2.0f;
    harness_game_config.gl_headlight_haze = 1.0f;
    harness_game_config.gl_ssao = 0;
    harness_game_config.gl_ssao_strength = 0.6f;
    harness_game_config.gl_ssao_radius = 0.5f;
    harness_game_config.gl_speed_blur = 0;
    harness_game_config.gl_lens_flare = 0.0f;
    harness_game_config.gl_heat_shimmer = 0;
    harness_game_config.gl_colour_grading = 0;
    harness_game_config.draw_distance = 1.0f;
    harness_game_config.max_detail = 0;
    harness_game_config.msaa_samples = 4;
    harness_game_config.supersampling = 0;
    harness_game_config.gamepad_rumble = 1;
    harness_game_config.vsync = 1;
    harness_game_config.pause_on_focus_loss = 1;
    harness_game_config.gamepad_stick_deadzone = 0.15f;
    harness_game_config.gamepad_trigger_deadzone = 0.05f;
    harness_game_config.gamepad_steering_curve = 2.5f;
    harness_game_config.gamepad_steering_speed = 60.f;
    harness_game_config.fov_scale = 1.f;
    harness_game_config.camera_smoothing = 0.f;
    harness_game_config.gl_pedestrian_shadows = 0;
    harness_game_config.gl_surface_variation = 0.f;
    harness_game_config.gl_sun_shading = 0.f;
    harness_game_config.texture_upscaler = 0;

    // first load ini file if exists
    Harness_ProcessIniFile();

    // then override any config options with command line args
    if (Harness_ProcessCommandLine(argc, argv) != 0) {
        fprintf(stderr, "Failed to parse harness command line\n");
        return 1;
    }

    // now resolve the platform
    if (Harness_InitPlatform() != 0) {
        return 1;
    }

    if (harness_game_config.install_signalhandler) {
        OS_InstallSignalHandler(argv[0]);
    }

    Harness_DetectAndSetWorkingDirectory(argv[0]);

    if (harness_game_info.mode == eGame_none) {
        Harness_DetectGameMode();
    }

    if (harness_game_config.opengl_3dfx_mode && !harness_game_info.data_dir_has_3dfx_assets) {
        printf("Error: data directory does not contain 3dfx assets so opengl mode cannot be used\n");
        exit(1);
    }

    return 0;
}

void Harness_Quit(void) {

    if (harness_game_config.install_signalhandler) {
        OS_RemoveSignalHandler();
    }
}

int Harness_ProcessCommandLine(int* argc, char* argv[]) {
    int i, j;
    char* val;
    for (i = 1; i < *argc;) {
        int consumed = -1;

        if (strcasecmp(argv[i], "--game") == 0) {
            if (i < *argc + 1) {
                val = argv[i + 1];
                for (i = 0; i < harness_game_config.game_dirs_count; i++) {
                    if (strcmp(harness_game_config.game_dirs[i].name, val) == 0) {
                        safe_strcpy(harness_game_config.selected_dir, harness_game_config.game_dirs[i].directory);
                        break;
                    }
                }
                if (i == harness_game_config.game_dirs_count) {
                    LOG_PANIC2("Failed to find game %s", val);
                }
                consumed = 2;
            }
        } else if (strcasecmp(argv[i], "--dir") == 0) {
            if (i < *argc + 1) {
                safe_strcpy(harness_game_config.selected_dir, argv[i + 1]);
                consumed = 2;
            }
        } else if (strcasecmp(argv[i], "--cdcheck") == 0) {
            harness_game_config.enable_cd_check = 1;
            consumed = 1;
        } else if (strstr(argv[i], "--debug=") != NULL) {
            char* s = strstr(argv[i], "=");
            harness_debug_level = atoi(s + 1);
            LOG_INFO2("debug level set to %d", harness_debug_level);
            consumed = 1;
        } else if (strstr(argv[i], "--physics-step-time=") != NULL) {
            // backwards compat, see `physics-per-frame`
            int val;
            char* s = strstr(argv[i], "=");
            val = atoi(s + 1);
            harness_game_config.physics_per_frame = val < PHYSICS_STEP_TIME;
            if (harness_game_config.physics_per_frame) {
                LOG_INFO("Physics per frame enabled");
            }
            consumed = 1;
#ifdef DETHRACE_FIX_BUGS
        } else if (strcasecmp(argv[i], "--physics-per-frame") == 0) {
            harness_game_config.physics_per_frame = 1;
            LOG_INFO("Physics per frame enabled");
            consumed = 1;
#endif
        } else if (strstr(argv[i], "--fps=") != NULL) {
            char* s = strstr(argv[i], "=");
            harness_game_config.fps = atoi(s + 1);
            LOG_INFO2("FPS limiter set to %f", harness_game_config.fps);
            consumed = 1;
        } else if (strcasecmp(argv[i], "--freeze-timer") == 0) {
            LOG_INFO("Timer frozen");
            harness_game_config.freeze_timer = 1;
            consumed = 1;
        } else if (strcasecmp(argv[i], "--no-signal-handler") == 0) {
            LOG_INFO("Don't install the signal handler");
            harness_game_config.install_signalhandler = 0;
            consumed = 1;
        } else if (strstr(argv[i], "--demo-timeout=") != NULL) {
            char* s = strstr(argv[i], "=");
            harness_game_config.demo_timeout = atoi(s + 1) * 1000;
            LOG_INFO2("Demo timeout set to %d milliseconds", harness_game_config.demo_timeout);
            consumed = 1;
        } else if (strcasecmp(argv[i], "--i-am-cheating") == 0) {
            gI_am_cheating = 0xa11ee75d;
            consumed = 1;
        } else if (strcasecmp(argv[i], "--enable-diagnostics") == 0) {
            harness_game_config.enable_diagnostics = 1;
            consumed = 1;
        } else if (strstr(argv[i], "--volume-multiplier=") != NULL) {
            char* s = strstr(argv[i], "=");
            harness_game_config.volume_multiplier = atof(s + 1);
            LOG_INFO2("Volume multiplier set to %f", harness_game_config.volume_multiplier);
            consumed = 1;
        } else if (strcasecmp(argv[i], "--full-screen") == 0) {
            // option left for backwards compatibility
            harness_game_config.start_full_screen = 1;
            consumed = 1;
        } else if (strcasecmp(argv[i], "--window") == 0) {
            harness_game_config.start_full_screen = 0;
            consumed = 1;
        } else if (strcasecmp(argv[i], "--gore-check") == 0) {
            harness_game_config.gore_check = 1;
            consumed = 1;
        } else if (strcasecmp(argv[i], "--sound-options") == 0) {
            harness_game_config.sound_options = 1;
            consumed = 1;
        } else if (strcasecmp(argv[i], "--opengl") == 0) {
            harness_game_config.opengl_3dfx_mode = 1;
            consumed = 1;
        } else if (strcasecmp(argv[i], "--game-completed") == 0) {
            harness_game_config.game_completed = 1;
            consumed = 1;
        } else if (strcasecmp(argv[i], "--no-bind") == 0) {
            harness_game_config.no_bind = 1;
            consumed = 1;
        } else if (strstr(argv[i], "--network-adapter-name") != NULL) {
            if (i < *argc + 1) {
                safe_strcpy(harness_game_config.network_adapter_name, argv[i + 1]);
                consumed = 2;
            }
        } else if (strcasecmp(argv[i], "--platform") == 0) {
            if (i < *argc + 1) {
                safe_strcpy(harness_game_config.platform_name, argv[i + 1]);
                consumed = 2;
            }
        }

        if (consumed > 0) {
            // shift args downwards
            for (j = i; j < *argc - consumed; j++) {
                argv[j] = argv[j + consumed];
            }
            *argc -= consumed;
        } else {
            i += 1;
        }
    }

    return 0;
}

static float ClampConfigFloat(float pValue, float pMin, float pMax) {
    return pValue < pMin ? pMin : (pValue > pMax ? pMax : pValue);
}

static int Harness_Ini_Callback(void* user, const char* section, const char* name, const char* value) {
    int i;
    float f;

#define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0
#define MATCH_NAME(s, n) strcmp(name, s) == 0

    if (strcmp(section, "Games") == 0) {
        safe_strcpy(harness_game_config.game_dirs[harness_game_config.game_dirs_count].name, name);
        safe_strcpy(harness_game_config.game_dirs[harness_game_config.game_dirs_count].directory, value);
        harness_game_config.game_dirs_count++;
    }

    else if (MATCH("General", "CdCheck")) {
        harness_game_config.enable_cd_check = (value[0] == '1');
    } else if (MATCH("General", "GoreCheck")) {
        harness_game_config.gore_check = (value[0] == '1');
    } else if (MATCH("General", "FPSLimit")) {
        i = atoi(value);
        harness_game_config.fps = i;
    } else if (MATCH("General", "DemoTimeout")) {
        i = atoi(value);
        harness_game_config.demo_timeout = i * 1000;
    } else if (MATCH("General", "Windowed")) {
        harness_game_config.start_full_screen = (value[0] == '0');
    } else if (MATCH("General", "Emulate3DFX")) {
        harness_game_config.opengl_3dfx_mode = (value[0] == '1');
    } else if (MATCH("General", "DefaultGame")) {
        safe_strcpy(harness_game_config.default_game, value);
    } else if (MATCH("General", "BoringMode")) {
        gSausage_override = (value[0] == '1');
    } else if (MATCH("General", "Hires")) {
        gGraf_spec_index = (value[0] == '1');
    } else if (MATCH("General", "TextureFiltering")) {
        // "sharp" is nearest-style texels with anti-aliased edges, so the game treats it like "nearest"
        harness_game_config.gl_sharp_filtering = (strcasecmp(value, "sharp") == 0);
        harness_game_config.gl_nearest_filtering = (strcasecmp(value, "nearest") == 0) || harness_game_config.gl_sharp_filtering;
    } else if (MATCH("General", "DrawDistance")) {
        harness_game_config.draw_distance = atof(value);
        if (harness_game_config.draw_distance < 1.0f) {
            harness_game_config.draw_distance = 1.0f;
        }
    } else if (MATCH("General", "MaxDetail")) {
        harness_game_config.max_detail = (value[0] == '1');
    } else if (MATCH("General", "TextureUpscaler")) {
        if (strcasecmp(value, "mmpx2x") == 0) {
            harness_game_config.texture_upscaler = 1;
        } else if (strcasecmp(value, "xbr2x") == 0) {
            harness_game_config.texture_upscaler = 2;
        } else {
            harness_game_config.texture_upscaler = 0;
        }
    } else if (MATCH("General", "GamepadStickDeadzone")) {
        harness_game_config.gamepad_stick_deadzone = ClampConfigFloat(atof(value), 0.f, 0.9f);
    } else if (MATCH("General", "GamepadTriggerDeadzone")) {
        harness_game_config.gamepad_trigger_deadzone = ClampConfigFloat(atof(value), 0.f, 0.9f);
    } else if (MATCH("General", "GamepadSteeringCurve")) {
        harness_game_config.gamepad_steering_curve = ClampConfigFloat(atof(value), 1.f, 5.f);
    } else if (MATCH("General", "GamepadSteeringSpeed")) {
        harness_game_config.gamepad_steering_speed = atof(value);
    } else if (MATCH("General", "PauseOnFocusLoss")) {
        harness_game_config.pause_on_focus_loss = (value[0] == '1');
    } else if (MATCH("General", "VSync")) {
        if (strcasecmp(value, "adaptive") == 0) {
            harness_game_config.vsync = -1;
        } else {
            harness_game_config.vsync = (value[0] != '0');
        }
    } else if (MATCH("General", "GamepadRumble")) {
        harness_game_config.gamepad_rumble = (value[0] == '1');
    } else if (MATCH("General", "FOVScale")) {
        harness_game_config.fov_scale = atof(value);
        if (harness_game_config.fov_scale < 0.5f || harness_game_config.fov_scale > 2.5f) {
            harness_game_config.fov_scale = 1.f;
        }
    } else if (MATCH("General", "SurfaceVariation")) {
        harness_game_config.gl_surface_variation = ClampConfigFloat(atof(value), 0.0f, 4.0f);
    } else if (MATCH("General", "SunShading")) {
        harness_game_config.gl_sun_shading = ClampConfigFloat(atof(value), 0.0f, 2.0f);
    } else if (MATCH("General", "PedestrianShadows")) {
        harness_game_config.gl_pedestrian_shadows = (value[0] == '1');
    } else if (MATCH("General", "CameraSmoothing")) {
        harness_game_config.camera_smoothing = ClampConfigFloat(atof(value), 0.0f, 1.0f);
    } else if (MATCH("General", "Supersampling")) {
        harness_game_config.supersampling = (value[0] == '1');
    } else if (MATCH("General", "MSAA")) {
        harness_game_config.msaa_samples = atoi(value);
        if (harness_game_config.msaa_samples < 2) {
            harness_game_config.msaa_samples = 0;
        }
    } else if (MATCH("General", "Widescreen")) {
        harness_game_config.gl_widescreen = (value[0] == '1');
    } else if (MATCH("General", "WidescreenHud")) {
        harness_game_config.gl_widescreen_hud = (value[0] == '1');
    } else if (MATCH("General", "DistanceFog")) {
        harness_game_config.gl_distance_fog = (value[0] == '1');
    } else if (MATCH("General", "DepthOfField")) {
        harness_game_config.gl_depth_of_field = (value[0] == '1');
    } else if (MATCH("General", "DepthOfFieldStart")) {
        harness_game_config.gl_dof_start = ClampConfigFloat(atof(value), 0.0f, 1.0f);
    } else if (MATCH("General", "DepthOfFieldEnd")) {
        harness_game_config.gl_dof_end = ClampConfigFloat(atof(value), 0.01f, 1.0f);
    } else if (MATCH("General", "DepthOfFieldStrength")) {
        harness_game_config.gl_dof_strength = ClampConfigFloat(atof(value), 0.0f, 20.0f);
    } else if (MATCH("General", "Night")) {
        // 0 = off, 1 = on, auto = on tracks with the darkness depth cue
        harness_game_config.gl_night = strcmp(value, "auto") == 0 ? 2 : (value[0] == '1');
    } else if (MATCH("General", "NightBrightness")) {
        harness_game_config.gl_night_brightness = ClampConfigFloat(atof(value), 0.0f, 1.0f);
    } else if (MATCH("General", "SSAO")) {
        harness_game_config.gl_ssao = (value[0] == '1');
    } else if (MATCH("General", "SSAOStrength")) {
        harness_game_config.gl_ssao_strength = ClampConfigFloat(atof(value), 0.0f, 4.0f);
    } else if (MATCH("General", "SSAORadius")) {
        harness_game_config.gl_ssao_radius = ClampConfigFloat(atof(value), 0.05f, 2.0f);
    } else if (MATCH("General", "SpeedBlur")) {
        harness_game_config.gl_speed_blur = (value[0] == '1');
    } else if (MATCH("General", "LensFlare")) {
        harness_game_config.gl_lens_flare = ClampConfigFloat(atof(value), 0.0f, 4.0f);
    } else if (MATCH("General", "HeatShimmer")) {
        harness_game_config.gl_heat_shimmer = (value[0] == '1');
    } else if (MATCH("General", "ColourGrading")) {
        harness_game_config.gl_colour_grading = (value[0] == '1');
    } else if (MATCH("General", "HeadlightHaze")) {
        harness_game_config.gl_headlight_haze = ClampConfigFloat(atof(value), 0.0f, 4.0f);
    } else if (MATCH("General", "HeadlightStrength")) {
        harness_game_config.gl_headlight_strength = ClampConfigFloat(atof(value), 0.0f, 4.0f);
    } else if (MATCH("General", "Glow")) {
        harness_game_config.gl_glow = (value[0] == '1');
    } else if (MATCH("General", "GlowStrength")) {
        harness_game_config.gl_glow_strength = ClampConfigFloat(atof(value), 0.0f, 4.0f);
    } else if (MATCH("General", "DistanceFade")) {
        harness_game_config.gl_distance_fade = ClampConfigFloat(atof(value), 0.0f, 0.99f);
    } else if (MATCH("General", "PhysicsPerFrame")) {
        harness_game_config.physics_per_frame = (value[0] == '1');
    }

    else if (MATCH("Cheats", "EditMode")) {
        if (value[0] == '1') {
            gI_am_cheating = 0xa11ee75d;
        }
    } else if (MATCH("Cheats", "FreezeTimer")) {
        harness_game_config.freeze_timer = (value[0] == '1');
    } else if (MATCH("Cheats", "GameCompleted")) {
        harness_game_config.game_completed = (value[0] == '1');
    }

    else if (MATCH("Sound", "Enabled")) {
        // gSound_override=1 means sound is disabled
        gSound_override = (value[0] == '0');
    } else if (MATCH("Sound", "SoundOptionsScreen")) {
        harness_game_config.sound_options = (value[0] == '1');
    } else if (MATCH("Sound", "VolumeMultiplier")) {
        f = atof(value);
        harness_game_config.volume_multiplier = f;
    }

    else if (MATCH("Network", "AdapterName")) {
        safe_strcpy(harness_game_config.network_adapter_name, value);
    }

    else if (MATCH("Developers", "Diagnostics")) {
        harness_game_config.enable_diagnostics = (value[0] == '1');
    } else if (MATCH("Developers", "PhysicsStepTime")) {
        // backwards compat, see `PhysicsPerFrame`
        i = atoi(value);
        harness_game_config.physics_per_frame = i < PHYSICS_STEP_TIME;
    } else if (MATCH("Developers", "InstallSignalHandler")) {
        harness_game_config.install_signalhandler = (value[0] == '1');
    }

    else {
        // unknown section/name
    }

    return 1;
}

int Harness_ProcessIniFile(void) {
    int i;
    char path[1024];

    if (OS_GetPrefPath(path, "dethrace")) {
        strcpy(path, "./");
    }
    strcat(path, "dethrace.ini");
    LOG_INFO2("Loading ini file %s", path);

    if (ini_parse(path, Harness_Ini_Callback, NULL) < 0) {
        LOG_DEBUG2("Failed to load config file %s", path);
        return 1;
    }

    // set default game dir
    if (harness_game_config.game_dirs_count > 0) {
        if (strlen(harness_game_config.default_game) > 0) {
            for (i = 0; i < harness_game_config.game_dirs_count; i++) {
                if (strcmp(harness_game_config.game_dirs[i].name, harness_game_config.default_game) == 0) {
                    safe_strcpy(harness_game_config.selected_dir, harness_game_config.game_dirs[i].directory);
                    break;
                }
            }
            // if no default found, and we have some game dirs, default to first one
            if (i == harness_game_config.game_dirs_count) {
                safe_strcpy(harness_game_config.selected_dir, harness_game_config.game_dirs[i].directory);
            }
        }
    }

    return 0;
}

// Filesystem hooks
FILE* Harness_Hook_fopen(const char* pathname, const char* mode) {
    return OS_fopen(pathname, mode);
}

// Localization
int Harness_Hook_isalnum(int c) {
    int i;
    if (harness_game_info.localization == eGameLocalization_polish) {
        // Polish diacritic letters in Windows-1250
        unsigned char letters[] = { 140, 143, 156, 159, 163, 165, 175, 179, 185, 191, 198, 202, 209, 211, 230, 234, 241, 243 };
        for (i = 0; i < (int)sizeof(letters); i++) {
            if ((unsigned char)c == letters[i]) {
                return 1;
            }
        }
    }
    if (harness_game_info.localization == eGameLocalization_french) {
        // French diacritic letters in Windows-1252
        unsigned char letters[] = { 140, 156, 159, 192, 194, 198, 199, 200, 201, 202, 203, 206, 207, 212, 217, 219, 220, 224, 226, 230, 231, 232, 233, 234, 235, 238, 239, 244, 249, 251, 252, 255 };
        for (i = 0; i < (int)sizeof(letters); i++) {
            if ((unsigned char)c == letters[i]) {
                return 1;
            }
        }
    }

    return isalnum(c);
}

// Widescreen support for OpenGL (3dfx emulation) mode.
//
// The 2d interface stays 640x480, scaled and centered (4:3) in the window. The main 3d view is rendered into
// a full-width sub-pixelmap of the back screen; while the game has marked that view as extendable, the OpenGL
// viewport for it is widened to the full window width and the camera aspect ratio widened to match (Hor+).
static br_pixelmap* widescreen_back_screen;
static br_pixelmap* widescreen_render_screen;

// Widescreen HUD, valid until the end of the frame
static br_pixelmap* hud_back_screen;
static float hud_factor;  // window width / interface width
static float hud_margin;  // width of each side area, interface pixels
static int hud_2d_wide;
static float hud_regions[5 * BR_GL_MAX_OVERLAY_REGIONS];
static int hud_region_count;

// Window width divided by the width of the 4:3 interface area
static float widescreen_factor(br_pixelmap* back_screen) {
    int x, y;
    float sx, sy;

    if (gHarness_platform.GetViewport == NULL) {
        return 1.0f;
    }
    gHarness_platform.GetViewport(&x, &y, &sx, &sy);
    if (x <= 0 || sx <= 0.0f) {
        return 1.0f;
    }
    return (2 * x + back_screen->width * sx) / (back_screen->width * sx);
}

float Harness_Hook_WidescreenBeginView(br_pixelmap* back_screen, br_pixelmap* render_screen, int allow) {
    float factor;

    widescreen_back_screen = NULL;
    widescreen_render_screen = NULL;
    if (!allow || !harness_game_config.opengl_3dfx_mode || !harness_game_config.gl_widescreen) {
        return 1.0f;
    }
    // Only a view spanning the full interface width can be extended
    if (render_screen->base_x != 0 || render_screen->width != back_screen->width) {
        return 1.0f;
    }
    factor = widescreen_factor(back_screen);
    if (factor <= 1.0f) {
        return 1.0f;
    }
    widescreen_back_screen = back_screen;
    widescreen_render_screen = render_screen;
    if (harness_game_config.gl_widescreen_hud) {
        hud_back_screen = back_screen;
        hud_factor = factor;
        hud_margin = (factor - 1.0f) * back_screen->width / 2.0f;
    }
    return factor;
}

float Harness_Hook_HudShift(int left, int right) {
    int centre;

    if (hud_back_screen == NULL) {
        return 0.0f;
    }
    // elements in the left or right third go to that edge; the ones in the middle (timer, messages) stay
    centre = (left + right) / 2;
    if (3 * centre < hud_back_screen->width) {
        return -hud_margin;
    }
    if (3 * centre > 2 * hud_back_screen->width) {
        return hud_margin;
    }
    return 0.0f;
}

void Harness_Hook_HudRegion(int left, int top, int right, int bottom, float shift) {
    float* r;

    if (hud_back_screen == NULL || shift == 0.0f || right <= left || bottom <= top || hud_region_count >= BR_GL_MAX_OVERLAY_REGIONS) {
        return;
    }
    r = &hud_regions[5 * hud_region_count++];
    r[0] = (float)left;
    r[1] = (float)top;
    r[2] = (float)right;
    r[3] = (float)bottom;
    r[4] = shift;
}

float Harness_Hook_Hud2dBegin(float* x_offset) {
    *x_offset = 0.0f;
    if (hud_back_screen == NULL) {
        return 1.0f;
    }
    hud_2d_wide = 1;
    *x_offset = -hud_margin;
    return hud_factor;
}

void Harness_Hook_Hud2dEnd(void) {
    hud_2d_wide = 0;
}

// Post-processing (OpenGL mode), implemented by glrend
// must match glrend's (include/brglrend_p.h)
typedef struct br_gl_post_params {
    float hither, yon;
    float dof_start, dof_end;
    float dof_max_radius;
    float glow_strength;
    float flare_strength;
    float ao_strength;
    float ao_radius;
    float speed;
    float grade;
    float night;
    float time;
    int shimmer_count;
    float shimmers[4 * 8];
} br_gl_post_params;
br_error BR_PUBLIC_ENTRY BrGLPostProcessBegin(br_pixelmap* screen, br_boolean glow);
void BR_PUBLIC_ENTRY BrGLPostProcessDepthSnapshot(br_pixelmap* screen);
static br_pixelmap* gPost_screen;
void BR_PUBLIC_ENTRY BrGLPostProcessEnd(br_pixelmap* screen, const br_gl_post_params* params);

int Harness_Hook_PostProcessBegin(br_pixelmap* screen) {
    if (!harness_game_config.opengl_3dfx_mode || screen == NULL
        || !(harness_game_config.gl_depth_of_field || harness_game_config.gl_glow || harness_game_config.gl_ssao
            || harness_game_config.gl_speed_blur || harness_game_config.gl_heat_shimmer || harness_game_config.gl_colour_grading)) {
        return 0;
    }
    if (BrGLPostProcessBegin(screen, harness_game_config.gl_glow) != BRE_OK) {
        return 0;
    }
    gPost_screen = screen;
    return 1;
}

void Harness_Hook_PostProcessDepthSnapshot(void) {
    if (gPost_screen != NULL && harness_game_config.gl_ssao) {
        BrGLPostProcessDepthSnapshot(gPost_screen);
    }
}

void Harness_Hook_PostProcessEnd(br_pixelmap* screen, float hither, float yon, float speed, int night, const float* fires, int fire_count) {
    br_gl_post_params params;
    int vx, vy;
    float sx, sy;

    memset(&params, 0, sizeof(params));
    params.hither = hither;
    params.yon = yon;
    params.dof_start = harness_game_config.gl_dof_start * yon;
    params.dof_end = (harness_game_config.gl_dof_end > harness_game_config.gl_dof_start + 0.01f ? harness_game_config.gl_dof_end : harness_game_config.gl_dof_start + 0.01f) * yon;
    // strength is the blur radius in pixels at 1080p
    params.dof_max_radius = harness_game_config.gl_depth_of_field ? harness_game_config.gl_dof_strength : 0.0f;
    if (gHarness_platform.GetViewport != NULL) {
        gHarness_platform.GetViewport(&vx, &vy, &sx, &sy);
        params.dof_max_radius *= (2 * vy + screen->height * sy) / 1080.0f;
    }
    params.glow_strength = harness_game_config.gl_glow ? harness_game_config.gl_glow_strength : 0.0f;
    params.flare_strength = harness_game_config.gl_glow ? harness_game_config.gl_lens_flare : 0.0f;
    params.ao_strength = harness_game_config.gl_ssao ? harness_game_config.gl_ssao_strength : 0.0f;
    params.ao_radius = harness_game_config.gl_ssao_radius;
    params.speed = harness_game_config.gl_speed_blur ? speed : 0.0f;
    params.grade = harness_game_config.gl_colour_grading ? 1.0f : 0.0f;
    params.night = night ? 1.0f : 0.0f;
    params.time = gHarness_platform.GetTicks != NULL ? (gHarness_platform.GetTicks() % 1000000) / 1000.0f : 0.0f;
    if (harness_game_config.gl_heat_shimmer && fires != NULL) {
        params.shimmer_count = fire_count < 8 ? fire_count : 8;
        memcpy(params.shimmers, fires, sizeof(float) * 4 * params.shimmer_count);
    }
    BrGLPostProcessEnd(screen, &params);
}

static int glow_compare(const void* key, const void* entry) {
    const char* a = key;
    const char* b = ((const tGlow_entry*)entry)->name;

    for (; *a != '\0' && toupper((unsigned char)*a) == *b; a++, b++) {
    }
    return toupper((unsigned char)*a) - *b;
}

// glow scale of each glow table entry (eg. brake lights), see Harness_Hook_CarLightGlow
static float glow_scales[1024];

float Harness_GL_GlowScale(int id) {
    return id >= 0 && id < (int)(sizeof(glow_scales) / sizeof(glow_scales[0])) ? glow_scales[id] : 1.0f;
}

void Harness_Hook_CarLightGlow(const char* identifier, int braking) {
    const tGlow_entry* entry;

    if (identifier == NULL) {
        return;
    }
    entry = bsearch(identifier, gGlow_table, gGlow_table_count, sizeof(tGlow_entry), glow_compare);
    if (entry != NULL && entry->light_type == eGlow_light_tail && entry - gGlow_table < (int)(sizeof(glow_scales) / sizeof(glow_scales[0]))) {
        // tail lights: dim while driving, bright brake lights
        glow_scales[entry - gGlow_table] = braking ? 1.6f : 0.45f;
    }
}

float Harness_GL_TextureGlow(const char* identifier, const unsigned char** indices, const short** rects, int* rect_count, int* id) {
    const tGlow_entry* entry;
    static int scales_set;

    if (!scales_set) {
        int i;
        for (i = 0; i < (int)(sizeof(glow_scales) / sizeof(glow_scales[0])); i++) {
            glow_scales[i] = 1.0f;
        }
        scales_set = 1;
    }

    if (!harness_game_config.gl_glow || identifier == NULL) {
        return 0.0f;
    }
    entry = bsearch(identifier, gGlow_table, gGlow_table_count, sizeof(tGlow_entry), glow_compare);
    if (entry == NULL) {
        return 0.0f;
    }
    *indices = entry->indices;
    *rects = entry->rects;
    *rect_count = entry->rect_count;
    *id = (int)(entry - gGlow_table);
    return entry->intensity;
}

void Harness_Hook_MakeDirectory(const char* path) {
#ifdef _WIN32
    _mkdir(path);
#else
    mkdir(path, 0755);
#endif
}

static int night_now;

int Harness_Hook_Night(int pDarkness) {
    night_now = harness_game_config.opengl_3dfx_mode
        && (harness_game_config.gl_night == 1 || (harness_game_config.gl_night == 2 && pDarkness));
    return night_now;
}

static float scene_brightness = 1.0f;
static float scene_lights[BR_GL_SCENE_LIGHT_FLOATS * BR_GL_MAX_SCENE_LIGHTS];
static int scene_light_count;

static float scene_shade = 1.f;

void Harness_Hook_SceneShade(float shade) {
    scene_shade = shade;
}

static float scene_surface[8];

void Harness_Hook_SceneSurface(const float* sun, const float* up, int night) {
    scene_surface[0] = sun[0];
    scene_surface[1] = sun[1];
    scene_surface[2] = sun[2];
    scene_surface[3] = night ? 0.f : harness_game_config.gl_sun_shading;
    scene_surface[4] = up[0];
    scene_surface[5] = up[1];
    scene_surface[6] = up[2];
    scene_surface[7] = harness_game_config.gl_surface_variation;
}

int Harness_GL_SceneSurface(float* params) {
    memcpy(params, scene_surface, sizeof(scene_surface));
    return scene_surface[3] > 0.f || scene_surface[7] > 0.f;
}

void Harness_Hook_SceneLighting(float brightness, const float* lights, int count) {
    scene_brightness = brightness;
    scene_light_count = 0;
    if (lights != NULL && count > 0) {
        scene_light_count = count < BR_GL_MAX_SCENE_LIGHTS ? count : BR_GL_MAX_SCENE_LIGHTS;
        memcpy(scene_lights, lights, sizeof(float) * BR_GL_SCENE_LIGHT_FLOATS * scene_light_count);
    }
}

int Harness_GL_SceneLighting(float* brightness, const float** lights) {
    *brightness = scene_brightness * scene_shade;
    *lights = scene_lights;
    return scene_light_count;
}

static int distance_fade_on;

void Harness_Hook_DistanceFade(int on) {
    distance_fade_on = on;
}

float Harness_GL_DistanceFade(void) {
    return distance_fade_on ? harness_game_config.gl_distance_fade : 0.0f;
}

int Harness_GL_OverlayRegions(const float** regions) {
    *regions = hud_regions;
    return hud_back_screen != NULL ? hud_region_count : 0;
}

void Harness_GL_SwapBuffers(br_pixelmap* pm) {
    gHarness_platform.Swap(pm);
    // the HUD positions are set up again by the next frame
    hud_back_screen = NULL;
    hud_region_count = 0;
    hud_2d_wide = 0;
}

static int harness_focus_lost;

void Harness_Hook_FocusLost(void) {
    harness_focus_lost = 1;
}

int Harness_ConsumeFocusLost(void) {
    int lost = harness_focus_lost && harness_game_config.pause_on_focus_loss;
    harness_focus_lost = 0;
    return lost;
}

void Harness_Hook_WidescreenEndView(void) {
    widescreen_back_screen = NULL;
    widescreen_render_screen = NULL;
}

void Harness_Hook_WidescreenFillBackground(br_pixelmap* pixelmap, br_uint_16 colour_565) {
    typedef void (*glClearColor_t)(float, float, float, float);
    typedef void (*glClear_t)(unsigned int);
    static glClearColor_t clear_colour;
    static glClear_t clear;
    int r, g, b;

    // The game fills the background (where no 3d geometry is drawn) through the 4:3 2d layer, so the
    // extended areas at the sides need the same colour
    if (pixelmap == NULL || pixelmap != widescreen_back_screen || gHarness_platform.GL_GetProcAddress == NULL) {
        return;
    }
    if (clear == NULL) {
        clear_colour = (glClearColor_t)gHarness_platform.GL_GetProcAddress("glClearColor");
        clear = (glClear_t)gHarness_platform.GL_GetProcAddress("glClear");
        if (clear == NULL || clear_colour == NULL) {
            return;
        }
    }
    r = (colour_565 >> 11) & 0x1f;
    g = (colour_565 >> 5) & 0x3f;
    b = colour_565 & 0x1f;
    clear_colour(r / 31.0f, g / 63.0f, b / 31.0f, 1.0f);
    clear(0x00004000 /* GL_COLOR_BUFFER_BIT */);
    clear_colour(0.0f, 0.0f, 0.0f, 1.0f);
}

void Harness_GL_AdjustRenderViewport(br_pixelmap* target, int* x, int* y, int* width, int* height) {
    int vx, vy;
    float sx, sy;

    if (target == NULL || gHarness_platform.GetViewport == NULL) {
        return;
    }
    if (target != widescreen_render_screen && !(hud_2d_wide && target == hud_back_screen)) {
        return;
    }
    gHarness_platform.GetViewport(&vx, &vy, &sx, &sy);
    *x = 0;
    *width = 2 * vx + (int)(target->width * sx + 0.5f);
}

// For physics-per-frame. We are calling some functions more often (like SkidNoise), so need to reduce the probability
// that each call does something to end up with visually similar behavior
int Harness_Hook_ScaleProbabilityWithDt(int min_inclusive, int max_inclusive, br_scalar dt_seconds) {
    br_scalar scaled_probability;
    br_scalar step_seconds;
    br_scalar step_ratio;
    int range;

    range = max_inclusive - min_inclusive + 1;
    if (range <= 0) {
        return 0;
    }
    step_seconds = PHYSICS_STEP_TIME / 1000.0f;
    step_ratio = 1.0f;
    if (dt_seconds > 0.0f && dt_seconds < step_seconds) {
        step_ratio = dt_seconds / step_seconds;
    }
    scaled_probability = step_ratio / range;
    return (rand() / (float)RAND_MAX) < scaled_probability;
}

// For physics-per-frame. We are calling some functions more often (like CreateSparks), so need to reduce
// the number of things that each call emits to end up with visually similar behavior
int Harness_Hook_ScaleEmissionCountWithDt(float count, float dt_seconds) {
    float step_ratio;
    float scaled_count;
    float random_unit;
    int emission_count;

    step_ratio = 1.0f;
    if (dt_seconds > 0.0f) {
        step_ratio = dt_seconds / (PHYSICS_STEP_TIME / 1000.0f);
    }
    if (step_ratio < 0.0f) {
        step_ratio = 0.0f;
    } else if (step_ratio > 1.0f) {
        step_ratio = 1.0f;
    }

    scaled_count = count * step_ratio;
    emission_count = (int)scaled_count;
    random_unit = rand() / (float)RAND_MAX;
    if (random_unit < scaled_count - emission_count) {
        emission_count++;
    }
    return emission_count;
}
