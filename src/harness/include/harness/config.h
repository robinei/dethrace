#ifndef HARNESS_CONFIG_H
#define HARNESS_CONFIG_H

#define MAX_PATH 1024

typedef enum tHarness_game_type {
    eGame_none,
    eGame_carmageddon,
    eGame_splatpack,
    eGame_carmageddon_demo,
    eGame_splatpack_demo,
    eGame_splatpack_xmas_demo,
} tHarness_game_type;

typedef enum {
    eGameLocalization_none,
    eGameLocalization_german,
    eGameLocalization_polish,
    eGameLocalization_french,
} tHarness_game_localization;

typedef struct tHarness_game_info {
    tHarness_game_type mode;
    tHarness_game_localization localization;
    struct {
        // different between carmageddon and splatpack
        char* INTRO_SMK_FILE;
        // different between demo and full game
        char* GERMAN_LOADSCRN;
        // some versions have an ascii table built-in, others provide it through KEYBOARD.COK
        int requires_ascii_table;
        // built-in keyboard look-up table for certain localized Carmageddon releases
        int* ascii_table;
        // built-in shifted keyboard look-up table for certain localized Carmageddon releases
        int* ascii_shift_table;
    } defines;
    int data_dir_has_3dfx_assets;
} tHarness_game_info;

typedef struct tHarness_game_dir {
    char name[256];
    char directory[MAX_PATH];
} tHarness_game_dir;

typedef struct tHarness_game_config {
    int enable_cd_check;
    int physics_per_frame;
    float fps;
    int freeze_timer;
    unsigned demo_timeout;
    int enable_diagnostics;
    float volume_multiplier;
    int start_full_screen;
    int gore_check;
    int sound_options;

    int verbose;
    int opengl_3dfx_mode;
    int game_completed;

    // OpenGL (3dfx emulation) mode: use nearest neighbour texture filtering instead of bilinear
    int gl_nearest_filtering;
    int gl_sharp_filtering;
    // OpenGL (3dfx emulation) mode: extend the 3d view to fill widescreen displays
    int gl_widescreen;
    int gl_widescreen_hud;
    int gl_distance_fog;
    float gl_distance_fade;
    int gl_depth_of_field;
    float gl_dof_start;
    float gl_dof_end;
    float gl_dof_strength;
    int gl_glow;
    float gl_glow_strength;
    int gl_night;
    float gl_night_brightness;
    float gl_headlight_strength;
    float gl_headlight_haze;
    int gl_ssao;
    float gl_ssao_strength;
    float gl_ssao_radius;
    int gl_speed_blur;
    float gl_lens_flare;
    int gl_heat_shimmer;
    int gl_colour_grading;

    // Multiplier for the draw distance chosen in the game's options
    float draw_distance;
    int draw_everything;
    // Always draw the track up to the full draw distance and use the most detailed car models
    int max_detail;
    int msaa_samples;
    int supersampling;
    int texture_upscaler; /* 0 = none, 1 = MMPX 2x, 2 = xBR 2x */
    float fov_scale;
    int gl_pedestrian_shadows;
    float gl_surface_variation;
    float gl_sun_shading;
    float gl_haze;
    float gl_flashes;   /* strength of the light flashes from sparks and fires breaking out, 0 = off */
    float gl_wet_roads; /* strength of the lights' reflections on the ground at night, 0 = off */
    float gl_ice_reflections; /* strength of the mirror image on ice, 0 = off */
    float camera_smoothing; /* seconds: how softly the external camera follows the car's turns, 0 = rigidly */
    int gamepad_rumble;
    int vsync; /* 1 = on, 0 = off, -1 = adaptive */
    int pause_on_focus_loss;
    float gamepad_stick_deadzone;
    float gamepad_trigger_deadzone;
    float gamepad_steering_curve;
    float gamepad_steering_speed;

    int install_signalhandler;
    int no_bind;
    char network_adapter_name[256];
    char platform_name[256];

    char selected_dir[MAX_PATH];
    int game_dirs_count;
    tHarness_game_dir game_dirs[10];
    char default_game[256];
} tHarness_game_config;

extern tHarness_game_info harness_game_info;
extern tHarness_game_config harness_game_config;

#endif
