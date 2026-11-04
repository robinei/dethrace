# Configuration

Dethrace looks for a `dethrace.ini` file in the [system preferences directory](https://wiki.libsdl.org/SDL2/SDL_GetPrefPath):
| Platform | Example Path                                                                 |
|----------|----------------------------------------------------------------------|
| Windows  | `C:\Users\<NAME>\AppData\Roaming\dethrace\dethrace.ini`                |
| macOS    | `/Users/<NAME>/Library/Application Support/dethrace/dethrace.ini`      |
| Linux    | `/home/<NAME>/.local/share/dethrace/dethrace.ini`                      |


If this file is not present, dethrace will run with default configuration and attempt to discover the correct Carmageddon directory to use (see below).

## Example dethrace.ini file
```ini
[General]
; Enable original CD check
CdCheck = 0

; Enable original censorship check
GoreCheck = 0

; set to 0 to disable
FPSLimit = 60

; Full screen or window
Windowed = 1

; 3dfx mode (via OpenGL)
Emulate3DFX = 0

; Only used in 3dfx mode. "nearest" for crisp, unfiltered textures (like the software renderer),
; "sharp" for nearest-style texels with anti-aliased edges (no shimmer), otherwise "linear"
TextureFiltering = linear

; Only used in 3dfx mode. Extend the 3d view to fill widescreen windows. The interface stays 4:3, centered
Widescreen = 1

; Only used with Widescreen. In the external view, move the HUD elements at the sides (pratcam, lap and
; kill counters, speedo, damage display, power-ups) out to the window edges. Centred ones stay centred
WidescreenHud = 1

; Only used in 3dfx mode. Fog and darkness (set per track) by distance: starting where the original's does
; and complete at the (extended) draw distance, so nothing pops in at the far edge. 0 = original approximation
DistanceFog = 1

; Only used in 3dfx mode. Depth of field: the distance blurs, nearer things stay sharp. The blur starts at
; DepthOfFieldStart and is strongest at DepthOfFieldEnd (fractions of the draw distance). The strength is the
; strongest blur radius in pixels at 1080p (scaled with the window height)
DepthOfField = 0
DepthOfFieldStart = 0.4
DepthOfFieldEnd = 1
DepthOfFieldStrength = 3

; Only used in 3dfx mode. Glow around light sources: lit windows, neon, lamps, car lights, fire and sparks
; (from a table of the textures' light texels, see tools/glow). Lights in the dark stay visible further
Glow = 0
GlowStrength = 0.7

; Only used in 3dfx mode. Night: everything is darkened to NightBrightness, except light sources (lit windows,
; neon, lamps... from the glow table), and the cars' headlights light the road and whatever is ahead of them.
; auto = on the tracks with the darkness depth cue (most of which have night skies), 1 = always, 0 = never.
; Burning cars light their surroundings too. HeadlightHaze: how much the headlight beams show in the air
; (more in fog). With Glow on, tail lights glow brighter while a car brakes, day and night
Night = auto
NightBrightness = 0.3
; NightHaze: on the tracks with darkness, the darkness becomes a haze (as Haze by day) in the sky's colour by the
; horizon (black in the mines), of this strength, that lights shine through, so more of the night is seen. Best
; with DrawEverything, as nothing then hides the far edge. 0 = the original fade into darkness
NightHaze = 0.5
HeadlightStrength = 2
HeadlightHaze = 1

; Only used in 3dfx mode. More post-processing:
; SSAO: ambient occlusion, soft contact shadows where surfaces meet (under cars, at the foot of walls).
;   SSAOStrength 0-1, above 1 (up to 4) deepens it further.
;   SSAORadius in world units: how far from a crease it reaches
; SpeedBlur: a radial blur towards the edges of the view, fading in from 70 to 150 mph
; LensFlare: light streaks through the glowing lights (needs Glow), strength
; HeatShimmer: hot air wobbling above burning cars
; ColourGrading: a gentle contrast curve and a little more saturation, cooler at night
SSAO = 0
SSAOStrength = 0.6
SSAORadius = 0.5
SpeedBlur = 0
LensFlare = 0
HeatShimmer = 0
ColourGrading = 0

; Only used in 3dfx mode. Break up the flat, tiled track and scenery:
; SurfaceVariation: a subtle large scale brightness variation (patches several metres across), 1 = about +-7%
; SunShading: surfaces turned away from a high sun darker (walls facing away up to about 25%), off at night
; Haze: on tracks without fog or darkness, the distance takes on a little of the sky's colour near the horizon
;   (aerial perspective, nothing blurs), 1 = about 30% by the original draw distance, at most 35%; off at night
SurfaceVariation = 0
SunShading = 0

; Only used in 3dfx mode. Everything casts shadows from the sun (the cars, bridges, buildings, cliffs and posts,
; whether in view or not), by three cascaded shadow maps of the world (by default 8192x8192 each, see
; ShadowMapSize: ~200m around the car at ~2.5cm per texel, ~830m at ~10cm, and the whole track), each taking over
; where the nearer one ends and its texels are about a screen pixel. At night they are fainter, as moonlight.
; The cars have these instead of their original shadows. Whatever is in shadow (the ground under a bridge, the
; sides turned away from the sun) is equally dark. The strength, 0 disables
SunShadows = 0

; Only used in 3dfx mode, with SunShadows. The size of each of the three shadow maps, in texels (a power of two,
; 1024 to 16384, at most what the GPU allows). The sharpness of the shadows (and their memory, 4 bytes a texel
; each) goes with it: 8192 is ~2.5cm per texel close up (768MB in all), 4096 ~5cm, 2048 ~10cm (48MB) for
; weaker GPUs
ShadowMapSize = 8192

; Only used in 3dfx mode. The sun catches the surfaces in a broad soft sheen (stronger at grazing angles, on the
; lighter parts of the textures), which slides over them as the view turns; not in the shadows (with
; SunShadows). Fainter at night. The strength, 0 disables
SunHighlights = 0

; Only used in 3dfx mode, with SunShading. The sun picks out the detail of the textures as relief (their
; brightness taken as height), by day and close up. The strength, 0 disables
Relief = 0
Haze = 0

; Only used in 3dfx mode. Soft round shadows on the ground under the pedestrians nearby
PedestrianShadows = 0

; Only used in 3dfx mode. Flashes: sparks light up their surroundings for a moment as they burst out, and so
; does a fire breaking out, day and night. The strength, 0 disables
Flashes = 0

; Only used in 3dfx mode, needs Glow. At night the ground looks wet: up-facing surfaces reflect the glowing
; lights (neon, lit windows, lamps, car lights), stretched towards the viewer. The strength, 0 disables
WetRoads = 0

; Only used in 3dfx mode, needs Glow. Ice (the surfaces with hardly any grip) mirrors the scenery, cars and sky
; around it, softly and more at grazing angles, day and night. The strength, 0 disables
IceReflections = 0

; Only used in 3dfx mode, needs Glow. Water (and the other liquids) mirrors the scenery, cars and sky with a
; gentle ripple, more at grazing angles. The strength, 0 disables
WaterReflections = 0

; Only used in 3dfx mode, needs a post-processing effect. Snow falls on the tracks with ice, and rain at night on
; the others. It stops under a roof (tunnels, the mines) once the car has been under it for 2 seconds, and starts
; again the same way. The amount, 0 disables
Weather = 0

; Only used in 3dfx mode. Cars get grimier and scratched as they are damaged (and clean again when repaired).
; The strength, 0 disables
CarWear = 0

; Only used in 3dfx mode. The map in a window of its own (titled "Dethrace map", eg. for a second screen): the
; race's map with the cars, checkpoint and pedestrians as map mode shows them, and the loading screen outside
; races. 1, 0, or auto (when there is more than one display)
MapWindow = 0

; Only used in 3dfx mode. The cars' wheels (low prisms in the original, eg. 8 sided) are made round: 32 sided,
; the same size and texture, the tread shaded smoothly. 0 = the original wheels
RoundWheels = 0

; Only used in 3dfx mode, needs Glow. Car paint gets a glossy coat: it reflects the sky and the scenery around
; it, mostly at grazing angles. The strength, 0 disables
CarReflections = 0

; Only used in 3dfx mode, needs a post-processing effect. Smoke, splashes and sparks fade out where they cut
; into the ground, walls and cars, instead of showing a hard line. The softness, 0 disables
SoftParticles = 0

; Only used in 3dfx mode. Widen (above 1) or narrow (below 1) the external camera view. 1.2 is about 10
; degrees wider. The cockpit view is unchanged
FOVScale = 1

; Only used in the external view. The camera moves towards where it wants to be, and aims towards the car, with
; this time constant (seconds), so bumps, knocks and spins don't jerk the view; it still jumps clear of walls
; and other cars. 0 = rigidly, as the original. 0.08 is quick but smooth
CameraSmoothing = 0

; Only used in 3dfx mode. The external camera jolts for a moment when the car is hit or hits something, and the
; view widens a little at speed. The strength, 0 disables
CameraKick = 0

; Only used in 3dfx mode. 1 = vsync on, 0 = off, adaptive = vsync, but a frame that is late is shown
; immediately (may tear briefly) instead of waiting a whole refresh. Variable refresh rate displays work with 1
VSync = 1

; Open the in-race menu (pausing the game) when the window loses focus, eg. on alt-tab
PauseOnFocusLoss = 1

; Game controller rumble on impacts, pedestrian hits and wrecking opponents
GamepadRumble = 1

; Game controller tuning. Stick and trigger deadzones (0-0.9)
GamepadStickDeadzone = 0.15
GamepadTriggerDeadzone = 0.05
; Steering response curve: steering = stick ^ curve. 1 is linear, higher is gentler near the centre
GamepadSteeringCurve = 2.5
; Speed sensitive steering: half the steering lock is available at this speed (mph). 0 disables
GamepadSteeringSpeed = 60

; Multiply the draw distance chosen in the game's options. Fog and darkness move out with it
DrawDistance = 1

; Draw everything, however far (the whole track): no far edge for things to pop in at (on the tracks without
; fog, and at night with NightHaze, nothing else hides it). The fog, and the ranges of
; pedestrians, smoke and shadows, still go by the draw distance (and DrawDistance)
DrawEverything = 0

; Always draw the track up to the full draw distance (whatever the "track detail" option says),
; and always use the most detailed car models
MaxDetail = 0

; Only used in 3dfx mode. Multisample anti-aliasing sample count (0 to disable, typically 2, 4 or 8)
MSAA = 4

; Only used in 3dfx mode. Magnify textures 2x at load time with a pixel-art upscaler: "none" (disabled),
; "mmpx2x" (smooths diagonals and curves using only each texture's own colours)
; or "xbr2x" (smoother, blended edges)
TextureUpscaler = none

; Only used in 3dfx mode, needs MSAA. Shade every MSAA sample (supersampling): also smooths texture
; detail and cut-out edges, at the cost of MSAA-times more pixel shading
Supersampling = 0

; Censored zombie/robots mode
BoringMode = 0

; Play cut scenes on startup and between races
Cutscenes = 0

; "hires" mode is 640x480, otherwise default 320x200
Hires = 1

; Only used in 'demo' mode. Default demo time out is 240s (4 mins)
DemoTimeout = 240

; Which directory in the [Games] section to run
DefaultGame = c1

; Original physics updates every 40ms, causing noticable camera juddering when cornering at slow speed
; Enabling this runs physics every frame, smoothing movement out
PhysicsPerFrame = 1

[Games]
c1 = /opt/carma/c1
c1demo = /opt/carma/c1demo
sp = /opt/carma/splatpack

[Cheats]
EditMode = 0
FreezeTimer = 0
GameCompleted = 0

[Sound]
Enabled = 1
SoundOptionsScreen = 1
VolumeMultiplier = 1

[Network]
AdapterName = ""
```

## Order of precedence for game directory detection:
1. Directory pointed to by DefaultGame
2. First game in the list if at least 1 game dir is specified
3. `--dir` command line argument
4. Current working directory (if `DATA/GENERAL.TXT` exists)
5. `SDL_GetPrefPath` directory

## Gamepad

Xbox-style controllers (and anything SDL maps as a game controller) are supported. The first connected controller is used.

| Control | Driving | Menus | Action replay |
|---|---|---|---|
| Left stick | Steer (analog) | Navigate | |
| Right trigger | Accelerate (analog) | | Fast forward |
| Left trigger | Brake / reverse (analog) | | Rewind |
| A | Handbrake | Select | Play / pause |
| B | Repair (hold) | Back | Return to race |
| X | Recover car | Delete character | Reverse direction |
| Y | Cockpit / external view | | Change camera |
| LB | Action replay | | Return to race |
| RB | Horn | | |
| View | Map | | |
| Menu | Pause menu | Back to race (in race) / select | Menu |
| Left stick click | Toggle Prat-cam | | |
| Right stick click | Toggle rear view mirror | | |
| Right stick left/right | Look left/right (hold) | | |
| D-pad | Move external camera | Navigate | Step frame (left/right), start/end (up/down) |

While driving, buttons trigger whatever key is bound to the action, so they keep working if the keyboard controls are remapped.
